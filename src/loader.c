#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
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
        o->opened         = 0;
        o->on_pickup_end  = dup(json_get_string(item, "on_pickup_end", NULL));
        o->on_use_end     = dup(json_get_string(item, "on_use_end",    NULL));
        o->end_message    = dup(json_get_string(item, "end_message",   NULL));

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
                ex->room_id          = NULL;
                ex->door_id          = NULL;
                ex->look_description = NULL;
                continue;
            }

            ex->room_id          = dup(json_get_string(ex_node, "room_id",          NULL));
            ex->door_id          = dup(json_get_string(ex_node, "door_id",          NULL));
            ex->look_description = dup(json_get_string(ex_node, "look_description", NULL));
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
                if (r->object_count < ROOM_MAX_OBJECTS)
                    r->objects[r->object_count++] = o;
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
                    if (r->npc_count < ROOM_MAX_NPCS)
                        r->npcs[r->npc_count++] = n;
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

    cJSON_Delete(root);

    /* Validate that a "start" room exists */
    gs->current_room = find_room(gs, "start");
    if (!gs->current_room) {
        fprintf(stderr, "error: game.json has no room with id 'start'\n");
        return 0;
    }

    return 1;
}

static void free_object(Object *o)
{
    int i;
    free(o->id);
    free(o->name);
    free(o->description);
    free(o->message);
    free(o->use_target);
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
    free(n->id);
    free(n->name);
    free(n->description);
    free(n->dialogue);
    free(n->after_dialogue);
    free(n->gives);
}

static void free_room(Room *r)
{
    int d;
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
    }
    /* objects[] are pointers into gs->objects[]; do not free them here */
}

void game_free(GameState *gs)
{
    int i;
    for (i = 0; i < gs->object_count; i++) free_object(&gs->objects[i]);
    for (i = 0; i < gs->door_count;   i++) free_door  (&gs->doors[i]);
    for (i = 0; i < gs->npc_count;    i++) free_npc   (&gs->npcs[i]);
    for (i = 0; i < gs->room_count;   i++) free_room  (&gs->rooms[i]);
    for (i = 0; i < gs->win_items_count; i++) free(gs->win_items[i]);
    free(gs->win_message);
    free(gs->save_path);
    memset(gs, 0, sizeof(*gs));
}
