#ifndef LOADER_H
#define LOADER_H

#include "types.h"

/*
 * loader.h - Game world loading from game.json.
 */

/*
 * game_load - Parse game.json into gs.
 *
 * Returns 1 on success, 0 on fatal error (message printed to stderr).
 * The "start" room must exist; absence is a fatal error.
 */
int game_load(GameState *gs, const char *path);

/*
 * game_free - Release all heap strings allocated during loading.
 */
void game_free(GameState *gs);

/*
 * create_template_instance - Create a numbered dynamic copy of a template
 * object (is_template == 1).  Finds the lowest free N so the new id is
 * "{template_base}_NN".  Returns a pointer to the new entry inside
 * gs->objects[], or NULL if the table is full or the template is not found.
 */
Object *create_template_instance(GameState *gs, const char *template_base);

/*
 * spawn_room_npcs - Roll the spawn table for room on room entry.
 * For each SpawnEntry, if the probability roll succeeds and the NPC is not
 * already present, a dynamic instance is created and added to the room.
 * Should be called from room_enter() before end-condition checks.
 */
void spawn_room_npcs(GameState *gs, Room *room);

#endif /* LOADER_H */

