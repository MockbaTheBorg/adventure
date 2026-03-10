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

#endif /* LOADER_H */
