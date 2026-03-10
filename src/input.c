#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "input.h"
#include "messages.h"

/* -------------------------------------------------------------------------
 * Terminal state
 * ------------------------------------------------------------------------- */

static struct termios saved_termios;
static int term_is_raw = 0;

void term_raw(void)
{
    struct termios raw;
    tcgetattr(STDIN_FILENO, &saved_termios);
    raw = saved_termios;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    term_is_raw = 1;
}

void term_restore(void)
{
    if (term_is_raw) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
        term_is_raw = 0;
    }
}

char term_getkey(void)
{
    char c = 0;
    if (read(STDIN_FILENO, &c, 1) != 1) c = 0;
    return c;
}

/* -------------------------------------------------------------------------
 * ANSI helpers
 * ------------------------------------------------------------------------- */

void ansi_bold(void)  { printf("\033[1m");     }
void ansi_reset(void) { printf("\033[0m");     }

void ansi_color(int fg)
{
    printf("\033[%dm", fg);
}

void ansi_clear_screen(void)
{
    printf("\033[2J\033[H");
}

/* -------------------------------------------------------------------------
 * Line input (cooked mode)
 * ------------------------------------------------------------------------- */

void term_readline(char *buf, int max_len)
{
    int len;
    term_restore();
    fflush(stdout);
    if (!fgets(buf, max_len, stdin))
        buf[0] = '\0';
    else {
        len = (int)strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
    }
    term_raw();
}

/* -------------------------------------------------------------------------
 * Direction input
 * ------------------------------------------------------------------------- */

int input_direction(const char *prompt, const Room *r)
{
    char c;
    int  dir, d, first;

    printf("\n%s\n", prompt);

    /* List valid directions in [x]xxx format */
    first = 1;
    for (d = 0; d < DIR_COUNT; d++) {
        if (r && !r->exits[d].room_id) continue; /* skip missing exits */
        if (!first) printf("  ");
        printf("[%c]%s", dir_key[d], dir_name[d] + 1);
        first = 0;
    }
    if (first)
        printf("(no exits available)");
    printf("\n> ");
    fflush(stdout);

    c = term_getkey();

    if (c == 27) { /* ESC */
        printf("cancelled\n");
        return INPUT_ESC;
    }

    dir = dir_from_key(c);

    if (dir >= 0)
        printf("%s\n", dir_name[dir]);
    else
        printf("?\n");

    return dir;
}

/* -------------------------------------------------------------------------
 * NPC picker
 * ------------------------------------------------------------------------- */

NPC *input_pick_npc(Room *r, const char *prompt, const char *empty_msg,
                    int *status)
{
    NPC *alive[ROOM_MAX_NPCS];
    int  count, i, shown;
    char c;

    count = 0;
    for (i = 0; i < r->npc_count; i++)
        if (r->npcs[i]->alive)
            alive[count++] = r->npcs[i];

    if (count == 0) {
        printf("%s\n", empty_msg);
        *status = -1;
        return NULL;
    }

    if (count == 1) {
        *status = 1;
        return alive[0];
    }

    printf("\n");
    ansi_bold();
    printf("%s\n", prompt);
    ansi_reset();
    shown = (count < 26) ? count : 26;
    for (i = 0; i < shown; i++) {
        printf("  [%c] %s", 'a' + i, DNAME(alive[i]));
        if (alive[i]->max_hp > 0)
            printf(" (%d/%d HP)", alive[i]->hp, alive[i]->max_hp);
        printf("\n");
    }
    printf("  [0] Cancel\n> ");
    fflush(stdout);

    c = term_getkey();
    if (c == 27) {
        printf("cancelled\n");
        *status = INPUT_ESC;
        return NULL;
    }
    c = TO_LOWER(c);
    printf("%c\n", c);
    if (c == '0' || c < 'a' || c > 'z' || c - 'a' >= shown) {
        *status = -1;
        return NULL;
    }
    *status = 1;
    return alive[c - 'a'];
}

/* -------------------------------------------------------------------------
 * Object picker
 * ------------------------------------------------------------------------- */

int input_pick_object(Object **objects, int count,
                      const char *prompt, const char *empty_msg)
{
    char c;
    int  choice;
    int  i;
    int  shown; /* number of items actually listed */

    if (count == 0) {
        printf("%s\n", empty_msg);
        return -1;
    }

    printf("\n");
    ansi_bold();
    printf("%s\n", prompt);
    ansi_reset();

    /* Support up to 26 items via a-z */
    shown = (count < 26) ? count : 26;
    for (i = 0; i < shown; i++) {
        printf("  [%c] %s\n", 'a' + i, DNAME(objects[i]));
    }
    printf("  [0] Cancel\n");
    printf("> ");
    fflush(stdout);

    c = term_getkey();

    if (c == 27) { /* ESC */
        printf("cancelled\n");
        return INPUT_ESC;
    }

    c = TO_LOWER(c);
    printf("%c\n", c);

    if (c == '0') return -1;

    if (c < 'a' || c > 'z') return -1;
    choice = c - 'a';
    if (choice >= shown) return -1;

    return choice;
}
