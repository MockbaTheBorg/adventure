# game.json Schema Reference

A game file is a single JSON object with four top-level arrays:
`npcs`, `objects`, `doors`, `rooms`.  All four are optional in the sense
that an empty array is valid, but `rooms` must contain at least one room
with `"id": "start"`.

---

## Top-level structure

```json
{
  "win_condition": ["gold_key", "old_map"],
  "win_message":   "You have found everything. You win!",
  "npcs":    [ ... ],
  "objects": [ ... ],
  "doors":   [ ... ],
  "rooms":   [ ... ]
}
```

**Load order matters conceptually** (the engine loads objects → doors → npcs → rooms
so that rooms can reference all three).

### Top-level end condition (type 3 — inventory win)

| Field           | Type             | Notes                                          |
|-----------------|------------------|------------------------------------------------|
| `win_condition` | array of strings | Object ids that must all be in inventory       |
| `win_message`   | string           | Shown on the win screen; falls back to default |

When **all** listed objects are simultaneously in the player's inventory
(checked after every successful pickup), the game ends with a win screen.

---

## Object

```json
{
  "id":            "iron_key",          (required) unique identifier
  "name":          "Iron Key",          (optional) display name; falls back to id
  "description":   "A rusty key...",    (optional) shown on [I]nspect
  "pickupable":    true,                (optional, default true) can player carry it?
  "single_use":    false,               (optional, default false) removed from inventory after use
  "openable":      false,               (optional, default false) can be opened with [O]pen
  "open_key_id":   "silver_key",       (optional) id of Object required in inventory to open;
                                        NULL = no key needed. Key is consumed if single_use.
  "contains":      ["old_map"],         (optional) object ids released into room when opened
  "use_target":    "cellar_door",       (optional) id of the Door this object unlocks
  "message":       "You can't lift it." (optional) shown when pickup blocked or use fails
  "on_pickup_end": "lose",              (optional) "win" or "lose" — triggered on successful pickup
  "on_use_end":    "win",               (optional) "win" or "lose" — triggered on successful use
  "end_message":   "You collapse..."    (optional) shown on the end screen; falls back to default
  "is_weapon":     true,                (optional, default false) can be equipped as a weapon
  "is_shield":     true,                (optional, default false) can be equipped as a shield
  "damage":        5,                   (optional, default 0) weapon bonus damage added per attack
  "defense":       20,                  (optional, default 0) shield damage absorption percentage (0-100)
  "max_durability": 100,                (optional, default 0) 0 = durability not tracked; -1 = indestructible
  "durability":    80,                  (optional) starting durability (0-100 scale); defaults to max_durability if absent
  "heal":          30,                  (optional, default 0) HP restored when player uses this item (0 = no heal)
  "repair_amount": 25,                  (optional, default 0) durability restored per use as a repair kit (0 = not a kit)
  "on_pickup_flags": {"set":["flag1"],"clear":["flag2"]},  (optional) flags to set/clear on pickup
  "on_use_flags":    {"set":["flag3"]},                    (optional) flags to set/clear on successful use
  "is_template":   true,                (optional, default false) prototype for dynamic spawning; never placed in world
  "template_base": "sword"              (optional) base name used when generating numbered instance ids (e.g. "sword_01")
}
```

- If `pickupable` is `false` and `message` is absent, the engine shows
  `"You can't pick that up."`
- If `open_key_id` is set on an openable object, the player must have the
  named object in inventory to open it.  If the key has `"single_use": true`
  it is consumed on use.  If no key is held, `message` (or `"It's locked."`)
  is shown.
- If `use_target` is absent or the target door is not reachable, the
  engine shows `message` or `"Nothing happens."`
- `on_pickup_end` fires only on a successful pickup (object is pickupable
  and inventory is not full).
- `on_use_end` fires only on a successful use (correct key, door unlocked).
- `end_message` is shared between `on_pickup_end` and `on_use_end`.
- **Durability** is an integer on a 0–100 scale representing condition percentage.
  Weapons lose 2 durability per attack; shields lose 3 per hit absorbed.
  At 0 durability the item is broken (weapon deals no bonus damage; shield
  absorbs nothing).  Set `max_durability` to `-1` for indestructible gear
  that never degrades.
- **Weapon damage** = 5 (base unarmed) + `damage` × (`durability` / 100).
- **Shield absorption** = `defense`% × (`durability`% / 100) of incoming NPC damage.

---

## NPC

```json
{
  "id":             "old_hermit",       (required) unique identifier
  "name":           "Old Hermit",       (optional) display name; falls back to id
  "description":    "A weathered man.", (optional) shown on [I]nspect
  "dialogue":       "Take this axe...", (optional) shown on first [T]alk
  "after_dialogue": "I told you all.",  (optional) shown on subsequent talks;
                                        falls back to dialogue if absent
  "gives":          "axe"               (optional) id of Object added to player
                                        inventory on first talk; if inventory is
                                        full the object is dropped in the room
  "hostile":        true,               (optional, default false) warns player on room entry;
                                        auto-attacks before the next non-go command
  "hp":             20,                 (optional, default 0) max (and starting) hit points;
                                        0 = non-combat NPC
  "damage":         5,                  (optional, default 0) counter-attack damage per round
  "drops":          "old_dagger",       (optional) id of Object dropped in the room on death
  "healer":         true,               (optional, default false) restores player HP on every [T]alk
  "heal_amount":    15,                 (optional, default 0) HP restored per talk when healer is true
  "repairer":       true,               (optional, default false) repairs all equipped gear to max via [R]epair
  "on_talk_flags":  {"set":["quest_accepted"]},  (optional) flags to set/clear on first talk
  "on_death_flags": {"set":["dragon_slain"]},    (optional) flags to set/clear when defeated
  "is_template":    true                (optional, default false) prototype for dynamic spawning; never placed in world
}
```

- NPCs are listed in the room display under "Also here:" in cyan.
- NPCs appear in [I]nspect; their description is shown when selected.
- `[T]alk` opens a talk prompt; if only one NPC is present the picker is skipped.
- `gives` is only given once (on first talk); `talked` state is saved/restored.
- The `gives` object should not also be placed in a room — it exists only in
  `gs->objects[]` as a hidden item until the NPC grants it.

---

## Door

```json
{
  "id":            "cellar_door",                 (required) unique identifier
  "name":          "Cellar Door",                 (optional) display name; falls back to id
  "description":   "A thick oak door...",         (optional) shown when player inspects it
  "locked":        true,                          (optional, default false)
  "key_id":        "iron_key",                    (optional) id of Object that unlocks it
  "message":       "You need the iron key...",    (optional) shown when player tries to pass
  "break_key_id":  "axe",                         (optional) id of Object that destroys it
  "break_message": "The door splinters apart."    (optional) shown when door is destroyed
}
```

- If `locked` is `true` and `message` is absent, the engine shows
  `"This door is locked."`
- A door with no `key_id` cannot be unlocked; it can only be destroyed.
- `break_key_id`: when the player uses the named object on this door, it is
  **destroyed** (passable, appears as destroyed in [I]nspect). The object is
  consumed if it has `"single_use": true`.
- A destroyed door never blocks movement. Its state is saved/restored.
- Both `key_id` (unlock) and `break_key_id` (destroy) can be set on the same door.

---

## Room

```json
{
  "id":                  "hallway",          (required) unique identifier; "start" is mandatory
  "name":                "Long Hallway",     (optional) display name; falls back to id
  "description":         "A corridor...",    (optional) shown on entry / during cutscene
  "cutscene":            "next_room_id",     (optional) if set: show description, display
                                             "Press any key to continue.", then warp to
                                             this room. No exits/objects/menu shown.
                                             Cutscene rooms may chain.
  "first_visit_message": "You notice...",    (optional) shown once, on the first visit only
  "end_condition":       "win",              (optional) "win" or "lose" — triggered on room entry
  "end_message":         "You have won!",    (optional) shown on the end screen; falls back to default
  "objects":             ["chest", "note"],  (optional) list of Object ids initially in the room
  "npcs":                ["old_hermit"],     (optional) list of NPC ids present in the room
  "spawns":              [ ... ],            (optional) random NPC spawn table — see Spawn Entry below
  "exits": {
    "north":    { ... },
    "south":    { ... },
    "east":     { ... },
    "west":     { ... },
    "up":       { ... },
    "down":     { ... },
    "teleport": { ... }
  }
}
```

- Only directions with an entry in `exits` are available.
- The `"start"` room is where the player begins; its absence is a fatal error.
- `end_condition` fires after the room is displayed and any
  `first_visit_message` shown.  Cutscene rooms do not trigger end conditions.

---

## Spawn Entry  (element of a room's `"spawns"` array)

```json
{
  "npc_id":      "goblin",   (required) id of a template NPC (must have "is_template": true)
  "probability": 50,         (optional, default 50) 0-100 integer percent chance of spawning per room entry
  "respawn":     false       (optional, default false) true = re-roll on every entry; false = spawn at most once per game
}
```

- The spawned NPC receives a unique id of the form `"{npc_id}_NN"` where
  `NN` is a zero-padded two-digit number starting at `01` (e.g. `"goblin_01"`,
  `"goblin_02"`).  The engine picks the lowest unused number; maximum 99
  instances per template.
- Template NPCs (those with `"is_template": true`) are never placed directly
  in a room; they serve only as prototypes for the spawn system.  The same
  applies to template Objects.
- Up to 4 spawn entries per room are supported (`ROOM_MAX_SPAWNS`).
- **respawn = false** (default): the engine checks whether an instance from
  this template is already present in the room (alive or dead).  If so, the
  entry is skipped entirely — the NPC spawns at most once per game.
- **respawn = true**: the probability roll happens on every room entry,
  regardless of existing instances.  This allows a room to accumulate
  multiple spawned NPCs over repeated visits.
- Spawned NPCs inherit all fields from the template (name, dialogue, stats,
  drops, etc.).  Only the `id` is unique; all other string fields are shared
  with the template and must not be freed separately.
- Dynamic NPC and Object state (alive, hp, talked, durability, room
  placement) is fully preserved across save/resume.

---

## Exit  (value of each direction key inside `"exits"`)

```json
{
  "room_id":          "cellar",                (required) destination room id
  "door_id":          "cellar_door",           (optional) id of blocking Door
  "look_description": "Darkness below...",     (optional) shown on [L]ook
  "require_flag":     "bridge_repaired",       (optional) flag that must be set
  "require_item":     "gold_key",              (optional) object id that must be in inventory
  "require_npc_dead": "dragon",                (optional) NPC id that must be defeated
  "hidden":           true,                    (optional, default false) invisible until conditions met
  "blocked_message":  "The bridge is broken."  (optional) shown when conditions not met
}
```

- If `look_description` is absent, `[L]ook` shows `"There's nothing to see that way."`
- If `door_id` references a locked Door, movement is blocked and the
  door's `message` (or the default) is shown.
- A `teleport` exit works identically to any other direction.

### Conditional exits

Exits can have one or more conditions that must **all** be satisfied
(AND logic) before the player can pass:

| Field              | Check                                        |
|--------------------|----------------------------------------------|
| `require_flag`     | Named flag must be set (see Flag system)     |
| `require_item`     | Named object must be in player's inventory   |
| `require_npc_dead` | Named NPC must have `alive = 0` (defeated)   |

If **any** condition is not met:
- **`hidden: true`** — the exit is completely invisible in the exits list,
  direction prompt, and `[L]ook`.  It appears only once all conditions
  are satisfied.
- **`hidden: false`** (default) — the exit is shown with a `(blocked)`
  tag.  Attempting to go that way shows `blocked_message` (or the
  fallback `"Something prevents you from going that way."`).

Conditions are checked at display time, so the exit dynamically
appears/unblocks as the player progresses.  Door checks (`door_id`)
are evaluated **after** conditional checks — the player must first meet
conditions, then also have the key if a locked door is present.

---

## End conditions summary

Three independent end-condition mechanisms exist; any may trigger at any time:

| Type | Trigger                     | JSON location          | Fields                                       |
|------|-----------------------------|------------------------|----------------------------------------------|
| 1    | Player enters room          | Room object            | `end_condition`, `end_message`               |
| 2    | Successful pickup or use    | Object object          | `on_pickup_end`, `on_use_end`, `end_message` |
| 3    | All win items in inventory  | Top-level              | `win_condition`, `win_message`               |

`end_condition` / `on_pickup_end` / `on_use_end` accept `"win"` or `"lose"`.

---

## Combat system

The player starts with **100 HP**.  Combat is turn-based and happens
inline during the normal command loop.

### Hostile NPCs

When the player enters a room containing a hostile NPC (`"hostile": true`,
alive, with `damage > 0`), a warning is printed.  If the player's next
command is anything other than `[G]o`, every hostile NPC in the room
strikes first (dealing their `damage` value), then the chosen command
executes normally.  Choosing `[G]o` lets the player flee without being hit.

### Attacking (`[A]ttack`)

The player picks an alive NPC and strikes.  Damage dealt:

- **Unarmed** (no weapon equipped): 5 base damage.
- **Armed**: 5 + `weapon.damage` × (`weapon.durability` / 100).

After the player strikes, a combat NPC (`damage > 0`) counter-attacks.
Counter-attack damage is reduced by an equipped shield:

- **Absorbed** = `npc.damage` × (`shield.defense` / 100) × (`shield.durability` / 100).
- **Damage taken** = `npc.damage` − absorbed (minimum 0).

When an NPC reaches 0 HP it is defeated; if it has a `drops` field the
referenced object appears in the room.  When the player reaches 0 HP the
game ends with a lose screen.

### Equipment (`[E]quip`)

The player can equip one weapon and one shield from inventory.
Selecting an already-equipped item unequips it.  Equipment status and
durability are shown in the room header and in `[I]nspect`.

### Durability

Weapons lose **2** durability per attack; shields lose **3** per hit
absorbed.  At 0 durability the item is broken (no bonus damage / no
absorption).  Set `max_durability` to `-1` for indestructible gear.

### Healing

- **Healing items**: an object with `heal > 0` and no `use_target` restores
  HP when the player uses it via `[U]se`.  Player HP is capped at 100.
- **Healer NPCs**: an NPC with `"healer": true` restores `heal_amount` HP
  every time the player talks to them.

### Repair

- **Repairer NPCs**: an NPC with `"repairer": true` restores all equipped
  gear to full durability when the player uses `[R]epair`.
- **Repair kits**: an object with `repair_amount > 0` in inventory lets
  the player repair one piece of equipped gear, adding `repair_amount`
  durability (capped at `max_durability`).  Consumed if `single_use` is set.

---

## Flag system

The engine supports global boolean flags that can be set or cleared by
game events.  Flags enable reactive game design: conditional dialogues,
gated progression, and state tracking across rooms.

### Declaring flags (optional)

Flags can be pre-declared at the top level.  Pre-declared flags start
as `false`.  Flags referenced in triggers but not declared are
auto-created when first set.

```json
{
  "flags": ["bridge_repaired", "dragon_slain", "quest_accepted"]
}
```

### Flag triggers on objects

```json
{
  "id": "lever",
  "on_pickup_flags": { "set": ["bridge_repaired"] },
  "on_use_flags":    { "set": ["blessed"], "clear": ["cursed"] }
}
```

- `on_pickup_flags` fires after a successful pickup.
- `on_use_flags` fires after a successful use (door unlock, door destroy, or healing).
- Both `set` and `clear` are arrays of flag name strings (max 8 each).

### Flag triggers on NPCs

```json
{
  "id": "guard",
  "on_talk_flags":  { "set": ["quest_accepted"] },
  "on_death_flags": { "set": ["dragon_slain"] }
}
```

- `on_talk_flags` fires after the NPC's **first** talk (not on repeats).
- `on_death_flags` fires when the NPC is defeated in combat.

### Checking flags

Flags are currently used by the **conditional exits** system (see Exit
section).  The engine provides `flag_check(gs, name)` which returns 1 if
set, 0 otherwise.

### Save/resume

All flag names and values are fully preserved across save/resume.

---

## Inline color tags

Any text field loaded from JSON (descriptions, dialogue, messages, end
messages, first-visit messages, look descriptions, door messages) supports
inline color tags that render as ANSI terminal colors:

| Tag         | Effect                      |
|-------------|-----------------------------|
| `{red}`     | Red text                    |
| `{green}`   | Green text                  |
| `{yellow}`  | Yellow text                 |
| `{blue}`    | Blue text                   |
| `{magenta}` | Magenta text                |
| `{cyan}`    | Cyan text                   |
| `{white}`   | White text                  |
| `{bold}`    | Bold text                   |
| `{/}`       | Reset to default formatting |

Tags can be nested (e.g. `{bold}{red}danger{/}`).  Use `{{` to produce
a literal `{` character.  Unknown tags are printed as-is including braces.

**Example:**
```json
"description": "A {red}blood-stained{/} altar sits in the {bold}center{/} of the room."
```

---

## Default fallback messages summary

| Situation                             | Default message                             |
|---------------------------------------|---------------------------------------------|
| No exit in that direction             | You can't go that way.                      |
| Conditional exit not met              | Something prevents you from going that way. |
| Locked door, no door message          | This door is locked.                        |
| Door just unlocked                    | The door swings open.                       |
| Door already unlocked                 | It's already open.                          |
| Look with no description / no exit    | There's nothing to see that way.            |
| Inspect with no description           | There's nothing special about it.           |
| Pickup non-pickupable, no message     | You can't pick that up.                     |
| Use with no valid target reachable    | Nothing happens.                            |
| Use on a door that's not nearby       | You can't use that here.                    |
| Teleport with no teleport exit        | There's no teleporter here.                 |
| Room object list empty                | There's nothing here.                       |
| Inventory empty                       | You're not carrying anything.               |
| Win (no win_message / end_message)    | You have won!                               |
| Lose (no end_message)                 | Game over.                                  |
| Door destroyed (no break_message)     | You smash through the door.                 |
| Door already destroyed                | That door is already destroyed.             |
| No NPC in room                        | There's no one to talk to here.             |
| NPC with no dialogue                  | They have nothing more to say.              |
| No alive NPC to attack                | There is no one to fight here.              |
| Player killed (0 HP)                  | You have been killed.                       |
| Weapon durability reaches 0           | Your weapon is now useless!                 |
| Shield durability reaches 0           | Your shield is now useless!                 |
| No equippable items in inventory      | You have nothing to equip.                  |
| Player already at full HP             | You are already at full health.             |
| No repairer NPC or repair kit         | Nothing to repair with.                     |
| All gear already at full durability   | Already at full condition.                  |
