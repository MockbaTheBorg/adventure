#ifndef ENDGAME_H
#define ENDGAME_H

#include "types.h"

/*
 * endgame.h - Win / lose screen helpers.
 *
 * game_end        - Display the end screen and exit.
 * check_room_end  - Check if the current room triggers an end condition.
 * check_obj_end   - Check an object's on_pickup_end or on_use_end field.
 * check_inv_win   - Check if the player is carrying all win_condition items.
 *
 * Each check_* function calls game_end (and does not return) when the
 * condition is met, or returns normally when it is not.
 */

/*
 * game_end - Show a win or lose banner, print message, wait for keypress,
 * then call exit(0).
 *
 * condition - "win" or "lose"  (anything else is treated as "lose")
 * message   - text to display; pass NULL to use the default from messages.h
 */
void game_end(const char *condition, const char *message);

/* Check room end_condition; call after printing room in room_enter. */
void check_room_end(const GameState *gs);

/* Check an object's end field (pass o->on_pickup_end or o->on_use_end). */
void check_obj_end(const GameState *gs, const Object *o, const char *end_field);

/* Check inventory win condition (win_items[]). */
void check_inv_win(const GameState *gs);

#endif /* ENDGAME_H */
