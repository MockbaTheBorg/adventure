#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "loader.h"
#include "json_load.h"
#include "cJSON.h"

/* -------------------------------------------------------------------------
 * Direction tables (definitions for externs declared in types.h)
 * ------------------------------------------------------------------------- */
const char *dir_name[DIR_COUNT] = {
    "north", "south", "east", "west", "up", "down", "teleport"
};

const char dir_key[DIR_COUNT] = {
    'n', 's', 'e', 'w', 'u', 'd', 't'
};

/* -------------------------------------------------------------------------
 * Lookup helpers
 * ------------------------------------------------------------------------- */
Room *find_room(GameState *gs, const char *id)
{
    int i;
    if (!id) return NULL;
    for (i = 0; i < gs->room_count; i++)
        if (strcmp(gs->rooms[i].id, id) == 0)
            return &gs->rooms[i];
    return NULL;
}

Door *find_door(GameState *gs, const char *id)
{
    int i;
    if (!id) return NULL;
    for (i = 0; i < gs->door_count; i++)
        if (strcmp(gs->doors[i].id, id) == 0)
            return &gs->doors[i];
    return NULL;
}

Object *find_object(GameState *gs, const char *id)
{
    int i;
    if (!id) return NULL;
    for (i = 0; i < gs->object_count; i++)
        if (strcmp(gs->objects[i].id, id) == 0)
            return &gs->objects[i];
    return NULL;
}

NPC *find_npc(GameState *gs, const char *id)
{
    int i;
    if (!id) return NULL;
    for (i = 0; i < gs->npc_count; i++)
        if (strcmp(gs->npcs[i].id, id) == 0)
            return &gs->npcs[i];
    return NULL;
}

int dir_from_key(char c)
{
    int i;
    /* accept both lower and upper case */
    c = TO_LOWER(c);
    for (i = 0; i < DIR_COUNT; i++)
        if (dir_key[i] == c) return i;
    return -1;
}

int dir_from_name(const char *name)
{
    int i;
    if (!name) return -1;
    for (i = 0; i < DIR_COUNT; i++)
        if (strcmp(dir_name[i], name) == 0) return i;
    return -1;
}

/* -------------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------------- */

/* Duplicate a string onto the heap; returns NULL if src is NULL. */
static char *dup(const char *src)
{
    size_t len;
    char  *dst;
    if (!src) return NULL;
    len = strlen(src) + 1;
    dst = (char *)malloc(len);
    if (dst) memcpy(dst, src, len);
    return dst;
}

/* -------------------------------------------------------------------------
 * FlagTrigger loader helper
 * ------------------------------------------------------------------------- */

static void load_flag_trigger(const cJSON *parent, const char *field,
                              FlagTrigger *ft)
{
    const cJSON *node, *arr, *elem;

    ft->set_count   = 0;
    ft->clear_count = 0;

    node = cJSON_GetObjectItemCaseSensitive(parent, field);
    if (!node || !cJSON_IsObject(node)) return;

    arr = cJSON_GetObjectItemCaseSensitive(node, "set");
    if (arr && cJSON_IsArray(arr)) {
        cJSON_ArrayForEach(elem, arr) {
            const char *s = cJSON_GetStringValue(elem);
            if (s && ft->set_count < MAX_FLAG_TRIGGERS)
                ft->set[ft->set_count++] = dup(s);
        }
    }

    arr = cJSON_GetObjectItemCaseSensitive(node, "clear");
    if (arr && cJSON_IsArray(arr)) {
        cJSON_ArrayForEach(elem, arr) {
            const char *s = cJSON_GetStringValue(elem);
            if (s && ft->clear_count < MAX_FLAG_TRIGGERS)
                ft->clear[ft->clear_count++] = dup(s);
        }
    }
}

static void free_flag_trigger(FlagTrigger *ft)
{
    int i;
    for (i = 0; i < ft->set_count; i++)   free(ft->set[i]);
    for (i = 0; i < ft->clear_count; i++) free(ft->clear[i]);
}

/* -------------------------------------------------------------------------
 * Loaders for each data type
 * ------------------------------------------------------------------------- */

static int load_objects(GameState *gs, const cJSON *root)
{
    const cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "objects");
    const cJSON *item;

    if (!arr || !cJSON_IsArray(arr)) return 1; /* optional section */

    cJSON_ArrayForEach(item, arr) {
        Object *o;
        const char *id;

        if (gs->object_count >= MAX_OBJECTS) {
            fprintf(stderr, "error: too many objects (max %d)\n", MAX_OBJECTS);
            return 0;
        }
        id = json_get_string(item, "id", NULL);
        if (!id) {
            fprintf(stderr, "error: object missing 'id' field\n");
            return 0;
        }

        o = &gs->objects[gs->object_count++];
        memset(o, 0, sizeof(*o));

        o->id          = dup(id);
        o->name        = dup(json_get_string(item, "name",        o->id));
        o->description = dup(json_get_string(item, "description", NULL));
        o->message     = dup(json_get_string(item, "message",     NULL));
        o->use_target  = dup(json_get_string(item, "use_target",  NULL));
        o->pickupable     = json_get_bool(item, "pickupable", 1);
        o->single_use     = json_get_bool(item, "single_use",  0);
        o->openable       = json_get_bool(item, "openable",    0);
        o->open_key_id    = dup(json_get_string(item, "open_key_id", NULL));
        o->opened         = 0;
        o->on_pickup_end  = dup(json_get_string(item, "on_pickup_end", NULL));
        o->on_use_end     = dup(json_get_string(item, "on_use_end",    NULL));
        o->end_message    = dup(json_get_string(item, "end_message",   NULL));

        /* Equipment / healing / repair / template fields */
        o->is_weapon      = json_get_bool(item, "is_weapon",      0);
        o->is_shield      = json_get_bool(item, "is_shield",      0);
        o->damage         = json_get_int (item, "damage",         0);
        o->defense        = json_get_int (item, "defense",        0);
        o->max_durability = json_get_int (item, "max_durability", 0);
        if (o->max_durability == -1)
            o->durability = -1;             /* indestructible */
        else if (o->max_durability > 0)
            o->durability = json_get_int(item, "durability", o->max_durability);
        else
            o->durability = 0;
        o->heal           = json_get_int (item, "heal",           0);
        o->repair_amount  = json_get_int (item, "repair_amount",  0);
        o->is_template    = json_get_bool(item, "is_template",    0);
        o->is_dynamic     = 0;
        {
            const char *tb = json_get_string(item, "template_base", NULL);
            if (tb) {
                strncpy(o->template_base, tb, sizeof(o->template_base) - 1);
                o->template_base[sizeof(o->template_base) - 1] = '\0';
            }
        }

        /* Load contains array */
        {
            const cJSON *carr = cJSON_GetObjectItemCaseSensitive(item, "contains");
            const cJSON *cid;
            o->contains_count = 0;
            if (carr && cJSON_IsArray(carr)) {
                cJSON_ArrayForEach(cid, carr) {
                    const char *s = cJSON_GetStringValue(cid);
                    if (s && o->contains_count < OBJ_MAX_CONTAINS)
                        o->contains[o->contains_count++] = dup(s);
                }
            }
        }

        /* Flag triggers */
        load_flag_trigger(item, "on_pickup_flags", &o->on_pickup_flags);
        load_flag_trigger(item, "on_use_flags",    &o->on_use_flags);
    }
    return 1;
}

static int load_doors(GameState *gs, const cJSON *root)
{
    const cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "doors");
    const cJSON *item;

    if (!arr || !cJSON_IsArray(arr)) return 1; /* optional section */

    cJSON_ArrayForEach(item, arr) {
        Door *d;
        const char *id;

        if (gs->door_count >= MAX_DOORS) {
            fprintf(stderr, "error: too many doors (max %d)\n", MAX_DOORS);
            return 0;
        }
        id = json_get_string(item, "id", NULL);
        if (!id) {
            fprintf(stderr, "error: door missing 'id' field\n");
            return 0;
        }

        d = &gs->doors[gs->door_count++];
        memset(d, 0, sizeof(*d));

        d->id            = dup(id);
        d->name          = dup(json_get_string(item, "name",          d->id));
        d->description   = dup(json_get_string(item, "description",   NULL));
        d->message       = dup(json_get_string(item, "message",       NULL));
        d->key_id        = dup(json_get_string(item, "key_id",        NULL));
        d->locked        = json_get_bool(item, "locked", 0);
        d->break_key_id  = dup(json_get_string(item, "break_key_id",  NULL));
        d->break_message = dup(json_get_string(item, "break_message", NULL));
        d->destroyed     = 0;
    }
    return 1;
}

static int load_npcs(GameState *gs, const cJSON *root)
{
    const cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "npcs");
    const cJSON *item;

    if (!arr || !cJSON_IsArray(arr)) return 1; /* optional section */

    cJSON_ArrayForEach(item, arr) {
        NPC        *n;
        const char *id;

        if (gs->npc_count >= MAX_NPCS) {
            fprintf(stderr, "error: too many NPCs (max %d)\n", MAX_NPCS);
            return 0;
        }
        id = json_get_string(item, "id", NULL);
        if (!id) {
            fprintf(stderr, "error: NPC missing 'id' field\n");
            return 0;
        }

        n = &gs->npcs[gs->npc_count++];
        memset(n, 0, sizeof(*n));

        n->id             = dup(id);
        n->name           = dup(json_get_string(item, "name",           n->id));
        n->description    = dup(json_get_string(item, "description",    NULL));
        n->dialogue       = dup(json_get_string(item, "dialogue",       NULL));
        n->after_dialogue = dup(json_get_string(item, "after_dialogue", NULL));
        n->gives          = dup(json_get_string(item, "gives",          NULL));
        n->talked         = 0;
        /* Combat / services / dynamic fields */
        n->hostile      = json_get_bool(item, "hostile",      0);
        n->max_hp       = json_get_int (item, "hp",           0);
        n->hp           = n->max_hp;
        n->damage       = json_get_int (item, "damage",       0);
        n->alive        = 1;
        n->drops        = dup(json_get_string(item, "drops",  NULL));
        n->healer       = json_get_bool(item, "healer",       0);
        n->heal_amount  = json_get_int (item, "heal_amount",  0);
        n->repairer     = json_get_bool(item, "repairer",     0);
        n->is_template  = json_get_bool(item, "is_template",  0);
        n->is_dynamic   = 0;

        /* Flag triggers */
        load_flag_trigger(item, "on_talk_flags",  &n->on_talk_flags);
        load_flag_trigger(item, "on_death_flags", &n->on_death_flags);
    }
    return 1;
}

static int load_rooms(GameState *gs, const cJSON *root)
{
    const cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "rooms");
    const cJSON *item;

    if (!arr || !cJSON_IsArray(arr)) {
        fprintf(stderr, "error: game.json has no 'rooms' array\n");
        return 0;
    }

    cJSON_ArrayForEach(item, arr) {
        Room *r;
        const char *id;
        const cJSON *exits_node;
        const cJSON *objects_node;
        int d;

        if (gs->room_count >= MAX_ROOMS) {
            fprintf(stderr, "error: too many rooms (max %d)\n", MAX_ROOMS);
            return 0;
        }
        id = json_get_string(item, "id", NULL);
        if (!id) {
            fprintf(stderr, "error: room missing 'id' field\n");
            return 0;
        }

        r = &gs->rooms[gs->room_count++];
        memset(r, 0, sizeof(*r));

        r->id                  = dup(id);
        r->name                = dup(json_get_string(item, "name",                r->id));
        r->description         = dup(json_get_string(item, "description",         NULL));
        r->cutscene            = dup(json_get_string(item, "cutscene",            NULL));
        r->first_visit_message = dup(json_get_string(item, "first_visit_message", NULL));
        r->end_condition       = dup(json_get_string(item, "end_condition",       NULL));
        r->end_message         = dup(json_get_string(item, "end_message",         NULL));
        r->visited             = 0;

        /* --- exits --- */
        exits_node = cJSON_GetObjectItemCaseSensitive(item, "exits");
        for (d = 0; d < DIR_COUNT; d++) {
            Exit *ex = &r->exits[d];
            const cJSON *ex_node = NULL;

            if (exits_node)
                ex_node = cJSON_GetObjectItemCaseSensitive(exits_node,
                                                           dir_name[d]);
            if (!ex_node) {
                memset(ex, 0, sizeof(*ex));
                continue;
            }

            ex->room_id          = dup(json_get_string(ex_node, "room_id",          NULL));
            ex->door_id          = dup(json_get_string(ex_node, "door_id",          NULL));
            ex->look_description = dup(json_get_string(ex_node, "look_description", NULL));
            ex->require_flag     = dup(json_get_string(ex_node, "require_flag",     NULL));
            ex->require_item     = dup(json_get_string(ex_node, "require_item",     NULL));
            ex->require_npc_dead = dup(json_get_string(ex_node, "require_npc_dead", NULL));
            ex->hidden           = json_get_bool(ex_node, "hidden", 0);
            ex->blocked_message  = dup(json_get_string(ex_node, "blocked_message",  NULL));
        }

        /* --- initial objects in this room --- */
        objects_node = cJSON_GetObjectItemCaseSensitive(item, "objects");
        if (objects_node && cJSON_IsArray(objects_node)) {
            const cJSON *oid_node;
            cJSON_ArrayForEach(oid_node, objects_node) {
                const char *oid = cJSON_GetStringValue(oid_node);
                Object *o = find_object(gs, oid);
                if (!o) {
                    fprintf(stderr,
                            "warning: room '%s' references unknown object '%s'\n",
                            id, oid ? oid : "(null)");
                    continue;
                }
                if (r->object_count < ROOM_MAX_OBJECTS) {
                    r->objects[r->object_count++] = o;
                } else {
                    fprintf(stderr,
                            "warning: room '%s' exceeds object limit (%d)\n",
                            id, ROOM_MAX_OBJECTS);
                }
            }
        }

        /* --- initial NPCs in this room --- */
        {
            const cJSON *npcs_node = cJSON_GetObjectItemCaseSensitive(item, "npcs");
            if (npcs_node && cJSON_IsArray(npcs_node)) {
                const cJSON *nid_node;
                cJSON_ArrayForEach(nid_node, npcs_node) {
                    const char *nid = cJSON_GetStringValue(nid_node);
                    NPC *n = find_npc(gs, nid);
                    if (!n) {
                        fprintf(stderr,
                                "warning: room '%s' references unknown NPC '%s'\n",
                                id, nid ? nid : "(null)");
                        continue;
                    }
                    if (r->npc_count < ROOM_MAX_NPCS) {
                        r->npcs[r->npc_count++] = n;
                    } else {
                        fprintf(stderr,
                                "warning: room '%s' exceeds NPC limit (%d)\n",
                                id, ROOM_MAX_NPCS);
                    }
                }
            }
        }

        /* --- spawn table for this room --- */
        {
            const cJSON *sp_arr = cJSON_GetObjectItemCaseSensitive(item, "spawns");
            if (sp_arr && cJSON_IsArray(sp_arr)) {
                const cJSON *sp_node;
                cJSON_ArrayForEach(sp_node, sp_arr) {
                    const char *sp_npc_id;
                    SpawnEntry *se;
                    if (r->spawn_count >= ROOM_MAX_SPAWNS) {
                        fprintf(stderr,
                                "warning: room '%s' exceeds spawn limit (%d)\n",
                                id, ROOM_MAX_SPAWNS);
                        break;
                    }
                    sp_npc_id = json_get_string(sp_node, "npc_id", NULL);
                    if (!sp_npc_id) continue;
                    se = &r->spawns[r->spawn_count++];
                    se->npc_id      = dup(sp_npc_id);
                    se->probability = json_get_int (sp_node, "probability", 50);
                    se->respawn     = json_get_bool(sp_node, "respawn",      0);
                }
            }
        }
    }
    return 1;
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

int game_load(GameState *gs, const char *path)
{
    cJSON *root;

    memset(gs, 0, sizeof(*gs));

    root = json_load_file(path);
    if (!root) return 0;

    /* Load order matters: objects, doors, and NPCs must exist before rooms
       reference them. */
    if (!load_objects(gs, root)) { cJSON_Delete(root); return 0; }
    if (!load_doors  (gs, root)) { cJSON_Delete(root); return 0; }
    if (!load_npcs   (gs, root)) { cJSON_Delete(root); return 0; }
    if (!load_rooms  (gs, root)) { cJSON_Delete(root); return 0; }

    /* Top-level inventory win condition */
    {
        const cJSON *wc = cJSON_GetObjectItemCaseSensitive(root, "win_condition");
        const cJSON *wid;
        gs->win_items_count = 0;
        if (wc && cJSON_IsArray(wc)) {
            cJSON_ArrayForEach(wid, wc) {
                const char *s = cJSON_GetStringValue(wid);
                if (s && gs->win_items_count < MAX_WIN_ITEMS)
                    gs->win_items[gs->win_items_count++] = dup(s);
            }
        }
        gs->win_message = dup(json_get_string(root, "win_message", NULL));
    }

    /* Top-level flag declarations (optional; flags are also auto-created) */
    {
        const cJSON *fl = cJSON_GetObjectItemCaseSensitive(root, "flags");
        const cJSON *fid;
        gs->flag_count = 0;
        if (fl && cJSON_IsArray(fl)) {
            cJSON_ArrayForEach(fid, fl) {
                const char *s = cJSON_GetStringValue(fid);
                if (s && gs->flag_count < MAX_FLAGS) {
                    gs->flag_names[gs->flag_count] = dup(s);
                    gs->flag_values[gs->flag_count] = 0;
                    gs->flag_count++;
                }
            }
        }
    }

    cJSON_Delete(root);

    /* Validate that a "start" room exists */
    gs->current_room = find_room(gs, "start");
    if (!gs->current_room) {
        fprintf(stderr, "error: game.json has no room with id 'start'\n");
        return 0;
    }

    /* Initialise combat state */
    gs->player_hp       = 100;
    gs->equipped_weapon = NULL;
    gs->equipped_shield = NULL;
    gs->pending_hostile = 0;

    /* Seed random for spawn rolls */
    srand((unsigned int)time(NULL));

    return 1;
}

static void free_object(Object *o)
{
    int i;
    if (o->is_dynamic) {
        /* Dynamic instances share all strings with template; only id is owned */
        free(o->id);
        return;
    }
    free_flag_trigger(&o->on_pickup_flags);
    free_flag_trigger(&o->on_use_flags);
    free(o->id);
    free(o->name);
    free(o->description);
    free(o->message);
    free(o->use_target);
    free(o->open_key_id);
    free(o->on_pickup_end);
    free(o->on_use_end);
    free(o->end_message);
    for (i = 0; i < o->contains_count; i++)
        free(o->contains[i]);
}

static void free_door(Door *d)
{
    free(d->id);
    free(d->name);
    free(d->description);
    free(d->message);
    free(d->key_id);
    free(d->break_key_id);
    free(d->break_message);
}

static void free_npc(NPC *n)
{
    if (n->is_dynamic) {
        /* Dynamic instances share all strings with template; only id is owned */
        free(n->id);
        return;
    }
    free_flag_trigger(&n->on_talk_flags);
    free_flag_trigger(&n->on_death_flags);
    free(n->id);
    free(n->name);
    free(n->description);
    free(n->dialogue);
    free(n->after_dialogue);
    free(n->gives);
    free(n->drops);
}

static void free_room(Room *r)
{
    int d, s;
    free(r->id);
    free(r->name);
    free(r->description);
    free(r->cutscene);
    free(r->first_visit_message);
    free(r->end_condition);
    free(r->end_message);
    for (d = 0; d < DIR_COUNT; d++) {
        free(r->exits[d].room_id);
        free(r->exits[d].door_id);
        free(r->exits[d].look_description);
        free(r->exits[d].require_flag);
        free(r->exits[d].require_item);
        free(r->exits[d].require_npc_dead);
        free(r->exits[d].blocked_message);
    }
    for (s = 0; s < r->spawn_count; s++)
        free(r->spawns[s].npc_id);
    /* objects[] and npcs[] are pointers into gs->objects[]/gs->npcs[] */
}

void game_free(GameState *gs)
{
    int i;
    for (i = 0; i < gs->object_count; i++) free_object(&gs->objects[i]);
    for (i = 0; i < gs->door_count;   i++) free_door  (&gs->doors[i]);
    for (i = 0; i < gs->npc_count;    i++) free_npc   (&gs->npcs[i]);
    for (i = 0; i < gs->room_count;   i++) free_room  (&gs->rooms[i]);
    for (i = 0; i < gs->win_items_count; i++) free(gs->win_items[i]);
    for (i = 0; i < gs->flag_count; i++) free(gs->flag_names[i]);
    free(gs->win_message);
    free(gs->save_path);
    memset(gs, 0, sizeof(*gs));
}

/* -------------------------------------------------------------------------
 * create_obj_from_template / create_npc_from_template
 *
 * Low-level cloners: copy a template struct into the next free slot,
 * assign a specific id, and mark the result as dynamic.
 * ------------------------------------------------------------------------- */

Object *create_obj_from_template(GameState *gs, Object *tmpl,
                                 const char *new_id)
{
    Object *obj;
    if (gs->object_count >= MAX_OBJECTS) return NULL;
    obj = &gs->objects[gs->object_count++];
    *obj = *tmpl;
    obj->id = dup(new_id);
    if (!obj->id) { gs->object_count--; return NULL; }
    obj->is_dynamic  = 1;
    obj->is_template = 0;
    return obj;
}

NPC *create_npc_from_template(GameState *gs, NPC *tmpl, const char *new_id)
{
    NPC *npc;
    if (gs->npc_count >= MAX_NPCS) return NULL;
    npc = &gs->npcs[gs->npc_count++];
    *npc = *tmpl;
    npc->id = dup(new_id);
    if (!npc->id) { gs->npc_count--; return NULL; }
    strncpy(npc->template_base, tmpl->id, sizeof(npc->template_base) - 1);
    npc->template_base[sizeof(npc->template_base) - 1] = '\0';
    npc->is_dynamic  = 1;
    npc->is_template = 0;
    npc->alive       = 1;
    npc->hp          = tmpl->max_hp;
    npc->talked      = 0;
    return npc;
}

/* -------------------------------------------------------------------------
 * create_template_instance
 *
 * Creates a numbered dynamic copy of a template object.  The new id is
 * "{template_base}_NN" where NN is the lowest number not already in use.
 * Returns a pointer to the new entry, or NULL on failure.
 * ------------------------------------------------------------------------- */
Object *create_template_instance(GameState *gs, const char *template_base)
{
    Object *tmpl;
    char    new_id[128];
    int     i, n, found;

    /* Find the template */
    tmpl = NULL;
    for (i = 0; i < gs->object_count; i++) {
        if (gs->objects[i].is_template &&
            strcmp(gs->objects[i].id, template_base) == 0) {
            tmpl = &gs->objects[i];
            break;
        }
    }
    if (!tmpl) return NULL;
    if (strlen(template_base) + 4 >= sizeof(new_id)) return NULL; /* "_NN\0" */

    /* Find lowest available slot number */
    for (n = 1; n < 100; n++) {
        sprintf(new_id, "%s_%02d", template_base, n);
        found = 0;
        for (i = 0; i < gs->object_count; i++) {
            if (strcmp(gs->objects[i].id, new_id) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) break;
    }
    if (n >= 100) return NULL;

    return create_obj_from_template(gs, tmpl, new_id);
}

/* -------------------------------------------------------------------------
 * spawn_room_npcs
 *
 * Rolls the spawn table for a room on room_enter.  For each SpawnEntry:
 *   - If probability roll fails, skip.
 *   - If respawn=0 and an instance from this template already lives in the
 *     room (dynamic NPC whose id starts with "npc_id_"), skip.
 *   - Otherwise create a new dynamic NPC instance with id "{npc_id}_NN".
 * ------------------------------------------------------------------------- */
void spawn_room_npcs(GameState *gs, Room *room)
{
    int s, i, present, n, found;
    char   new_id[128];
    NPC   *tmpl;
    NPC   *npc;

    for (s = 0; s < room->spawn_count; s++) {
        SpawnEntry *se = &room->spawns[s];

        /* Probability roll */
        if ((rand() % 100) >= se->probability) continue;

        /* If no-respawn: check if an instance is already in the room */
        if (!se->respawn) {
            present  = 0;
            for (i = 0; i < room->npc_count; i++) {
                NPC *rn = room->npcs[i];
                if (rn->is_dynamic &&
                    strcmp(rn->template_base, se->npc_id) == 0) {
                    present = 1;
                    break;
                }
            }
            if (present) continue;
        }

        /* Find template NPC */
        tmpl = NULL;
        for (i = 0; i < gs->npc_count; i++) {
            if (gs->npcs[i].is_template &&
                strcmp(gs->npcs[i].id, se->npc_id) == 0) {
                tmpl = &gs->npcs[i];
                break;
            }
        }
        if (!tmpl) continue;
        if (room->npc_count >= ROOM_MAX_NPCS) continue;
        if (strlen(se->npc_id) + 4 >= sizeof(new_id)) continue; /* "_NN\0" */

        /* Find lowest available slot number */
        for (n = 1; n < 100; n++) {
            sprintf(new_id, "%s_%02d", se->npc_id, n);
            found = 0;
            for (i = 0; i < gs->npc_count; i++) {
                if (strcmp(gs->npcs[i].id, new_id) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found) break;
        }
        if (n >= 100) continue;

        npc = create_npc_from_template(gs, tmpl, new_id);
        if (!npc) continue;

        room->npcs[room->npc_count++] = npc;
    }
}

/* -------------------------------------------------------------------------
 * Flag system helpers
 * ------------------------------------------------------------------------- */

/* Find flag index by name; returns -1 if not found. */
static int flag_find(const GameState *gs, const char *name)
{
    int i;
    if (!name) return -1;
    for (i = 0; i < gs->flag_count; i++)
        if (strcmp(gs->flag_names[i], name) == 0) return i;
    return -1;
}

void flag_set(GameState *gs, const char *name)
{
    int idx;
    if (!name) return;
    idx = flag_find(gs, name);
    if (idx >= 0) {
        gs->flag_values[idx] = 1;
        return;
    }
    /* Auto-create */
    if (gs->flag_count >= MAX_FLAGS) {
        fprintf(stderr, "warning: flag limit reached, cannot set '%s'\n", name);
        return;
    }
    gs->flag_names[gs->flag_count] = dup(name);
    gs->flag_values[gs->flag_count] = 1;
    gs->flag_count++;
}

void flag_clear(GameState *gs, const char *name)
{
    int idx;
    if (!name) return;
    idx = flag_find(gs, name);
    if (idx >= 0) gs->flag_values[idx] = 0;
}

int flag_check(const GameState *gs, const char *name)
{
    int idx;
    if (!name) return 0;
    idx = flag_find(gs, name);
    return (idx >= 0) ? gs->flag_values[idx] : 0;
}

void apply_flags(GameState *gs, const FlagTrigger *ft)
{
    int i;
    for (i = 0; i < ft->set_count; i++)
        flag_set(gs, ft->set[i]);
    for (i = 0; i < ft->clear_count; i++)
        flag_clear(gs, ft->clear[i]);
}
