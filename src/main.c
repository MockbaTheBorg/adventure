#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "loader.h"
#include "save.h"
#include "input.h"
#include "commands.h"

/* -------------------------------------------------------------------------
 * Usage / argument parsing
 * ------------------------------------------------------------------------- */

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s --game <game.json> [--resume <status.json>]\n",
            prog);
}

static int parse_args(int argc, char *argv[],
                      const char **game_path, const char **save_path)
{
    int i;
    *game_path = NULL;
    *save_path = NULL;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--game") == 0 && i + 1 < argc) {
            *game_path = argv[++i];
        } else if (strcmp(argv[i], "--resume") == 0 && i + 1 < argc) {
            *save_path = argv[++i];
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return 0;
        }
    }

    if (!*game_path) {
        fprintf(stderr, "error: --game <file> is required\n");
        return 0;
    }
    return 1;
}

/* -------------------------------------------------------------------------
 * Command prompt display
 * ------------------------------------------------------------------------- */

static void print_prompt(void)
{
    ansi_color(COLOR_MAGENTA);
    printf("\nCommand: ");
    ansi_reset();
    fflush(stdout);
}

/* -------------------------------------------------------------------------
 * Game loop
 * ------------------------------------------------------------------------- */

static void game_loop(GameState *gs)
{
    char c;

    /* Display the starting room (handles cutscene rooms too) */
    room_enter(gs, gs->current_room);

    for (;;) {
        print_prompt();
        c = term_getkey();
        /* Echo the key so the player sees what they pressed */
        printf("%c\n", TO_UPPER(c));

        if (!cmd_dispatch(gs, c))
            printf("Unknown command. Press a highlighted key.\n");
    }
}

/* -------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    const char *game_path;
    const char *save_path;
    GameState   gs;

    if (!parse_args(argc, argv, &game_path, &save_path)) {
        usage(argv[0]);
        return 1;
    }

    if (!game_load(&gs, game_path)) return 1;

    if (save_path) {
        /* Store save path so [Q]uit knows where to write */
        gs.save_path = (char *)malloc(strlen(save_path) + 1);
        if (gs.save_path) strcpy(gs.save_path, save_path);
        /* Resume from existing save if the file was also given as --resume */
        if (!game_resume(&gs, save_path)) {
            fprintf(stderr, "warning: could not load save '%s', starting fresh\n",
                    save_path);
        }
    }

    term_raw();
    atexit(term_restore);

    game_loop(&gs);

    /* game_loop only returns via exit() in cmd_quit, but keep tidy */
    game_free(&gs);
    return 0;
}
