#ifndef INPUT_H
#define INPUT_H

#include "types.h"

/*
 * input.h - Raw terminal I/O and display helpers.
 */

/* --- Terminal control ---------------------------------------------------- */

/* Switch terminal to raw (unbuffered, no echo) mode. */
void term_raw(void);

/* Restore terminal to the state it was in before term_raw(). */
void term_restore(void);

/* Read a single keypress (blocks until one is available). */
char term_getkey(void);

/*
 * term_readline - Read a line of text (cooked mode, user can backspace/edit).
 *
 * Temporarily restores the terminal to cooked mode, reads one line into buf
 * (at most max_len-1 chars, NUL-terminated, trailing newline stripped),
 * then returns to raw mode.  buf[0] == '\0' means the user hit Enter with
 * no input (cancel).
 */
void term_readline(char *buf, int max_len);

/* --- ANSI helpers -------------------------------------------------------- */

void ansi_bold(void);
void ansi_reset(void);
void ansi_color(int fg); /* standard ANSI fg color 30-37 */
void ansi_clear_screen(void);

/* Convenience color names (standard ANSI fg codes) */
#define COLOR_DEFAULT  39
#define COLOR_RED      31
#define COLOR_GREEN    32
#define COLOR_YELLOW   33
#define COLOR_BLUE     34
#define COLOR_MAGENTA  35
#define COLOR_CYAN     36
#define COLOR_WHITE    37

/* Return value from input_direction / input_pick_object when ESC is pressed.
 * Distinct from -1 (ordinary cancel / invalid key) so handlers can clear
 * the screen and redraw the room instead of just printing an error. */
#define INPUT_ESC (-2)

/* --- High-level input helpers ------------------------------------------- */

/*
 * input_direction - Print a direction prompt and read a direction keypress.
 *
 * prompt  - label printed before the direction list (e.g. "Go where?")
 * r       - current room; only exits that exist are shown. If NULL all
 *           standard directions are listed.  Teleport is suppressed when
 *           the room has no teleport exit.
 *
 * Returns a DIR_* index, or -1 if the key is not a valid direction.
 * Echoes the chosen direction name to stdout.
 */
int input_direction(const char *prompt, const Room *r);

/*
 * input_pick_object - Show a numbered list of objects and let the player
 * choose one by pressing a digit key.
 *
 * objects[]  - array of Object pointers to display
 * count      - number of entries
 * prompt     - line printed above the list (e.g. "Pick up what?")
 * empty_msg  - printed (and -1 returned) when count == 0
 *
 * Returns the index into objects[] (0-based), or -1 on cancel / empty list.
 */
int input_pick_object(Object **objects, int count,
                      const char *prompt, const char *empty_msg);

/*
 * input_pick_npc - Pick an alive NPC from the current room.
 *
 * Builds the alive-NPC list internally.  If exactly one is alive it is
 * returned immediately (no menu).  If multiple are alive, a lettered
 * menu is shown.
 *
 * prompt    - header above the list (e.g. "Talk to whom?")
 * empty_msg - printed when no alive NPC exists (returns NULL, *status = -1)
 * status    - set to 1 on success, -1 on empty/cancel, INPUT_ESC on ESC
 *
 * Returns the chosen NPC pointer, or NULL on cancel / empty.
 */
NPC *input_pick_npc(Room *r, const char *prompt, const char *empty_msg,
                    int *status);

#endif /* INPUT_H */
