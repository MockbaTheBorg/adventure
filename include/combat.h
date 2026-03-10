#ifndef COMBAT_H
#define COMBAT_H

#include "types.h"

/*
 * combat.h - Combat resolution functions.
 *
 * combat_hp_bar   - Print a coloured HP bar to stdout.
 * combat_attack   - Resolve one full attack round (player vs NPC).
 * combat_drop_loot - Place npc->drops object into the current room.
 */

/*
 * combat_hp_bar - Print a coloured progress bar.
 *
 * label   - text prefix, e.g. "HP"
 * current - current value
 * max     - maximum value
 * bar_w   - width of the filled/empty portion in characters
 *
 * Output looks like:  HP [########........]  72/100
 * Colour: green > 60 %, yellow > 30 %, red <= 30 %.
 */
void combat_hp_bar(const char *label, int current, int max, int bar_w);

/*
 * combat_attack - Resolve one attack round where the player attacks npc.
 *
 * Uses gs->equipped_weapon and gs->equipped_shield for the player.
 * Degrades weapon/shield durability.  Reduces npc->hp and gs->player_hp.
 * Calls game_end("lose", MSG_PLAYER_KILLED) if the player dies.
 * Marks npc->alive = 0 and calls combat_drop_loot if the NPC dies.
 */
void combat_attack(GameState *gs, NPC *npc);

/*
 * combat_drop_loot - Drop npc->drops into the current room.
 * No-op if npc->drops is NULL or the object is not found.
 */
void combat_drop_loot(GameState *gs, NPC *npc);

#endif /* COMBAT_H */
