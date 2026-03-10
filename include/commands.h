#ifndef COMMANDS_H
#define COMMANDS_H

#include "types.h"

/*
 * commands.h - Command dispatch table and handler declarations.
 *
 * To add a new command:
 *   1. Write a handler:  void cmd_foo(GameState *gs);
 *   2. Add one row to the command_table[] in commands.c:
 *        { 'f', "foo", cmd_foo, "Short help text" }
 * That's it.  No other files need to change.
 */

/* -------------------------------------------------------------------------
 * Command table entry
 * ------------------------------------------------------------------------- */
typedef void (*CmdHandler)(GameState *gs);

typedef struct Command {
    char        key;      /* single character trigger (lower case)     */
    const char *label;    /* display name shown in the prompt          */
    CmdHandler  handler;  /* function to call                          */
    const char *help;     /* one-line description for future /help cmd */
} Command;

/* Null-terminated command table, defined in commands.c */
extern Command command_table[];

/*
 * cmd_dispatch - Find and execute the command for key c.
 *
 * Returns 1 if a command was found, 0 if the key is unknown.
 */
int cmd_dispatch(GameState *gs, char c);

/*
 * print_room - Display the current room (name, description, exits, objects).
 * Does NOT trigger first_visit_message or cutscene logic.
 * Used by cmd_help to refresh the room view without side effects.
 */
void print_room(const GameState *gs);

/*
 * room_enter - Transition the player into dest.
 *
 * If dest has a cutscene field set, shows its description and waits for
 * a keypress before warping to the cutscene destination (no menu shown).
 * Otherwise prints the normal room display.
 * This is the single entry point for all room transitions.
 */
void room_enter(GameState *gs, Room *dest);

/* -------------------------------------------------------------------------
 * Individual command handlers
 * ------------------------------------------------------------------------- */
void cmd_go       (GameState *gs);
void cmd_look     (GameState *gs);
void cmd_inspect  (GameState *gs);
void cmd_pickup   (GameState *gs);
void cmd_drop     (GameState *gs);
void cmd_use      (GameState *gs);
void cmd_open      (GameState *gs);
void cmd_talk      (GameState *gs);
void cmd_save      (GameState *gs);
void cmd_help      (GameState *gs);
void cmd_help_text (GameState *gs);
void cmd_quit      (GameState *gs);

#endif /* COMMANDS_H */
