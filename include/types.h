#ifndef TYPES_H
#define TYPES_H

/*
 * types.h - Core data model for the adventure engine.
 *
 * All string fields that are optional in the JSON are NULL when absent.
 * Every code path that prints them must check for NULL and substitute
 * the appropriate constant from messages.h.
 */

/* -------------------------------------------------------------------------
 * Utility macros
 * ------------------------------------------------------------------------- */
#define TO_LOWER(c) ((char)((c) >= 'A' && (c) <= 'Z' ? (c) + 32 : (c)))
#define TO_UPPER(c) ((char)((c) >= 'a' && (c) <= 'z' ? (c) - 32 : (c)))

/* Display name: use ->name if set, otherwise fall back to ->id.
 * Works for Object*, NPC*, Door* (all have id/name as first two fields). */
#define DNAME(x) ((x)->name ? (x)->name : (x)->id)

/* -------------------------------------------------------------------------
 * Direction indices (used as array index into Room.exits[])
 * ------------------------------------------------------------------------- */
#define DIR_NORTH    0
#define DIR_SOUTH    1
#define DIR_EAST     2
#define DIR_WEST     3
#define DIR_UP       4
#define DIR_DOWN     5
#define DIR_TELEPORT 6
#define DIR_COUNT    7

/* Human-readable names, indexed by DIR_* */
extern const char *dir_name[DIR_COUNT];   /* "north", "south", ... "teleport" */
extern const char  dir_key[DIR_COUNT];    /* 'n', 's', 'e', 'w', 'u', 'd', 't' */

/* -------------------------------------------------------------------------
 * FlagTrigger  (set/clear lists attached to objects and NPCs)
 * ------------------------------------------------------------------------- */
#define MAX_FLAG_TRIGGERS 8

typedef struct FlagTrigger {
    char *set[MAX_FLAG_TRIGGERS];    /* flag names to set   */
    int   set_count;
    char *clear[MAX_FLAG_TRIGGERS];  /* flag names to clear */
    int   clear_count;
} FlagTrigger;

/* -------------------------------------------------------------------------
 * Door
 * ------------------------------------------------------------------------- */
typedef struct Door {
    char *id;           /* unique identifier used in JSON references */
    char *name;         /* short display name                         */
    char *description;  /* shown when player inspects the door        */
    char *message;      /* shown when player tries to pass while locked;
                           NULL → MSG_DOOR_LOCKED fallback            */
    char *key_id;       /* id of the Object that unlocks this door;
                           NULL = door needs no key (decorative)      */
    int   locked;       /* 1 = currently locked, 0 = open             */
    char *break_key_id;  /* id of Object that destroys this door;
                            NULL = indestructible                      */
    char *break_message; /* shown when door is destroyed;
                            NULL → MSG_DOOR_DESTROYED fallback        */
    int   destroyed;     /* runtime: 1 = smashed, passable, inert     */
} Door;

/* -------------------------------------------------------------------------
 * NPC  (non-player character)
 * ------------------------------------------------------------------------- */
typedef struct NPC {
    char *id;             /* unique identifier                          */
    char *name;           /* display name                               */
    char *description;    /* shown on [I]nspect; NULL → fallback        */
    char *dialogue;       /* text shown on first [T]alk                 */
    char *after_dialogue; /* text on subsequent talks;
                             NULL = dialogue repeats                    */
    char *gives;          /* id of Object given on first talk;
                             NULL = NPC gives nothing                   */
    int   talked;         /* runtime: 1 = first talk has occurred       */
    /* Combat */
    int   hostile;        /* 1 = warns on room entry, auto-attacks      */
    int   hp;             /* current hit points                         */
    int   max_hp;         /* 0 = non-combat NPC                         */
    int   damage;         /* counter-attack damage per round            */
    int   alive;          /* runtime: 1 = active; 0 = defeated          */
    char *drops;          /* object id dropped on death; NULL = none    */
    /* Flag triggers */
    FlagTrigger on_talk_flags;  /* flags set/cleared on first talk     */
    FlagTrigger on_death_flags; /* flags set/cleared when defeated     */
    /* Services */
    int   healer;         /* 1 = heals player on [T]alk                 */
    int   heal_amount;    /* HP restored per Talk                       */
    int   repairer;       /* 1 = repairs gear via [R]epair              */
    /* Dynamic */
    int   is_template;       /* 1 = prototype for spawning, not in world   */
    char  template_base[64]; /* template id this instance was cloned from  */
    int   is_dynamic;        /* 1 = id is malloc'd, must be freed          */
} NPC;

/* -------------------------------------------------------------------------
 * Object
 * ------------------------------------------------------------------------- */
#define OBJ_MAX_CONTAINS 16

typedef struct Object {
    char *id;           /* unique identifier                          */
    char *name;         /* short display name                         */
    char *description;  /* shown on [I]nspect; NULL → fallback        */
    char *message;      /* shown when pickup blocked or use fails;
                           NULL → appropriate MSG_* fallback          */
    char *use_target;   /* id of the Door this object unlocks;
                           NULL = object has no use-effect            */
    int   pickupable;   /* 1 = can be taken by player, 0 = fixed      */
    int   single_use;   /* 1 = removed from inventory after successful
                           use (e.g. a one-time key)                  */
    int   openable;     /* 1 = can be opened with [O]pen command      */
    char *open_key_id;  /* id of Object needed to open (NULL = no key)*/
    int   opened;       /* runtime: 1 = has already been opened       */
    char *contains[OBJ_MAX_CONTAINS]; /* object ids released into room
                           when this object is opened                 */
    int   contains_count;
    /* End conditions (type 2) ---------------------------------------- */
    char *on_pickup_end; /* "win"/"lose" triggered on successful pickup;
                            NULL = no end effect                      */
    char *on_use_end;    /* "win"/"lose" triggered on successful use;
                            NULL = no end effect                      */
    char *end_message;   /* text shown on the end screen;
                            NULL → MSG_WIN_DEFAULT / MSG_LOSE_DEFAULT */
    /* Flag triggers */
    FlagTrigger on_pickup_flags; /* flags set/cleared on successful pickup */
    FlagTrigger on_use_flags;    /* flags set/cleared on successful use    */
    /* Equipment */
    int  is_weapon;      /* 1 = can equip in weapon slot               */
    int  is_shield;      /* 1 = can equip in shield slot               */
    int  damage;         /* weapon bonus damage (0-100)                */
    int  defense;        /* shield absorb % (0-100)                    */
    int  durability;     /* current; -1 = indestructible               */
    int  max_durability; /* 0 = not tracked                            */
    /* Healing */
    int  heal;           /* HP restored on [U]se (0 = no heal)         */
    /* Repair kit */
    int  repair_amount;  /* durability restored per use (0 = no repair)*/
    /* Dynamic template */
    int  is_template;    /* 1 = prototype, never placed in world        */
    char template_base[64]; /* base name for numbered instances        */
    int  is_dynamic;     /* 1 = id is malloc'd, must be freed          */
} Object;

/* -------------------------------------------------------------------------
 * SpawnEntry  (random NPC spawn definition for a room)
 * ------------------------------------------------------------------------- */
#define ROOM_MAX_SPAWNS 4

typedef struct SpawnEntry {
    char *npc_id;       /* template NPC id                                */
    int   probability;  /* 0-100 integer percent chance per room_enter    */
    int   respawn;      /* 1 = re-roll every entry; 0 = once per game     */
} SpawnEntry;

/* -------------------------------------------------------------------------
 * Exit  (one slot per direction in a Room)
 * ------------------------------------------------------------------------- */
typedef struct Exit {
    char *room_id;          /* destination room id; NULL = no exit    */
    char *door_id;          /* blocking Door id; NULL = no door       */
    char *look_description; /* shown on [L]ook; NULL → fallback       */
    /* Conditional exit fields */
    char *require_flag;     /* flag must be set (NULL = no flag req)  */
    char *require_item;     /* item must be in inventory (NULL = none)*/
    char *require_npc_dead; /* NPC must be defeated (NULL = none)     */
    int   hidden;           /* 1 = invisible until conditions met     */
    char *blocked_message;  /* shown when conditions not met;
                               NULL → MSG_EXIT_BLOCKED fallback      */
} Exit;

/* -------------------------------------------------------------------------
 * Room
 * ------------------------------------------------------------------------- */
#define ROOM_MAX_OBJECTS 32
#define ROOM_MAX_NPCS     8

typedef struct Room {
    char   *id;                          /* unique identifier              */
    char   *name;                        /* display name                   */
    char   *description;                 /* shown on entry/look; NULL ok   */
    char   *cutscene;                    /* if set: show description,
                                           wait for keypress, then warp
                                           to this room id. No menu is
                                           shown while in a cutscene.     */
    char   *first_visit_message;         /* shown only the first time the
                                           player enters this room;
                                           NULL = no first-visit message  */
    int     visited;                     /* runtime: 1 = already visited   */
    /* End condition (type 1) ----------------------------------------- */
    char   *end_condition;               /* "win" or "lose"; NULL = normal  */
    char   *end_message;                 /* text on end screen; NULL → default */
    Exit    exits[DIR_COUNT];            /* indexed by DIR_*               */
    Object *objects[ROOM_MAX_OBJECTS];   /* objects currently in room      */
    int     object_count;
    NPC    *npcs[ROOM_MAX_NPCS];         /* NPCs currently in room         */
    int     npc_count;
    SpawnEntry spawns[ROOM_MAX_SPAWNS]; /* random NPC spawn definitions   */
    int        spawn_count;
} Room;

/* -------------------------------------------------------------------------
 * Inventory
 * ------------------------------------------------------------------------- */
#define INV_MAX_OBJECTS 32

typedef struct Inventory {
    Object *items[INV_MAX_OBJECTS];
    int     count;
} Inventory;

/* -------------------------------------------------------------------------
 * GameState  (single global instance)
 * ------------------------------------------------------------------------- */
#define MAX_ROOMS    128
#define MAX_DOORS     64
#define MAX_OBJECTS  128
#define MAX_NPCS      64
#define MAX_WIN_ITEMS 32

typedef struct GameState {
    /* World data (loaded from game.json, never freed during play) */
    Room    rooms[MAX_ROOMS];
    int     room_count;

    Door    doors[MAX_DOORS];
    int     door_count;

    Object  objects[MAX_OBJECTS];
    int     object_count;

    NPC     npcs[MAX_NPCS];
    int     npc_count;

    /* Dynamic play state */
    Room      *current_room;
    Inventory  inventory;

    /* Path to save file (may be NULL if not specified on command line) */
    char *save_path;

    /* Inventory win condition (type 3): all listed object ids must be
       carried simultaneously.  Empty = no inventory win condition.   */
    char *win_items[MAX_WIN_ITEMS];   /* object ids required to win       */
    int   win_items_count;
    char *win_message;                /* end-screen text; NULL → default  */

    /* Global flags */
    #define MAX_FLAGS 64
    char *flag_names[MAX_FLAGS];   /* declared flag name strings        */
    int   flag_values[MAX_FLAGS];  /* 0 = cleared, 1 = set             */
    int   flag_count;

    /* Combat state */
    int     player_hp;          /* 0-100; initialised to 100            */
    Object *equipped_weapon;    /* NULL = unarmed                        */
    Object *equipped_shield;    /* NULL = no shield                      */
    int     pending_hostile;    /* 1 = NPC attacks before next non-go cmd*/
} GameState;

/* -------------------------------------------------------------------------
 * Lookup helpers (implemented in loader.c)
 * ------------------------------------------------------------------------- */
Room   *find_room  (GameState *gs, const char *id);
Door   *find_door  (GameState *gs, const char *id);
Object *find_object(GameState *gs, const char *id);

NPC    *find_npc   (GameState *gs, const char *id);

/* Convert a direction key character to a DIR_* index, or -1 if invalid. */
int dir_from_key(char c);

/* Convert a direction name string to a DIR_* index, or -1 if not found. */
int dir_from_name(const char *name);

#endif /* TYPES_H */
