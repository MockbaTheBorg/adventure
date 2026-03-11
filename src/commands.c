#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "commands.h"
#include "combat.h"
#include "loader.h"
#include "input.h"
#include "save.h"
#include "messages.h"
#include "endgame.h"

/* -------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

/*
 * exit_conditions_met - Check whether a conditional exit's requirements are
 * all satisfied.  Returns 1 if the exit is passable, 0 otherwise.
 * An exit with no conditions always returns 1.
 */
static int exit_conditions_met(const GameState *gs, const Exit *ex)
{
    if (ex->require_flag && !flag_check(gs, ex->require_flag))
        return 0;
    if (ex->require_item) {
        int i, found = 0;
        for (i = 0; i < gs->inventory.count; i++) {
            if (strcmp(gs->inventory.items[i]->id, ex->require_item) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) return 0;
    }
    if (ex->require_npc_dead) {
        NPC *n = find_npc((GameState *)gs, ex->require_npc_dead);
        if (!n || n->alive) return 0;
    }
    return 1;
}

/*
 * exit_visible - Returns 1 if the exit should be displayed to the player.
 * Hidden exits are invisible until their conditions are met.
 */
static int exit_visible(const GameState *gs, const Exit *ex)
{
    if (!ex->room_id) return 0;
    if (ex->hidden && !exit_conditions_met(gs, ex)) return 0;
    return 1;
}

/*
 * print_room - Display current room: name, HP bar, equipped gear, description,
 * exits, objects, NPCs.  Clears the screen first.
 * No first_visit_message, no cutscene logic.
 * Declared non-static (see commands.h) so cmd_help can call it externally.
 */
void print_room(const GameState *gs)
{
    const Room *r = gs->current_room;
    int d, i, alive_count;

    ansi_clear_screen();
    printf("\n");
    ansi_bold(); ansi_color(COLOR_CYAN);
    printf("[ %s ]\n", r->name);
    ansi_reset();

    /* HP bar */
    combat_hp_bar("HP", gs->player_hp, 100, 20);

    /* Equipped gear line */
    if (gs->equipped_weapon || gs->equipped_shield) {
        if (gs->equipped_weapon) {
            printf("[Weapon: %s", DNAME(gs->equipped_weapon));
            if (gs->equipped_weapon->max_durability > 0)
                printf(" %d%%",
                       (gs->equipped_weapon->durability * 100)
                       / gs->equipped_weapon->max_durability);
            printf("]");
        }
        if (gs->equipped_shield) {
            if (gs->equipped_weapon) printf(" ");
            printf("[Shield: %s", DNAME(gs->equipped_shield));
            if (gs->equipped_shield->max_durability > 0)
                printf(" %d%%",
                       (gs->equipped_shield->durability * 100)
                       / gs->equipped_shield->max_durability);
            printf("]");
        }
        printf("\n");
    }

    if (r->description && r->description[0] != '\0') {
        ansi_print(r->description);
        printf("\n");
    }

    /* Exits summary */
    printf("\n");
    ansi_color(COLOR_YELLOW);
    printf("Exits:");
    ansi_reset();
    for (d = 0; d < DIR_COUNT; d++) {
        const Exit *ex = &r->exits[d];
        if (!exit_visible(gs, ex)) continue;
        printf(" [%c]%s", dir_key[d], dir_name[d] + 1);
        if (!exit_conditions_met(gs, ex)) {
            printf("(blocked)");
        } else if (ex->door_id) {
            Door *door = find_door((GameState *)gs, ex->door_id);
            if (door && !door->destroyed && door->locked)
                printf("(locked)");
        }
    }
    printf("\n");

    /* Objects on the floor */
    if (r->object_count > 0) {
        printf("\n");
        ansi_color(COLOR_GREEN);
        printf("You can see:");
        ansi_reset();
        for (i = 0; i < r->object_count; i++) {
            if (i > 0) printf(",");
            printf(" %s", DNAME(r->objects[i]));
        }
        printf("\n");
    }

    /* NPCs (alive only; track hostile presence) */
    alive_count = 0;
    for (i = 0; i < r->npc_count; i++)
        if (r->npcs[i]->alive) alive_count++;

    if (alive_count > 0) {
        int first        = 1;
        int hostile_here = 0;
        printf("\n");
        ansi_color(COLOR_CYAN);
        printf("Also here:");
        ansi_reset();
        for (i = 0; i < r->npc_count; i++) {
            if (!r->npcs[i]->alive) continue;
            if (!first) printf(",");
            printf(" %s", DNAME(r->npcs[i]));
            if (r->npcs[i]->hostile) hostile_here = 1;
            first = 0;
        }
        if (hostile_here && gs->pending_hostile) {
            ansi_color(COLOR_RED);
            printf("  [hostile]");
            ansi_reset();
        }
        printf("\n");
    }
}

/*
 * cancel_command - Called when ESC is pressed during any sub-prompt.
 */
static void cancel_command(GameState *gs)
{
    print_room(gs);
}

/*
 * inspect_object - Print the name and description of a single object.
 */
static void inspect_object(const Object *o)
{
    ansi_bold();
    printf("%s\n", DNAME(o));
    ansi_reset();
    ansi_print(o->description ? o->description : MSG_INSPECT_NOTHING);
    printf("\n");
    if (o->max_durability > 0) {
        combat_hp_bar("Durability",
                      o->durability, o->max_durability, 20);
    }
    if (o->is_weapon)
        printf("Weapon  damage: +%d\n", o->damage);
    if (o->is_shield)
        printf("Shield  defense: %d%%\n", o->defense);
    if (o->heal > 0)
        printf("Heals: %d HP\n", o->heal);
    if (o->openable && !o->opened && o->open_key_id)
        printf("Status: locked\n");
}

/*
 * inspect_door - Print name, description, and status of a door.
 */
static void inspect_door(const Door *d)
{
    const char *status;
    ansi_bold();
    printf("%s\n", DNAME(d));
    ansi_reset();
    ansi_print(d->description ? d->description : MSG_INSPECT_NOTHING);
    printf("\n");
    if (d->destroyed)   status = "destroyed";
    else if (d->locked) status = "locked";
    else                status = "open";
    printf("Status: %s\n", status);
}

/*
 * inspect_npc - Print name, HP, and description of an NPC.
 */
static void inspect_npc(const NPC *n)
{
    ansi_bold();
    printf("%s\n", DNAME(n));
    ansi_reset();
    if (!n->alive) {
        ansi_color(COLOR_RED);
        printf("[defeated]\n");
        ansi_reset();
        return;
    }
    if (n->max_hp > 0)
        combat_hp_bar("HP", n->hp, n->max_hp, 20);
    ansi_print(n->description ? n->description : MSG_INSPECT_NOTHING);
    printf("\n");
}

/*
 * set_save_path / prompt_save_path
 */
static void set_save_path(GameState *gs, const char *path)
{
    free(gs->save_path);
    gs->save_path = (char *)malloc(strlen(path) + 1);
    if (gs->save_path) strcpy(gs->save_path, path);
}

static int prompt_save_path(GameState *gs)
{
    char filename[256];
    if (gs->save_path) return 1;

    printf("Save to file: ");
    fflush(stdout);
    term_readline(filename, sizeof(filename));

    if (filename[0] == '\0') {
        printf("Cancelled.\n");
        return 0;
    }
    set_save_path(gs, filename);
    return 1;
}

/* -------------------------------------------------------------------------
 * room_enter
 * ------------------------------------------------------------------------- */
void room_enter(GameState *gs, Room *dest)
{
    int first_visit;
    int i, has_hostile;

    gs->current_room = dest;
    first_visit      = !dest->visited;
    dest->visited    = 1;

    /* Random NPC spawns for this room */
    spawn_room_npcs(gs, dest);

    /* Determine if any hostile NPC is alive here */
    has_hostile = 0;
    for (i = 0; i < dest->npc_count; i++) {
        if (dest->npcs[i]->hostile && dest->npcs[i]->alive) {
            has_hostile = 1;
            break;
        }
    }
    gs->pending_hostile = has_hostile;

    if (dest->cutscene) {
        /* cutscene room: narrative passage, no game menu */
        Room *next;
        ansi_clear_screen();
        printf("\n");
        if (dest->description && dest->description[0] != '\0') {
            ansi_print(dest->description);
            printf("\n");
        }

        if (first_visit && dest->first_visit_message
                        && dest->first_visit_message[0] != '\0') {
            printf("\n");
            ansi_color(COLOR_YELLOW);
            ansi_print(dest->first_visit_message);
            printf("\n");
            ansi_reset();
        }

        printf("\n");
        ansi_color(COLOR_YELLOW);
        printf("Press any key to continue...");
        ansi_reset();
        fflush(stdout);
        term_getkey();
        printf("\n");

        next = find_room(gs, gs->current_room->cutscene);
        if (!next) {
            fprintf(stderr, "error: cutscene destination '%s' not found\n",
                    gs->current_room->cutscene);
            return;
        }
        room_enter(gs, next);

    } else {
        /* normal room */
        print_room(gs);

        if (first_visit && dest->first_visit_message
                        && dest->first_visit_message[0] != '\0') {
            printf("\n");
            ansi_color(COLOR_YELLOW);
            ansi_print(dest->first_visit_message);
            printf("\n");
            ansi_reset();
        }

        /* Hostile warning after room is shown */
        if (has_hostile) {
            printf("\n");
            ansi_color(COLOR_RED);
            for (i = 0; i < dest->npc_count; i++) {
                if (dest->npcs[i]->hostile && dest->npcs[i]->alive)
                    printf("%s eyes you with hostility.\n",
                           DNAME(dest->npcs[i]));
            }
            ansi_reset();
        }

        check_room_end(gs);
    }
}

/*
 * do_go - Move in direction dir.  Shared by cmd_go.
 */
static void do_go(GameState *gs, int dir)
{
    Room *r = gs->current_room;
    Exit *ex;
    Door *door;
    Room *dest;

    if (dir == INPUT_ESC) { cancel_command(gs); return; }

    if (dir < 0) {
        printf("%s\n", MSG_NO_EXIT);
        return;
    }

    ex = &r->exits[dir];
    if (!ex->room_id || (ex->hidden && !exit_conditions_met(gs, ex))) {
        printf("%s\n", MSG_NO_EXIT);
        return;
    }

    /* Check conditional exit requirements */
    if (!exit_conditions_met(gs, ex)) {
        const char *msg = ex->blocked_message
                        ? ex->blocked_message : MSG_EXIT_BLOCKED;
        ansi_print(msg);
        printf("\n");
        return;
    }

    if (ex->door_id) {
        door = find_door(gs, ex->door_id);
        if (door && door->locked && !door->destroyed) {
            const char *msg = door->message ? door->message : MSG_DOOR_LOCKED;
            ansi_print(msg);
            printf("\n");
            return;
        }
    }

    dest = find_room(gs, ex->room_id);
    if (!dest) {
        fprintf(stderr, "error: destination room '%s' not found\n",
                ex->room_id);
        return;
    }

    room_enter(gs, dest);
}

/* -------------------------------------------------------------------------
 * Command implementations
 * ------------------------------------------------------------------------- */

/*
 * build_dir_mask - Build a bitmask of visible directions for the current room.
 */
static int build_dir_mask(const GameState *gs)
{
    int d, mask = 0;
    for (d = 0; d < DIR_COUNT; d++)
        if (exit_visible(gs, &gs->current_room->exits[d]))
            mask |= (1 << d);
    return mask;
}

void cmd_go(GameState *gs)
{
    int dir = input_direction("Go where?", gs->current_room, build_dir_mask(gs));
    do_go(gs, dir);
}

void cmd_look(GameState *gs)
{
    int dir = input_direction("Look where?", gs->current_room, build_dir_mask(gs));
    const Exit *ex;

    if (dir == INPUT_ESC) { cancel_command(gs); return; }

    if (dir < 0) {
        printf("%s\n", MSG_LOOK_NOTHING);
        return;
    }

    ex = &gs->current_room->exits[dir];

    if (!exit_visible(gs, ex)) {
        printf("%s\n", MSG_LOOK_NOTHING);
        return;
    }

    if (ex->look_description && ex->look_description[0] != '\0') {
        ansi_print(ex->look_description);
        printf("\n");
    } else
        printf("%s\n", MSG_LOOK_NOTHING);

    if (ex->door_id) {
        Door *door = find_door(gs, ex->door_id);
        if (door)
            printf("%s is %s.\n",
                   DNAME(door),
                   door->locked ? "locked" : "open");
    }
}

/*
 * cmd_inspect - Inspect objects, doors, or NPCs in the room, or inventory.
 */
void cmd_inspect(GameState *gs)
{
#define ITYPE_OBJ  0
#define ITYPE_DOOR 1
#define ITYPE_NPC  2

    Room       *r = gs->current_room;
    void       *items[25];
    int         itypes[25];
    const char *inames[25];
    int         count, i, idx, d_idx, seen, already;
    char        c;

    count = 0;

    /* Room objects */
    for (i = 0; i < r->object_count && count < 25; i++) {
        items[count]  = r->objects[i];
        itypes[count] = ITYPE_OBJ;
        inames[count] = DNAME(r->objects[i]);
        count++;
    }

    /* Doors from exits (skip destroyed; deduplicate) */
    for (d_idx = 0; d_idx < DIR_COUNT && count < 25; d_idx++) {
        const Exit *ex = &r->exits[d_idx];
        Door       *door;
        if (!ex->door_id) continue;
        door = find_door(gs, ex->door_id);
        if (!door) continue;
        already = 0;
        for (seen = 0; seen < count; seen++) {
            if (itypes[seen] == ITYPE_DOOR && items[seen] == (void *)door) {
                already = 1;
                break;
            }
        }
        if (already) continue;
        items[count]  = door;
        itypes[count] = ITYPE_DOOR;
        inames[count] = DNAME(door);
        count++;
    }

    /* All NPCs in the room (alive and defeated) */
    for (i = 0; i < r->npc_count && count < 25; i++) {
        items[count]  = r->npcs[i];
        itypes[count] = ITYPE_NPC;
        inames[count] = DNAME(r->npcs[i]);
        count++;
    }

    printf("\n");
    ansi_bold();
    printf("Inspect what?\n");
    ansi_reset();

    ansi_color(COLOR_YELLOW);
    printf("  [a] Inventory\n");
    ansi_reset();

    for (i = 0; i < count; i++) {
        if (itypes[i] == ITYPE_NPC) {
            NPC *npc = (NPC *)items[i];
            if (npc->alive) ansi_color(COLOR_CYAN);
            else            ansi_color(COLOR_RED);
        } else if (itypes[i] == ITYPE_DOOR) {
            ansi_color(COLOR_YELLOW);
        }
        printf("  [%c] %s", 'b' + i, inames[i]);
        ansi_reset();
        if (itypes[i] == ITYPE_DOOR) {
            Door *door = (Door *)items[i];
            if (door->destroyed)   printf(" [destroyed]");
            else if (door->locked) printf(" [locked]");
            else                   printf(" [open]");
        } else if (itypes[i] == ITYPE_NPC) {
            NPC *npc = (NPC *)items[i];
            if (!npc->alive) printf(" [defeated]");
        }
        printf("\n");
    }
    printf("  [0] Cancel\n> ");
    fflush(stdout);

    c = term_getkey();
    if (c == 27) { cancel_command(gs); return; }

    c = TO_LOWER(c);
    printf("%c\n", c);

    if (c == '0') return;

    if (c == 'a') {
        idx = input_pick_object(gs->inventory.items, gs->inventory.count,
                                "Inspect which item?", MSG_INV_EMPTY);
        if (idx == INPUT_ESC) { cancel_command(gs); return; }
        if (idx < 0) return;
        inspect_object(gs->inventory.items[idx]);
        return;
    }

    idx = c - 'b';
    if (idx < 0 || idx >= count) return;

    if      (itypes[idx] == ITYPE_OBJ)  inspect_object((Object *)items[idx]);
    else if (itypes[idx] == ITYPE_DOOR) inspect_door  ((Door   *)items[idx]);
    else                                inspect_npc   ((NPC    *)items[idx]);

#undef ITYPE_OBJ
#undef ITYPE_DOOR
#undef ITYPE_NPC
}

void cmd_pickup(GameState *gs)
{
    Room   *r = gs->current_room;
    int     idx;
    Object *o;

    idx = input_pick_object(r->objects, r->object_count,
                            "Pick up what?", MSG_ROOM_EMPTY);
    if (idx == INPUT_ESC) { cancel_command(gs); return; }
    if (idx < 0) return;

    o = r->objects[idx];

    if (!o->pickupable) {
        printf("%s\n", o->message ? o->message : MSG_PICKUP_CANT);
        return;
    }

    if (gs->inventory.count >= INV_MAX_OBJECTS) {
        printf("Your hands are full.\n");
        return;
    }

    r->objects[idx] = r->objects[--r->object_count];
    gs->inventory.items[gs->inventory.count++] = o;
    printf("%s\n", MSG_PICKUP_OK);
    apply_flags(gs, &o->on_pickup_flags);
    check_obj_end(gs, o, o->on_pickup_end);
    check_inv_win(gs);
}

void cmd_drop(GameState *gs)
{
    Room   *r = gs->current_room;
    int     idx;
    Object *o;

    idx = input_pick_object(gs->inventory.items, gs->inventory.count,
                            "Drop what?", MSG_INV_EMPTY);
    if (idx == INPUT_ESC) { cancel_command(gs); return; }
    if (idx < 0) return;

    if (r->object_count >= ROOM_MAX_OBJECTS) {
        printf("There's no room to put that here.\n");
        return;
    }

    o = gs->inventory.items[idx];
    gs->inventory.items[idx] = gs->inventory.items[--gs->inventory.count];
    r->objects[r->object_count++] = o;
    printf("%s\n", MSG_DROP_OK);
}

void cmd_use(GameState *gs)
{
    int     idx;
    Object *o;
    Door   *target_door;
    int     d;

    idx = input_pick_object(gs->inventory.items, gs->inventory.count,
                            "Use what?", MSG_INV_EMPTY);
    if (idx == INPUT_ESC) { cancel_command(gs); return; }
    if (idx < 0) return;

    o = gs->inventory.items[idx];

    /* Healing items: no use_target needed */
    if (o->heal > 0 && !o->use_target) {
        if (gs->player_hp >= 100) {
            printf("%s\n", MSG_HEAL_FULL);
            return;
        }
        gs->player_hp += o->heal;
        if (gs->player_hp > 100) gs->player_hp = 100;
        printf("%s (You: %d/100 HP)\n", MSG_HEAL_OK, gs->player_hp);
        if (o->single_use)
            gs->inventory.items[idx] =
                gs->inventory.items[--gs->inventory.count];
        apply_flags(gs, &o->on_use_flags);
        return;
    }

    if (!o->use_target) {
        printf("%s\n", o->message ? o->message : MSG_USE_NOTHING);
        return;
    }

    target_door = NULL;
    for (d = 0; d < DIR_COUNT; d++) {
        const Exit *ex = &gs->current_room->exits[d];
        if (ex->door_id && strcmp(ex->door_id, o->use_target) == 0) {
            target_door = find_door(gs, ex->door_id);
            break;
        }
    }

    if (!target_door) {
        printf("%s\n", o->message ? o->message : MSG_USE_NO_TARGET);
        return;
    }

    /* Check destruction */
    if (target_door->break_key_id &&
        strcmp(target_door->break_key_id, o->id) == 0) {
        const char *bmsg;
        if (target_door->destroyed) {
            printf("%s\n", MSG_DOOR_ALREADY_BROKEN);
            return;
        }
        target_door->destroyed = 1;
        target_door->locked    = 0;
        bmsg = target_door->break_message
               ? target_door->break_message : MSG_DOOR_DESTROYED;
        ansi_print(bmsg);
        printf("\n");
        if (o->single_use)
            gs->inventory.items[idx] =
                gs->inventory.items[--gs->inventory.count];
        apply_flags(gs, &o->on_use_flags);
        check_obj_end(gs, o, o->on_use_end);
        return;
    }

    /* Check unlock */
    if (target_door->destroyed || !target_door->locked) {
        printf("%s\n", MSG_DOOR_ALREADY_OPEN);
        return;
    }

    if (!target_door->key_id ||
        strcmp(target_door->key_id, o->id) != 0) {
        printf("%s\n", o->message ? o->message : MSG_USE_NOTHING);
        return;
    }

    target_door->locked = 0;
    printf("%s\n", MSG_DOOR_UNLOCKED);

    if (o->single_use)
        gs->inventory.items[idx] = gs->inventory.items[--gs->inventory.count];

    apply_flags(gs, &o->on_use_flags);
    check_obj_end(gs, o, o->on_use_end);
}

void cmd_open(GameState *gs)
{
    Room   *r = gs->current_room;
    Object *openable[ROOM_MAX_OBJECTS];
    int     openable_count = 0;
    int     i, idx;
    Object *o;

    for (i = 0; i < r->object_count; i++)
        if (r->objects[i]->openable)
            openable[openable_count++] = r->objects[i];

    idx = input_pick_object(openable, openable_count,
                            "Open what?", MSG_OPEN_NOTHING);
    if (idx == INPUT_ESC) { cancel_command(gs); return; }
    if (idx < 0) return;

    o = openable[idx];

    if (o->opened) {
        printf("%s\n", o->message ? o->message : MSG_ALREADY_OPENED);
        return;
    }

    /* Locked container: check for key in inventory */
    if (o->open_key_id) {
        int k, found_key;
        found_key = -1;
        for (k = 0; k < gs->inventory.count; k++) {
            if (strcmp(gs->inventory.items[k]->id, o->open_key_id) == 0) {
                found_key = k;
                break;
            }
        }
        if (found_key < 0) {
            printf("%s\n", o->message ? o->message : MSG_OPEN_LOCKED);
            return;
        }
        printf("You use the %s.\n", DNAME(gs->inventory.items[found_key]));
        if (gs->inventory.items[found_key]->single_use)
            gs->inventory.items[found_key] =
                gs->inventory.items[--gs->inventory.count];
    }

    o->opened = 1;
    printf("%s\n", MSG_OPEN_OK);

    for (i = 0; i < o->contains_count; i++) {
        Object *contained = find_object(gs, o->contains[i]);
        if (!contained) {
            fprintf(stderr, "warning: contained object '%s' not found\n",
                    o->contains[i]);
            continue;
        }
        if (r->object_count < ROOM_MAX_OBJECTS) {
            r->objects[r->object_count++] = contained;
            printf("  You find: %s\n",
                   DNAME(contained));
        }
    }
}

void cmd_talk(GameState *gs)
{
    Room       *r = gs->current_room;
    NPC        *n;
    Object     *gift_tmpl;
    Object     *gift;
    const char *dialogue;
    int         status;

    n = input_pick_npc(r, "Talk to whom?", MSG_NO_NPC, &status);
    if (status == INPUT_ESC) { cancel_command(gs); return; }
    if (!n) return;

    ansi_bold();
    printf("%s:\n", DNAME(n));
    ansi_reset();

    if (!n->talked) {
        dialogue = n->dialogue ? n->dialogue : MSG_NPC_NOTHING;
        ansi_print(dialogue);
        printf("\n");
        n->talked = 1;

        /* Give object on first talk (handle templates) */
        if (n->gives) {
            gift_tmpl = find_object(gs, n->gives);
            if (gift_tmpl) {
                if (gift_tmpl->is_template) {
                    gift = create_template_instance(gs, n->gives);
                } else {
                    gift = gift_tmpl;
                }
                if (gift) {
                    if (gs->inventory.count < INV_MAX_OBJECTS) {
                        gs->inventory.items[gs->inventory.count++] = gift;
                        printf("%s hands you the %s.\n",
                               n->name ? n->name : "They",
                               DNAME(gift));
                    } else {
                        if (r->object_count < ROOM_MAX_OBJECTS)
                            r->objects[r->object_count++] = gift;
                        printf("%s sets the %s on the floor.\n",
                               n->name ? n->name : "They",
                               DNAME(gift));
                    }
                    check_inv_win(gs);
                }
            }
        }
        apply_flags(gs, &n->on_talk_flags);
    } else {
        dialogue = n->after_dialogue ? n->after_dialogue
                 : n->dialogue       ? n->dialogue
                 : MSG_NPC_NOTHING;
        ansi_print(dialogue);
        printf("\n");
    }

    /* Healer: restore HP (on any talk) */
    if (n->healer && gs->player_hp < 100) {
        gs->player_hp += n->heal_amount;
        if (gs->player_hp > 100) gs->player_hp = 100;
        printf("%s heals you for %d HP. (You: %d/100 HP)\n",
               n->name ? n->name : "They",
               n->heal_amount, gs->player_hp);
    }
}

/* -------------------------------------------------------------------------
 * cmd_equip - Equip or unequip a weapon/shield from inventory.
 * ------------------------------------------------------------------------- */
void cmd_equip(GameState *gs)
{
    Object *equippable[INV_MAX_OBJECTS];
    int     eq_count;
    int     i, idx;
    Object *o;
    char    c;

    eq_count = 0;
    for (i = 0; i < gs->inventory.count; i++) {
        Object *item = gs->inventory.items[i];
        if (item->is_weapon || item->is_shield)
            equippable[eq_count++] = item;
    }

    if (eq_count == 0) {
        printf("%s\n", MSG_EQUIP_NOTHING);
        return;
    }

    printf("\n");
    ansi_bold();
    printf("Equip what?\n");
    ansi_reset();

    for (i = 0; i < eq_count; i++) {
        int is_eq;
        o    = equippable[i];
        is_eq = (o == gs->equipped_weapon || o == gs->equipped_shield);
        printf("  [%c] %s [%s]",
               'a' + i,
               DNAME(o),
               o->is_weapon ? "weapon" : "shield");
        if (is_eq) {
            ansi_color(COLOR_GREEN);
            printf(" (equipped)");
            ansi_reset();
        }
        if (o->max_durability > 0)
            printf(" %d%%",
                   (o->durability * 100) / o->max_durability);
        printf("\n");
    }
    printf("  [0] Cancel\n> ");
    fflush(stdout);

    c = term_getkey();
    if (c == 27) { cancel_command(gs); return; }
    c = TO_LOWER(c);
    printf("%c\n", c);
    if (c == '0') return;
    if (c < 'a' || c > 'z') return;
    idx = c - 'a';
    if (idx >= eq_count) return;

    o = equippable[idx];

    if (o->is_weapon) {
        if (gs->equipped_weapon == o) {
            gs->equipped_weapon = NULL;
            printf("%s\n", MSG_UNEQUIP_OK);
        } else {
            gs->equipped_weapon = o;
            printf("%s\n", MSG_EQUIP_WEAPON_OK);
        }
    } else {
        if (gs->equipped_shield == o) {
            gs->equipped_shield = NULL;
            printf("%s\n", MSG_UNEQUIP_OK);
        } else {
            gs->equipped_shield = o;
            printf("%s\n", MSG_EQUIP_SHIELD_OK);
        }
    }
}

/* -------------------------------------------------------------------------
 * cmd_attack - Attack an NPC in the current room.
 * ------------------------------------------------------------------------- */
void cmd_attack(GameState *gs)
{
    NPC *n;
    int  status;

    n = input_pick_npc(gs->current_room, "Attack whom?", MSG_ATTACK_NO_NPC,
                       &status);
    if (status == INPUT_ESC) { cancel_command(gs); return; }
    if (!n) return;

    combat_attack(gs, n);
}

/* -------------------------------------------------------------------------
 * cmd_repair - Repair equipped gear via NPC repairer or repair kit.
 * ------------------------------------------------------------------------- */
void cmd_repair(GameState *gs)
{
    Room   *r = gs->current_room;
    Object *gear[2];
    int     gear_count;
    int     i, repaired, idx;
    NPC    *repairer;
    Object *repair_kit;
    int     kit_idx;
    Object *target;
    char    c;

    gear_count = 0;
    repairer   = NULL;
    repair_kit = NULL;
    kit_idx    = -1;
    target     = NULL;

    /* Look for a repairer NPC in the room */
    for (i = 0; i < r->npc_count; i++) {
        if (r->npcs[i]->repairer && r->npcs[i]->alive) {
            repairer = r->npcs[i];
            break;
        }
    }

    /* If no repairer, look for a repair kit in inventory */
    if (!repairer) {
        for (i = 0; i < gs->inventory.count; i++) {
            if (gs->inventory.items[i]->repair_amount > 0) {
                repair_kit = gs->inventory.items[i];
                kit_idx    = i;
                break;
            }
        }
    }

    if (!repairer && !repair_kit) {
        printf("%s\n", MSG_REPAIR_NOTHING);
        return;
    }

    if (repairer) {
        /* Repair repairer repairs all equipped items to max */
        repaired = 0;
        if (gs->equipped_weapon
            && gs->equipped_weapon->max_durability > 0
            && gs->equipped_weapon->durability
               < gs->equipped_weapon->max_durability) {
            gs->equipped_weapon->durability =
                gs->equipped_weapon->max_durability;
            printf("%s repairs your %s.\n",
                   repairer->name ? repairer->name : "They",
                   gs->equipped_weapon->name
                   ? gs->equipped_weapon->name : "weapon");
            repaired = 1;
        }
        if (gs->equipped_shield
            && gs->equipped_shield->max_durability > 0
            && gs->equipped_shield->durability
               < gs->equipped_shield->max_durability) {
            gs->equipped_shield->durability =
                gs->equipped_shield->max_durability;
            printf("%s repairs your %s.\n",
                   repairer->name ? repairer->name : "They",
                   gs->equipped_shield->name
                   ? gs->equipped_shield->name : "shield");
            repaired = 1;
        }
        if (!repaired)
            printf("%s\n", MSG_REPAIR_FULL);
        return;
    }

    /* Repair kit path: find damaged equipped gear */
    if (gs->equipped_weapon
        && gs->equipped_weapon->max_durability > 0
        && gs->equipped_weapon->durability
           < gs->equipped_weapon->max_durability)
        gear[gear_count++] = gs->equipped_weapon;
    if (gs->equipped_shield
        && gs->equipped_shield->max_durability > 0
        && gs->equipped_shield->durability
           < gs->equipped_shield->max_durability)
        gear[gear_count++] = gs->equipped_shield;

    if (gear_count == 0) {
        printf("%s\n", MSG_REPAIR_FULL);
        return;
    }

    if (gear_count == 1) {
        target = gear[0];
    } else {
        printf("\n");
        ansi_bold();
        printf("Repair which item?\n");
        ansi_reset();
        for (i = 0; i < gear_count; i++) {
            printf("  [%c] %s (%d%%)\n",
                   'a' + i,
                   DNAME(gear[i]),
                   (gear[i]->max_durability > 0)
                   ? (gear[i]->durability * 100)
                     / gear[i]->max_durability : 0);
        }
        printf("  [0] Cancel\n> ");
        fflush(stdout);

        c = term_getkey();
        if (c == 27) { cancel_command(gs); return; }
        c = TO_LOWER(c);
        printf("%c\n", c);
        if (c == '0') return;
        if (c < 'a' || c > 'z') return;
        idx = c - 'a';
        if (idx >= gear_count) return;
        target = gear[idx];
    }

    target->durability += repair_kit->repair_amount;
    if (target->durability > target->max_durability)
        target->durability = target->max_durability;
    printf("%s Your %s is now at %d%%.\n",
           MSG_REPAIR_OK,
           target->name ? target->name : "item",
           (target->durability * 100) / target->max_durability);
    if (repair_kit->single_use)
        gs->inventory.items[kit_idx] =
            gs->inventory.items[--gs->inventory.count];
}

void cmd_save(GameState *gs)
{
    if (!prompt_save_path(gs)) return;
    game_save(gs, gs->save_path);
}

void cmd_help(GameState *gs)
{
    print_room(gs);
}

/*
 * cmd_help_text - [?] Contextual command reference.
 */
void cmd_help_text(GameState *gs)
{
    const Room    *r = gs->current_room;
    const Command *cmd;
    int i, has_pickupable, has_openable, has_usable;
    int has_alive_npc, has_repairer, has_repair_kit;

    has_pickupable = 0;
    has_openable   = 0;
    has_usable     = (gs->inventory.count > 0);
    has_alive_npc  = 0;
    has_repairer   = 0;
    has_repair_kit = 0;

    for (i = 0; i < r->object_count; i++) {
        if (r->objects[i]->pickupable) has_pickupable = 1;
        if (r->objects[i]->openable)   has_openable   = 1;
    }
    for (i = 0; i < r->npc_count; i++) {
        if (r->npcs[i]->alive) {
            has_alive_npc = 1;
            if (r->npcs[i]->repairer) has_repairer = 1;
        }
    }
    for (i = 0; i < gs->inventory.count; i++) {
        if (gs->inventory.items[i]->repair_amount > 0)
            has_repair_kit = 1;
    }

    ansi_clear_screen();
    printf("\n");
    ansi_bold(); ansi_color(COLOR_CYAN);
    printf("Available commands:\n\n");
    ansi_reset();

    for (cmd = command_table; cmd->key != 0; cmd++) {
        if (cmd->key == 't' && !has_alive_npc)               continue;
        if (cmd->key == 'p' && !has_pickupable)               continue;
        if (cmd->key == 'o' && !has_openable)                 continue;
        if (cmd->key == 'd' && !has_usable)                   continue;
        if (cmd->key == 'u' && !has_usable)                   continue;
        if (cmd->key == 'a' && !has_alive_npc)                continue;
        if (cmd->key == 'r' && !has_repairer && !has_repair_kit) continue;

        ansi_bold();
        printf("  [%c] %-10s",
               TO_UPPER(cmd->key),
               cmd->label);
        ansi_reset();
        printf("  %s\n", cmd->help);
    }

    printf("\n");
    ansi_color(COLOR_YELLOW);
    printf("Press any key to continue...");
    ansi_reset();
    fflush(stdout);
    term_getkey();

    print_room(gs);
}

void cmd_quit(GameState *gs)
{
    char c;

    printf("\n");
    ansi_bold();
    printf("Quit?  [Y]es  [N]o  [S]ave and quit\n> ");
    ansi_reset();
    fflush(stdout);

    c = term_getkey();

    if (c == 27) { cancel_command(gs); return; }

    c = TO_LOWER(c);
    printf("%c\n", c);

    if (c == 'n') {
        cancel_command(gs);
        return;
    }

    if (c == 's') {
        if (prompt_save_path(gs))
            game_save(gs, gs->save_path);
        else
            printf("Quitting without saving.\n");
    } else if (c != 'y') {
        cancel_command(gs);
        return;
    }

    printf("Goodbye.\n");
    exit(0);
}

/* -------------------------------------------------------------------------
 * Command dispatch table
 * ------------------------------------------------------------------------- */
Command command_table[] = {
    { 'a', "Attack",  cmd_attack,    "Attack an NPC in the room"         },
    { 'd', "Drop",    cmd_drop,      "Drop an inventory item"            },
    { 'e', "Equip",   cmd_equip,     "Equip or unequip a weapon/shield"  },
    { 'g', "Go",      cmd_go,        "Move in a direction"               },
    { 'h', "Help",    cmd_help,      "Redisplay the current room"        },
    { 'i', "Inspect", cmd_inspect,   "Inspect an object or inventory"    },
    { 'l', "Look",    cmd_look,      "Look in a direction"               },
    { 'o', "Open",    cmd_open,      "Open a container in the room"      },
    { 'p', "Pickup",  cmd_pickup,    "Pick up an object"                 },
    { 'q', "Quit",    cmd_quit,      "Quit (with save option)"           },
    { 'r', "Repair",  cmd_repair,    "Repair equipment"                  },
    { 's', "Save",    cmd_save,      "Save the game"                     },
    { 't', "Talk",    cmd_talk,      "Talk to someone in the room"       },
    { 'u', "Use",     cmd_use,       "Use an inventory item"             },
    { '?', "?",       cmd_help_text, "Show contextual command list"      },
    { 0,   NULL,      NULL,          NULL                                }
};

int cmd_dispatch(GameState *gs, char c)
{
    Command *cmd;
    int i;

    c = TO_LOWER(c);

    /* Hostile NPC pre-attack: fires before any non-movement command */
    if (gs->pending_hostile) {
        if (c == 'g') {
            /* Escaping: clear flag without being attacked */
            gs->pending_hostile = 0;
        } else {
            Room *r = gs->current_room;
            gs->pending_hostile = 0;
            for (i = 0; i < r->npc_count; i++) {
                NPC *n = r->npcs[i];
                if (n->hostile && n->alive && n->damage > 0) {
                    printf("%s attacks!\n", DNAME(n));
                    gs->player_hp -= n->damage;
                    if (gs->player_hp < 0) gs->player_hp = 0;
                    printf("You take %d damage. (You: %d/100 HP)\n",
                           n->damage, gs->player_hp);
                    if (gs->player_hp <= 0)
                        game_end("lose", MSG_PLAYER_KILLED);
                }
            }
        }
    }

    for (cmd = command_table; cmd->key != 0; cmd++) {
        if (cmd->key == c) {
            cmd->handler(gs);
            return 1;
        }
    }
    return 0;
}
