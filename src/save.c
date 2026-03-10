#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "save.h"
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
    char  *text;
    FILE  *fp;
    int    i, j;

    root = cJSON_CreateObject();
    if (!root) return 0;

    /* current room */
    cJSON_AddStringToObject(root, "current_room", gs->current_room->id);

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

    /* opened object states */
    objs_arr = cJSON_AddArrayToObject(root, "objects");
    for (i = 0; i < gs->object_count; i++) {
        if (!gs->objects[i].openable) continue;
        obj_node = cJSON_CreateObject();
        cJSON_AddStringToObject(obj_node, "id", gs->objects[i].id);
        cJSON_AddBoolToObject  (obj_node, "opened", gs->objects[i].opened);
        cJSON_AddItemToArray(objs_arr, obj_node);
    }

    /* NPC talked states */
    npcs_arr = cJSON_AddArrayToObject(root, "npcs");
    for (i = 0; i < gs->npc_count; i++) {
        npc_node = cJSON_CreateObject();
        cJSON_AddStringToObject(npc_node, "id",     gs->npcs[i].id);
        cJSON_AddBoolToObject  (npc_node, "talked", gs->npcs[i].talked);
        cJSON_AddItemToArray(npcs_arr, npc_node);
    }

    /* visited rooms shortlist (redundant but convenient for tooling) */
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
            cJSON *obj_arr, *oid_node;
            Room *r = find_room(gs, rid);
            if (!r) continue;

            r->visited = json_get_bool(item, "visited", 0);

            r->object_count = 0;
            obj_arr = cJSON_GetObjectItemCaseSensitive(item, "objects");
            if (obj_arr && cJSON_IsArray(obj_arr)) {
                cJSON_ArrayForEach(oid_node, obj_arr) {
                    const char *oid = cJSON_GetStringValue(oid_node);
                    Object *o = find_object(gs, oid);
                    if (!o) continue;
                    if (r->object_count < ROOM_MAX_OBJECTS)
                        r->objects[r->object_count++] = o;
                }
            }
        }
    }

    /* opened object states */
    arr = cJSON_GetObjectItemCaseSensitive(root, "objects");
    if (arr && cJSON_IsArray(arr)) {
        cJSON_ArrayForEach(item, arr) {
            const char *oid = json_get_string(item, "id", NULL);
            Object *o = find_object(gs, oid);
            if (!o) continue;
            o->opened = json_get_bool(item, "opened", o->opened);
        }
    }

    /* NPC talked states */
    arr = cJSON_GetObjectItemCaseSensitive(root, "npcs");
    if (arr && cJSON_IsArray(arr)) {
        cJSON_ArrayForEach(item, arr) {
            const char *nid = json_get_string(item, "id", NULL);
            NPC *n = find_npc(gs, nid);
            if (!n) continue;
            n->talked = json_get_bool(item, "talked", n->talked);
        }
    }

    cJSON_Delete(root);
    return 1;
}
