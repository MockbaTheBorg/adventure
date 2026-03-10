#ifndef MESSAGES_H
#define MESSAGES_H

/*
 * messages.h - Default fallback strings for every engine interaction.
 *
 * When a JSON element has no optional "message" or description field,
 * the engine uses one of these constants instead.  Change them here to
 * alter the game's default voice without touching logic code.
 */

/* Movement */
#define MSG_NO_EXIT          "You can't go that way."
#define MSG_DOOR_LOCKED      "This door is locked."
#define MSG_DOOR_UNLOCKED    "The door swings open."
#define MSG_DOOR_ALREADY_OPEN "It's already open."
#define MSG_DOOR_DESTROYED   "You smash through the door."
#define MSG_DOOR_ALREADY_BROKEN "That door is already destroyed."

/* Looking */
#define MSG_LOOK_NOTHING     "There's nothing to see that way."

/* Inspecting objects */
#define MSG_INSPECT_NOTHING  "There's nothing special about it."

/* Picking up */
#define MSG_PICKUP_CANT      "You can't pick that up."
#define MSG_PICKUP_OK        "Taken."

/* Dropping */
#define MSG_DROP_OK          "Dropped."

/* Using objects */
#define MSG_USE_NOTHING      "Nothing happens."
#define MSG_USE_NO_TARGET    "You can't use that here."

/* Teleport */
#define MSG_NO_TELEPORT      "There's no teleporter here."

/* Inventory / room object lists */
#define MSG_ROOM_EMPTY       "There's nothing here."
#define MSG_INV_EMPTY        "You're not carrying anything."

/* Opening containers */
#define MSG_OPEN_NOTHING     "There's nothing to open here."
#define MSG_ALREADY_OPENED   "It's already open."
#define MSG_OPEN_OK          "Opened."

/* End conditions */
#define MSG_WIN_DEFAULT      "You have won!"
#define MSG_LOSE_DEFAULT     "Game over."

/* NPCs */
#define MSG_NO_NPC           "There's no one to talk to here."
#define MSG_NPC_NOTHING      "They have nothing more to say."

/* Combat */
#define MSG_ATTACK_NO_NPC   "There is no one to fight here."
#define MSG_ATTACK_DEAD_NPC "They are already defeated."
#define MSG_PLAYER_KILLED   "You have been killed."
#define MSG_WEAPON_BROKE    "Your weapon is now useless!"
#define MSG_SHIELD_BROKE    "Your shield is now useless!"

/* Equipment */
#define MSG_EQUIP_NOTHING   "You have nothing to equip."
#define MSG_EQUIP_WEAPON_OK "Weapon equipped."
#define MSG_EQUIP_SHIELD_OK "Shield equipped."
#define MSG_UNEQUIP_OK      "Unequipped."

/* Healing */
#define MSG_HEAL_FULL       "You are already at full health."
#define MSG_HEAL_OK         "You feel better."

/* Repair */
#define MSG_REPAIR_NOTHING  "Nothing to repair with."
#define MSG_REPAIR_FULL     "Already at full condition."
#define MSG_REPAIR_OK       "Repaired."

/* NPC death */
#define MSG_NPC_DEAD        "They are dead."

/* Room */
#define MSG_NO_ROOM_DESC     ""   /* silent: just show room name */

#endif /* MESSAGES_H */
