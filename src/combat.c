#include <stdio.h>
#include <string.h>

#include "combat.h"
#include "input.h"
#include "endgame.h"
#include "messages.h"
#include "loader.h"

/* -------------------------------------------------------------------------
 * combat_hp_bar
 * ------------------------------------------------------------------------- */

void combat_hp_bar(const char *label, int current, int max, int bar_w)
{
    int filled, i, color;

    if (max <= 0) max = 1;
    if (current < 0) current = 0;
    if (current > max) current = max;

    filled = (current * bar_w) / max;

    /* green >60%, yellow >30%, red <=30% */
    if (current * 10 > max * 6)       color = COLOR_GREEN;
    else if (current * 10 > max * 3)  color = COLOR_YELLOW;
    else                               color = COLOR_RED;

    printf("%s ", label);
    ansi_color(color);
    printf("[");
    for (i = 0; i < bar_w; i++)
        printf("%c", i < filled ? '#' : '.');
    printf("]");
    ansi_reset();
    printf(" %d/%d\n", current, max);
}

/* -------------------------------------------------------------------------
 * combat_drop_loot
 * ------------------------------------------------------------------------- */

void combat_drop_loot(GameState *gs, NPC *npc)
{
    Object *o;
    Room   *r = gs->current_room;

    if (!npc->drops) return;

    o = find_object(gs, npc->drops);
    if (!o) return;

    if (r->object_count < ROOM_MAX_OBJECTS) {
        r->objects[r->object_count++] = o;
        printf("> %s dropped: %s\n",
               DNAME(npc),
               DNAME(o));
    }
}

/* -------------------------------------------------------------------------
 * combat_attack
 * ------------------------------------------------------------------------- */

void combat_attack(GameState *gs, NPC *npc)
{
    int           player_damage;
    int           npc_damage;
    double        factor;
    Object       *weapon = gs->equipped_weapon;
    Object       *shield = gs->equipped_shield;
    const char   *weapon_name;

    weapon_name = "your fists";

    /* ---- player's strike ---- */
    player_damage = 5; /* base: unarmed */

    if (weapon) {
        weapon_name = DNAME(weapon);

        if (weapon->durability == -1)
            factor = 1.0;
        else
            factor = (double)weapon->durability / 100.0;

        player_damage = 5 + (int)(weapon->damage * factor);
        if (player_damage < 1) player_damage = 1;

        /* degrade weapon */
        if (weapon->durability != -1) {
            weapon->durability -= 2;
            if (weapon->durability < 0) weapon->durability = 0;
            if (weapon->durability == 0)
                printf("%s\n", MSG_WEAPON_BROKE);
        }
    }

    npc->hp -= player_damage;
    if (npc->hp < 0) npc->hp = 0;

    printf("You attack %s with %s.\n",
           DNAME(npc),
           weapon_name);
    printf("> You deal %d damage. (%s: %d/%d HP)\n",
           player_damage,
           DNAME(npc),
           npc->hp, npc->max_hp);

    if (npc->hp <= 0) {
        npc->alive = 0;
        printf("> %s has been defeated!\n",
               DNAME(npc));
        combat_drop_loot(gs, npc);
        return;
    }

    /* non-combat NPCs don't counter-attack */
    if (npc->damage <= 0) return;

    /* ---- NPC counter-attack ---- */
    npc_damage = npc->damage;

    if (shield) {
        double shield_factor;
        double def_ratio    = (double)shield->defense / 100.0;
        double dur_ratio    = (shield->durability == -1)
                              ? 1.0
                              : (double)shield->durability / 100.0;
        shield_factor = def_ratio * dur_ratio;
        npc_damage = npc->damage - (int)((double)npc->damage * shield_factor);
        if (npc_damage < 0) npc_damage = 0;

        /* degrade shield */
        if (shield->durability != -1) {
            shield->durability -= 3;
            if (shield->durability < 0) shield->durability = 0;
            if (shield->durability == 0)
                printf("%s\n", MSG_SHIELD_BROKE);
        }
    }

    gs->player_hp -= npc_damage;
    if (gs->player_hp < 0) gs->player_hp = 0;

    printf("> %s strikes back! You take %d damage. (You: %d/100 HP)\n",
           DNAME(npc),
           npc_damage,
           gs->player_hp);

    if (gs->player_hp <= 0)
        game_end("lose", MSG_PLAYER_KILLED);
}
