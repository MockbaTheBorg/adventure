#ifndef SAVE_H
#define SAVE_H

#include "types.h"

/*
 * save.h - Save and resume game state.
 */

/*
 * game_save - Write current state to path.
 *
 * Saves: current room id, inventory (object ids), door locked states,
 * and the current object list for every room (since pickups move objects).
 *
 * Returns 1 on success, 0 on error.
 */
int game_save(const GameState *gs, const char *path);

/*
 * game_resume - Apply a previously saved status.json onto a freshly
 * loaded GameState (call game_load() first, then game_resume()).
 *
 * Returns 1 on success, 0 on error.
 */
int game_resume(GameState *gs, const char *path);

#endif /* SAVE_H */
