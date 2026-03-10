#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "save.h"
#include "loader.h"
#include "json_load.h"
#include "cJSON.h"

/* -------------------------------------------------------------------------
 * game_save
 * ------------------------------------------------------------------------- */

int game_save(const GameState *gs, const char *path)
{
    cJSON *root, *inv_arr, *doors_arr, *rooms_arr, *objs_arr, *visited_arr;
    cJSON *npcs_arr, *npc_node;
    cJSON *door_node, *room_node, *obj_arr, *obj_node;
    cJSON *dyn_objs_arr, *dyn_npcs_arr, *dyn_entry;
    char  *text;
    FILE  *fp;
    int    i, j;

    root = cJSON_CreateObject();
    if (!root) return 0;

    /* current room */
    cJSON_AddStringToObject(root, "current_room", gs->current_room->id);

    /* combat state */
    cJSON_AddNumberToObject(root, "player_hp", gs->player_hp);
    if (gs->equipped_weapon)
        cJSON_AddStringToObject(root, "equipped_weapon",
                                gs->equipped_weapon->id);
    else
        cJSON_AddNullToObject(root, "equipped_weapon");
    if (gs->equipped_shield)
        cJSON_AddStringToObject(root, "equipped_shield",
                                gs->equipped_shield->id);
    else
        cJSON_AddNullToObject(root, "equipped_shield");

    /* dynamic objects (template instances) */
    dyn_objs_arr = cJSON_AddArrayToObject(root, "dynamic_objects");
    for (i = 0; i < gs->object_count; i++) {
        if (!gs->objects[i].is_dynamic) continue;
        dyn_entry = cJSON_CreateObject();
        cJSON_AddStringToObject(dyn_entry, "id", gs->objects[i].id);
        cJSON_AddStringToObject(dyn_entry, "template_base",
                                gs->objects[i].template_base);
        cJSON_AddItemToArray(dyn_objs_arr, dyn_entry);
    }

    /* dynamic NPCs (spawned instances) — save which room they're in */
    dyn_npcs_arr = cJSON_AddArrayToObject(root, "dynamic_npcs");
    for (i = 0; i < gs->npc_count; i++) {
        int r;
        if (!gs->npcs[i].is_dynamic) continue;
        /* Find which room this NPC is in */
        for (r = 0; r < gs->room_count; r++) {
            int k;
            const Room *room = &gs->rooms[r];
            for (k = 0; k < room->npc_count; k++) {
                if (room->npcs[k] == &gs->npcs[i]) {
                    /* Derive template id: strip "_NN" suffix */
                    char tmpl_id[128];
                    const char *p;
                    size_t len;
                    p   = strrchr(gs->npcs[i].id, '_');
                    len = p ? (size_t)(p - gs->npcs[i].id)
                            : strlen(gs->npcs[i].id);
                    if (len >= sizeof(tmpl_id)) len = sizeof(tmpl_id) - 1;
                    strncpy(tmpl_id, gs->npcs[i].id, len);
                    tmpl_id[len] = '\0';

                    dyn_entry = cJSON_CreateObject();
                    cJSON_AddStringToObject(dyn_entry, "id",
                                           gs->npcs[i].id);
                    cJSON_AddStringToObject(dyn_entry, "template", tmpl_id);
                    cJSON_AddStringToObject(dyn_entry, "room", room->id);
                    cJSON_AddItemToArray(dyn_npcs_arr, dyn_entry);
                    break;
                }
            }
        }
    }

    /* inventory */
    inv_arr = cJSON_AddArrayToObject(root, "inventory");
    for (i = 0; i < gs->inventory.count; i++)
        cJSON_AddItemToArray(inv_arr,
                             cJSON_CreateString(gs->inventory.items[i]->id));

    /* door states */
    doors_arr = cJSON_AddArrayToObject(root, "doors");
    for (i = 0; i < gs->door_count; i++) {
        door_node = cJSON_CreateObject();
        cJSON_AddStringToObject(door_node, "id",        gs->doors[i].id);
        cJSON_AddBoolToObject  (door_node, "locked",    gs->doors[i].locked);
        cJSON_AddBoolToObject  (door_node, "destroyed", gs->doors[i].destroyed);
        cJSON_AddItemToArray(doors_arr, door_node);
    }

    /* room object lists + visited flags */
    rooms_arr = cJSON_AddArrayToObject(root, "rooms");
    for (i = 0; i < gs->room_count; i++) {
        const Room *r = &gs->rooms[i];
        room_node = cJSON_CreateObject();
        cJSON_AddStringToObject(room_node, "id", r->id);
        cJSON_AddBoolToObject  (room_node, "visited", r->visited);
        obj_arr = cJSON_AddArrayToObject(room_node, "objects");
        for (j = 0; j < r->object_count; j++)
            cJSON_AddItemToArray(obj_arr,
                                 cJSON_CreateString(r->objects[j]->id));
        cJSON_AddItemToArray(rooms_arr, room_node);
    }

    /* object states (opened + durability) */
    objs_arr = cJSON_AddArrayToObject(root, "objects");
    for (i = 0; i < gs->object_count; i++) {
        const Object *o = &gs->objects[i];
        if (!o->openable && o->max_durability <= 0) continue;
        obj_node = cJSON_CreateObject();
        cJSON_AddStringToObject(obj_node, "id", o->id);
        if (o->openable)
            cJSON_AddBoolToObject(obj_node, "opened", o->opened);
        if (o->max_durability > 0)
            cJSON_AddNumberToObject(obj_node, "durability", o->durability);
        cJSON_AddItemToArray(objs_arr, obj_node);
    }

    /* NPC states (talked + hp + alive) */
    npcs_arr = cJSON_AddArrayToObject(root, "npcs");
    for (i = 0; i < gs->npc_count; i++) {
        npc_node = cJSON_CreateObject();
        cJSON_AddStringToObject(npc_node, "id",     gs->npcs[i].id);
        cJSON_AddBoolToObject  (npc_node, "talked", gs->npcs[i].talked);
        cJSON_AddBoolToObject  (npc_node, "alive",  gs->npcs[i].alive);
        cJSON_AddNumberToObject(npc_node, "hp",     gs->npcs[i].hp);
        cJSON_AddItemToArray(npcs_arr, npc_node);
    }

    /* visited rooms shortlist */
    visited_arr = cJSON_AddArrayToObject(root, "visited_rooms");
    for (i = 0; i < gs->room_count; i++)
        if (gs->rooms[i].visited)
            cJSON_AddItemToArray(visited_arr,
                                 cJSON_CreateString(gs->rooms[i].id));

    text = cJSON_Print(root);
    cJSON_Delete(root);

    if (!text) {
        fprintf(stderr, "error: failed to serialise save data\n");
        return 0;
    }

    fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "error: cannot write save file '%s'\n", path);
        free(text);
        return 0;
    }
    fputs(text, fp);
    fclose(fp);
    free(text);

    printf("Game saved to '%s'.\n", path);
    return 1;
}

/* -------------------------------------------------------------------------
 * game_resume
 * ------------------------------------------------------------------------- */

int game_resume(GameState *gs, const char *path)
{
    cJSON *root, *arr, *item;
    const char *room_id;

    root = json_load_file(path);
    if (!root) return 0;

    /* --- Phase 1: Recreate dynamic objects --- */
    arr = cJSON_GetObjectItemCaseSensitive(root, "dynamic_objects");
    if (arr && cJSON_IsArray(arr)) {
        cJSON_ArrayForEach(item, arr) {
            const char *dyn_id  = json_get_string(item, "id",            NULL);
            const char *tb      = json_get_string(item, "template_base", NULL);
            Object     *tmpl    = NULL;
            Object     *obj;
            int         k;

            if (!dyn_id || !tb) continue;
            if (gs->object_count >= MAX_OBJECTS) continue;

            /* find template */
            for (k = 0; k < gs->object_count; k++) {
                if (gs->objects[k].is_template &&
                    strcmp(gs->objects[k].id, tb) == 0) {
                    tmpl = &gs->objects[k];
                    break;
                }
            }
            if (!tmpl) continue;

            obj = &gs->objects[gs->object_count++];
            *obj = *tmpl;
            obj->id = (char *)malloc(strlen(dyn_id) + 1);
            if (!obj->id) { gs->object_count--; continue; }
            strcpy(obj->id, dyn_id);
            obj->is_dynamic  = 1;
            obj->is_template = 0;
        }
    }

    /* --- Phase 2: Recreate dynamic NPCs + place in rooms --- */
    arr = cJSON_GetObjectItemCaseSensitive(root, "dynamic_npcs");
    if (arr && cJSON_IsArray(arr)) {
        cJSON_ArrayForEach(item, arr) {
            const char *dyn_id  = json_get_string(item, "id",       NULL);
            const char *tmpl_id = json_get_string(item, "template", NULL);
            const char *rid     = json_get_string(item, "room",     NULL);
            NPC        *tmpl    = NULL;
            NPC        *npc;
            Room       *room;
            int         k;

            if (!dyn_id || !tmpl_id) continue;
            if (gs->npc_count >= MAX_NPCS) continue;

            for (k = 0; k < gs->npc_count; k++) {
                if (gs->npcs[k].is_template &&
                    strcmp(gs->npcs[k].id, tmpl_id) == 0) {
                    tmpl = &gs->npcs[k];
                    break;
                }
            }
            if (!tmpl) continue;

            npc = &gs->npcs[gs->npc_count++];
            *npc = *tmpl;
            npc->id = (char *)malloc(strlen(dyn_id) + 1);
            if (!npc->id) { gs->npc_count--; continue; }
            strcpy(npc->id, dyn_id);
            npc->is_dynamic  = 1;
            npc->is_template = 0;

            /* Place in room */
            room = find_room(gs, rid);
            if (room && room->npc_count < ROOM_MAX_NPCS)
                room->npcs[room->npc_count++] = npc;
        }
    }

    /* --- Phase 3: Standard state restoration --- */

    /* current room */
    room_id = json_get_string(root, "current_room", NULL);
    if (room_id) {
        Room *r = find_room(gs, room_id);
        if (r)
            gs->current_room = r;
        else
            fprintf(stderr, "warning: saved room '%s' not found\n", room_id);
    }

    /* inventory */
    gs->inventory.count = 0;
    arr = cJSON_GetObjectItemCaseSensitive(root, "inventory");
    if (arr && cJSON_IsArray(arr)) {
        cJSON_ArrayForEach(item, arr) {
            const char *oid = cJSON_GetStringValue(item);
            Object *o = find_object(gs, oid);
            if (!o) {
                fprintf(stderr, "warning: saved inventory object '%s' not found\n",
                        oid ? oid : "(null)");
                continue;
            }
            if (gs->inventory.count < INV_MAX_OBJECTS)
                gs->inventory.items[gs->inventory.count++] = o;
        }
    }

    /* door states */
    arr = cJSON_GetObjectItemCaseSensitive(root, "doors");
    if (arr && cJSON_IsArray(arr)) {
        cJSON_ArrayForEach(item, arr) {
            const char *did = json_get_string(item, "id", NULL);
            Door *d = find_door(gs, did);
            if (!d) continue;
            d->locked    = json_get_bool(item, "locked",    d->locked);
            d->destroyed = json_get_bool(item, "destroyed", d->destroyed);
        }
    }

    /* room object lists + visited flags */
    arr = cJSON_GetObjectItemCaseSensitive(root, "rooms");
    if (arr && cJSON_IsArray(arr)) {
        cJSON_ArrayForEach(item, arr) {
            const char *rid = json_get_string(item, "id", NULL);
            cJSON *obj_arr2, *oid_node;
            Room *r = find_room(gs, rid);
            if (!r) continue;

            r->visited = json_get_bool(item, "visited", 0);

            r->object_count = 0;
            obj_arr2 = cJSON_GetObjectItemCaseSensitive(item, "objects");
            if (obj_arr2 && cJSON_IsArray(obj_arr2)) {
                cJSON_ArrayForEach(oid_node, obj_arr2) {
                    const char *oid = cJSON_GetStringValue(oid_node);
                    Object *o = find_object(gs, oid);
                    if (!o) continue;
                    if (r->object_count < ROOM_MAX_OBJECTS)
                        r->objects[r->object_count++] = o;
                }
            }
        }
    }

    /* object states (opened + durability) */
    arr = cJSON_GetObjectItemCaseSensitive(root, "objects");
    if (arr && cJSON_IsArray(arr)) {
        cJSON_ArrayForEach(item, arr) {
            const char *oid = json_get_string(item, "id", NULL);
            Object *o = find_object(gs, oid);
            if (!o) continue;
            if (o->openable)
                o->opened = json_get_bool(item, "opened", o->opened);
            if (o->max_durability > 0)
                o->durability = json_get_int(item, "durability", o->durability);
        }
    }

    /* NPC states (talked + hp + alive) */
    arr = cJSON_GetObjectItemCaseSensitive(root, "npcs");
    if (arr && cJSON_IsArray(arr)) {
        cJSON_ArrayForEach(item, arr) {
            const char *nid = json_get_string(item, "id", NULL);
            NPC *n = find_npc(gs, nid);
            if (!n) continue;
            n->talked = json_get_bool(item, "talked", n->talked);
            n->alive  = json_get_bool(item, "alive",  n->alive);
            n->hp     = json_get_int (item, "hp",     n->hp);
        }
    }

    /* --- Phase 4: Combat state --- */
    gs->player_hp = json_get_int(root, "player_hp", gs->player_hp);

    {
        const char *wid = json_get_string(root, "equipped_weapon", NULL);
        gs->equipped_weapon = wid ? find_object(gs, wid) : NULL;
    }
    {
        const char *sid = json_get_string(root, "equipped_shield", NULL);
        gs->equipped_shield = sid ? find_object(gs, sid) : NULL;
    }

    gs->pending_hostile = 0;

    cJSON_Delete(root);
    return 1;
}
