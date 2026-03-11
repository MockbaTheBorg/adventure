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
 * create_obj_from_template - Low-level: clone a template Object with a
 * specific id.  Returns a pointer to the new entry in gs->objects[],
 * or NULL on failure.  Used by create_template_instance and game_resume.
 */
Object *create_obj_from_template(GameState *gs, Object *tmpl,
                                 const char *new_id);

/*
 * create_npc_from_template - Low-level: clone a template NPC with a
 * specific id.  Resets runtime state (alive, hp, talked).  Returns a
 * pointer to the new entry in gs->npcs[], or NULL on failure.
 * Used by spawn_room_npcs and game_resume.
 */
NPC *create_npc_from_template(GameState *gs, NPC *tmpl, const char *new_id);

/*
 * spawn_room_npcs - Roll the spawn table for room on room entry.
 * For each SpawnEntry, if the probability roll succeeds and the NPC is not
 * already present, a dynamic instance is created and added to the room.
 * Should be called from room_enter() before end-condition checks.
 */
void spawn_room_npcs(GameState *gs, Room *room);

/*
 * Flag system helpers.
 *
 * flag_set   - Set a flag to 1 (auto-creates if not yet declared).
 * flag_clear - Set a flag to 0 (no-op if flag doesn't exist).
 * flag_check - Return 1 if flag is set, 0 otherwise.
 * apply_flags - Apply a FlagTrigger (set all its "set" flags, clear its "clear" flags).
 */
void flag_set   (GameState *gs, const char *name);
void flag_clear (GameState *gs, const char *name);
int  flag_check (const GameState *gs, const char *name);
void apply_flags(GameState *gs, const FlagTrigger *ft);

#endif /* LOADER_H */

