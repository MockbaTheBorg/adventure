#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "endgame.h"
#include "input.h"
#include "messages.h"

void game_end(const char *condition, const char *message)
{
    int win = (condition && strcmp(condition, "win") == 0);

    ansi_clear_screen();
    printf("\n\n");

    if (win) {
        ansi_bold(); ansi_color(COLOR_GREEN);
        printf("  **** YOU WIN ****\n");
    } else {
        ansi_bold(); ansi_color(COLOR_RED);
        printf("  **** GAME OVER ****\n");
    }
    ansi_reset();

    printf("\n");
    if (message && message[0] != '\0')
        printf("%s\n", message);
    else
        printf("%s\n", win ? MSG_WIN_DEFAULT : MSG_LOSE_DEFAULT);

    printf("\n");
    ansi_color(COLOR_YELLOW);
    printf("Press any key to exit...");
    ansi_reset();
    fflush(stdout);
    term_getkey();
    printf("\n");

    exit(0);
}

void check_room_end(const GameState *gs)
{
    const Room *r = gs->current_room;
    if (r->end_condition && r->end_condition[0] != '\0')
        game_end(r->end_condition, r->end_message);
}

void check_obj_end(const GameState *gs, const Object *o, const char *end_field)
{
    (void)gs; /* unused; reserved for future context */
    if (end_field && end_field[0] != '\0')
        game_end(end_field, o->end_message);
}

void check_inv_win(const GameState *gs)
{
    int i, j, found;

    if (gs->win_items_count == 0) return;

    for (i = 0; i < gs->win_items_count; i++) {
        found = 0;
        for (j = 0; j < gs->inventory.count; j++) {
            if (strcmp(gs->inventory.items[j]->id, gs->win_items[i]) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) return; /* missing at least one item */
    }

    /* All required items are in inventory */
    game_end("win", gs->win_message);
}
