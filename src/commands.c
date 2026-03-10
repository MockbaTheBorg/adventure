#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "commands.h"
#include "input.h"
#include "save.h"
#include "messages.h"
#include "endgame.h"

/* -------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

/*
 * print_room - Display current room: name, description, exits, objects.
 * Clears the screen first.  No first_visit_message, no cutscene logic.
 * Declared non-static (see commands.h) so cmd_help can call it externally.
 */
void print_room(const GameState *gs)
{
    const Room *r = gs->current_room;
    int d, i;

    ansi_clear_screen();
    printf("\n");
    ansi_bold(); ansi_color(COLOR_CYAN);
    printf("[ %s ]\n", r->name);
    ansi_reset();

    if (r->description && r->description[0] != '\0')
        printf("%s\n", r->description);

    /* Exits summary */
    printf("\n");
    ansi_color(COLOR_YELLOW);
    printf("Exits:");
    ansi_reset();
    for (d = 0; d < DIR_COUNT; d++) {
        const Exit *ex = &r->exits[d];
        if (!ex->room_id) continue;
        printf(" [%c]%s", dir_key[d], dir_name[d] + 1);
        if (ex->door_id) {
            Door *door = find_door((GameState *)gs, ex->door_id);
            if (door && !door->destroyed && door->locked)
                printf("(locked)");
        }
    }
    printf("\n");

    /* Objects on the floor — comma-separated */
    if (r->object_count > 0) {
        printf("\n");
        ansi_color(COLOR_GREEN);
        printf("You can see:");
        ansi_reset();
        for (i = 0; i < r->object_count; i++) {
            if (i > 0) printf(",");
            printf(" %s", r->objects[i]->name
                          ? r->objects[i]->name : r->objects[i]->id);
        }
        printf("\n");
    }

    /* NPCs in the room */
    if (r->npc_count > 0) {
        printf("\n");
        ansi_color(COLOR_CYAN);
        printf("Also here:");
        ansi_reset();
        for (i = 0; i < r->npc_count; i++) {
            if (i > 0) printf(",");
            printf(" %s", r->npcs[i]->name
                          ? r->npcs[i]->name : r->npcs[i]->id);
        }
        printf("\n");
    }
}

/*
 * cancel_command - Called when ESC is pressed during any sub-prompt.
 * Clears the screen and redraws the current room.
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
    printf("%s\n", o->name ? o->name : o->id);
    ansi_reset();
    printf("%s\n", o->description ? o->description : MSG_INSPECT_NOTHING);
}

/*
 * inspect_door - Print the name, description, and status of a door.
 */
static void inspect_door(const Door *d)
{
    const char *status;
    ansi_bold();
    printf("%s\n", d->name ? d->name : d->id);
    ansi_reset();
    printf("%s\n", d->description ? d->description : MSG_INSPECT_NOTHING);
    if (d->destroyed)   status = "destroyed";
    else if (d->locked) status = "locked";
    else                status = "open";
    printf("Status: %s\n", status);
}

/*
 * inspect_npc - Print the name and description of an NPC.
 */
static void inspect_npc(const NPC *n)
{
    ansi_bold();
    printf("%s\n", n->name ? n->name : n->id);
    ansi_reset();
    printf("%s\n", n->description ? n->description : MSG_INSPECT_NOTHING);
}

/*
 * set_save_path - Store a new save path in gs, freeing any previous one.
 */
static void set_save_path(GameState *gs, const char *path)
{
    free(gs->save_path);
    gs->save_path = (char *)malloc(strlen(path) + 1);
    if (gs->save_path) strcpy(gs->save_path, path);
}

/*
 * prompt_save_path - If no save_path is set, ask the user for a filename.
 * Returns 1 if a path is now set, 0 if the user cancelled.
 */
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
 *
 * Single entry point for all room transitions.  Handles:
 *   - Cutscene rooms: show description + "Press any key", warp to dest.
 *   - First-visit messages: shown once after the normal room display.
 * ------------------------------------------------------------------------- */
void room_enter(GameState *gs, Room *dest)
{
    int first_visit;

    gs->current_room = dest;
    first_visit      = !dest->visited;
    dest->visited    = 1;

    if (dest->cutscene) {
        /* --- cutscene room: narrative passage, no game menu --- */
        ansi_clear_screen();
        printf("\n");
        if (dest->description && dest->description[0] != '\0')
            printf("%s\n", dest->description);

        if (first_visit && dest->first_visit_message
                        && dest->first_visit_message[0] != '\0') {
            printf("\n");
            ansi_color(COLOR_YELLOW);
            printf("%s\n", dest->first_visit_message);
            ansi_reset();
        }

        printf("\n");
        ansi_color(COLOR_YELLOW);
        printf("Press any key to continue...");
        ansi_reset();
        fflush(stdout);
        term_getkey();
        printf("\n");

        dest = find_room(gs, gs->current_room->cutscene);
        if (!dest) {
            fprintf(stderr, "error: cutscene destination '%s' not found\n",
                    gs->current_room->cutscene);
            return;
        }
        room_enter(gs, dest);

    } else {
        /* --- normal room --- */
        print_room(gs);

        if (first_visit && dest->first_visit_message
                        && dest->first_visit_message[0] != '\0') {
            printf("\n");
            ansi_color(COLOR_YELLOW);
            printf("%s\n", dest->first_visit_message);
            ansi_reset();
        }

        check_room_end(gs);
    }
}

/*
 * do_go - Move in direction dir.  Shared by cmd_go and cmd_teleport.
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
    if (!ex->room_id) {
        printf("%s\n", MSG_NO_EXIT);
        return;
    }

    if (ex->door_id) {
        door = find_door(gs, ex->door_id);
        if (door && door->locked && !door->destroyed) {
            const char *msg = door->message ? door->message : MSG_DOOR_LOCKED;
            printf("%s\n", msg);
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

void cmd_go(GameState *gs)
{
    int dir = input_direction("Go where?", gs->current_room);
    do_go(gs, dir);
}

void cmd_look(GameState *gs)
{
    int dir = input_direction("Look where?", gs->current_room);
    const Exit *ex;

    if (dir == INPUT_ESC) { cancel_command(gs); return; }

    if (dir < 0) {
        printf("%s\n", MSG_LOOK_NOTHING);
        return;
    }

    ex = &gs->current_room->exits[dir];

    if (!ex->room_id) {
        printf("%s\n", MSG_LOOK_NOTHING);
        return;
    }

    if (ex->look_description && ex->look_description[0] != '\0')
        printf("%s\n", ex->look_description);
    else
        printf("%s\n", MSG_LOOK_NOTHING);

    if (ex->door_id) {
        Door *door = find_door(gs, ex->door_id);
        if (door)
            printf("%s is %s.\n",
                   door->name ? door->name : door->id,
                   door->locked ? "locked" : "open");
    }
}

/*
 * cmd_inspect - Inspect objects, doors, or NPCs in the room, or inventory.
 *
 * [a] = fixed Inventory shortcut.
 * [b]-[z] = room objects, then accessible doors, then room NPCs (up to 25).
 * Inventory sub-list uses the full a-z range (26 items).
 * ESC at any level cancels and redraws the room.
 */
void cmd_inspect(GameState *gs)
{
    /* Unified inspectable-item list: objects first, then doors, then NPCs */
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
        inames[count] = r->objects[i]->name
                        ? r->objects[i]->name : r->objects[i]->id;
        count++;
    }

    /* Doors from exits (skip destroyed; deduplicate) */
    for (d_idx = 0; d_idx < DIR_COUNT && count < 25; d_idx++) {
        const Exit *ex = &r->exits[d_idx];
        Door       *door;
        if (!ex->door_id) continue;
        door = find_door(gs, ex->door_id);
        if (!door) continue;
        /* dedup: same door can appear in multiple exits */
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
        inames[count] = door->name ? door->name : door->id;
        count++;
    }

    /* NPCs in the room */
    for (i = 0; i < r->npc_count && count < 25; i++) {
        items[count]  = r->npcs[i];
        itypes[count] = ITYPE_NPC;
        inames[count] = r->npcs[i]->name
                        ? r->npcs[i]->name : r->npcs[i]->id;
        count++;
    }

    /* Display */
    printf("\n");
    ansi_bold();
    printf("Inspect what?\n");
    ansi_reset();

    ansi_color(COLOR_YELLOW);
    printf("  [a] Inventory\n");
    ansi_reset();

    for (i = 0; i < count; i++) {
        if (itypes[i] == ITYPE_NPC)       ansi_color(COLOR_CYAN);
        else if (itypes[i] == ITYPE_DOOR)  ansi_color(COLOR_YELLOW);
        printf("  [%c] %s", 'b' + i, inames[i]);
        ansi_reset();
        if (itypes[i] == ITYPE_DOOR) {
            Door *door = (Door *)items[i];
            if (door->destroyed)   printf(" [destroyed]");
            else if (door->locked) printf(" [locked]");
            else                   printf(" [open]");
        }
        printf("\n");
    }
    printf("  [0] Cancel\n> ");
    fflush(stdout);

    c = term_getkey();
    if (c == 27) { cancel_command(gs); return; }

    if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
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
    Door   *target_door = NULL;
    int     d;

    idx = input_pick_object(gs->inventory.items, gs->inventory.count,
                            "Use what?", MSG_INV_EMPTY);
    if (idx == INPUT_ESC) { cancel_command(gs); return; }
    if (idx < 0) return;

    o = gs->inventory.items[idx];

    if (!o->use_target) {
        printf("%s\n", o->message ? o->message : MSG_USE_NOTHING);
        return;
    }

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
        printf("%s\n", bmsg);
        if (o->single_use)
            gs->inventory.items[idx] =
                gs->inventory.items[--gs->inventory.count];
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
                   contained->name ? contained->name : contained->id);
        }
    }
}

/* cmd_teleport removed; teleport is still accessible via [G]o -> T */

void cmd_talk(GameState *gs)
{
    Room   *r = gs->current_room;
    NPC    *n;
    Object *gift;
    const char *dialogue;
    int     i, idx, shown;
    char    c;

    if (r->npc_count == 0) {
        printf("%s\n", MSG_NO_NPC);
        return;
    }

    if (r->npc_count == 1) {
        n = r->npcs[0];
    } else {
        /* Inline NPC picker */
        printf("\n");
        ansi_bold();
        printf("Talk to whom?\n");
        ansi_reset();
        shown = (r->npc_count < 26) ? r->npc_count : 26;
        for (i = 0; i < shown; i++) {
            printf("  [%c] %s\n", 'a' + i,
                   r->npcs[i]->name ? r->npcs[i]->name : r->npcs[i]->id);
        }
        printf("  [0] Cancel\n> ");
        fflush(stdout);

        c = term_getkey();
        if (c == 27) { cancel_command(gs); return; }
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        printf("%c\n", c);
        if (c == '0') return;
        if (c < 'a' || c > 'z') return;
        idx = c - 'a';
        if (idx >= shown) return;
        n = r->npcs[idx];
    }

    ansi_bold();
    printf("%s:\n", n->name ? n->name : n->id);
    ansi_reset();

    if (!n->talked) {
        dialogue = n->dialogue ? n->dialogue : MSG_NPC_NOTHING;
        printf("%s\n", dialogue);
        n->talked = 1;

        /* Give object on first talk */
        if (n->gives) {
            gift = find_object(gs, n->gives);
            if (gift) {
                if (gs->inventory.count < INV_MAX_OBJECTS) {
                    gs->inventory.items[gs->inventory.count++] = gift;
                    printf("%s hands you the %s.\n",
                           n->name ? n->name : "They",
                           gift->name ? gift->name : gift->id);
                } else {
                    if (r->object_count < ROOM_MAX_OBJECTS)
                        r->objects[r->object_count++] = gift;
                    printf("%s sets the %s on the floor.\n",
                           n->name ? n->name : "They",
                           gift->name ? gift->name : gift->id);
                }
                check_inv_win(gs);
            }
        }
    } else {
        dialogue = n->after_dialogue ? n->after_dialogue
                 : n->dialogue       ? n->dialogue
                 : MSG_NPC_NOTHING;
        printf("%s\n", dialogue);
    }
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
 *
 * Shows only commands that are relevant given the current game state:
 * - [T]eleport hidden if the room has no teleport exit
 * - [P]ickup hidden if nothing in the room is pickupable
 * - [O]pen hidden if nothing in the room is openable
 * - [D]rop / [U]se hidden if inventory is empty
 */
void cmd_help_text(GameState *gs)
{
    const Room    *r = gs->current_room;
    const Command *cmd;
    int i, has_pickupable, has_openable, has_usable;

    /* Evaluate context */
    has_pickupable = 0;
    has_openable   = 0;
    has_usable     = (gs->inventory.count > 0);

    for (i = 0; i < r->object_count; i++) {
        if (r->objects[i]->pickupable) has_pickupable = 1;
        if (r->objects[i]->openable)   has_openable   = 1;
    }

    ansi_clear_screen();
    printf("\n");
    ansi_bold(); ansi_color(COLOR_CYAN);
    printf("Available commands:\n\n");
    ansi_reset();

    for (cmd = command_table; cmd->key != 0; cmd++) {
        /* Skip commands not applicable right now */
        if (cmd->key == 't' && r->npc_count == 0) continue;
        if (cmd->key == 'p' && !has_pickupable)                  continue;
        if (cmd->key == 'o' && !has_openable)                    continue;
        if (cmd->key == 'd' && !has_usable)                      continue;
        if (cmd->key == 'u' && !has_usable)                      continue;

        ansi_bold();
        printf("  [%c] %-10s",
               (cmd->key >= 'a' && cmd->key <= 'z')
                   ? (char)(cmd->key - 32) : cmd->key,
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

    /* ESC or N cancels */
    if (c == 27) { cancel_command(gs); return; }

    if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
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
 * -------------------------------------------------------------------------
 * To add a new command: add one row here and a handler above.
 * The table is scanned linearly; keep it short.
 * Terminate with a zero-key sentinel.
 * ------------------------------------------------------------------------- */
Command command_table[] = {
    { 'g', "Go",       cmd_go,        "Move in a direction"              },
    { 'l', "Look",     cmd_look,      "Look in a direction"              },
    { 'i', "Inspect",  cmd_inspect,   "Inspect an object or inventory"   },
    { 'p', "Pickup",   cmd_pickup,    "Pick up an object"                },
    { 'd', "Drop",     cmd_drop,      "Drop an inventory item"           },
    { 'u', "Use",      cmd_use,       "Use an inventory item"            },
    { 'o', "Open",     cmd_open,      "Open a container in the room"     },
    { 't', "Talk",     cmd_talk,      "Talk to someone in the room"      },
    { 's', "Save",     cmd_save,      "Save the game"                    },
    { 'h', "Help",     cmd_help,      "Redisplay the current room"       },
    { '?', "?",        cmd_help_text, "Show contextual command list"     },
    { 'q', "Quit",     cmd_quit,      "Quit (with save option)"          },
    { 0,   NULL,       NULL,          NULL                               }
};

int cmd_dispatch(GameState *gs, char c)
{
    Command *cmd;
    if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
    for (cmd = command_table; cmd->key != 0; cmd++) {
        if (cmd->key == c) {
            cmd->handler(gs);
            return 1;
        }
    }
    return 0;
}
