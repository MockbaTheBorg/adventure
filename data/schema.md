# game.json Schema Reference

A game file is a single JSON object with three top-level arrays:
`objects`, `doors`, `rooms`.  All three are optional in the sense that
an empty array is valid, but `rooms` must contain at least one room
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

**Load order matters conceptually** (the engine loads objects → doors → rooms
so that rooms can reference both).

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
  "contains":      ["old_map"],         (optional) object ids released into room when opened
  "use_target":    "cellar_door",       (optional) id of the Door this object unlocks
  "message":       "You can't lift it." (optional) shown when pickup blocked or use fails
  "on_pickup_end": "lose",              (optional) "win" or "lose" — triggered on successful pickup
  "on_use_end":    "win",              (optional) "win" or "lose" — triggered on successful use
  "end_message":   "You collapse..."   (optional) shown on the end screen; falls back to default
}
```

- If `pickupable` is `false` and `message` is absent, the engine shows
  `"You can't pick that up."`
- If `use_target` is absent or the target door is not reachable, the
  engine shows `message` or `"Nothing happens."`
- `on_pickup_end` fires only on a successful pickup (object is pickupable
  and inventory is not full).
- `on_use_end` fires only on a successful use (correct key, door unlocked).
- `end_message` is shared between `on_pickup_end` and `on_use_end`.

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
  "gives":          "axe"              (optional) id of Object added to player
                                        inventory on first talk; if inventory is
                                        full the object is dropped in the room
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

## Exit  (value of each direction key inside `"exits"`)

```json
{
  "room_id":          "cellar",                (required) destination room id
  "door_id":          "cellar_door",           (optional) id of blocking Door
  "look_description": "Darkness below..."      (optional) shown on [L]ook
}
```

- If `look_description` is absent, `[L]ook` shows `"There's nothing to see that way."`
- If `door_id` references a locked Door, movement is blocked and the
  door's `message` (or the default) is shown.
- A `teleport` exit works identically to any other direction.

---

## End conditions summary

Three independent end-condition mechanisms exist; any may trigger at any time:

| Type | Trigger                     | JSON location          | Fields                             |
|------|-----------------------------|------------------------|------------------------------------|
| 1    | Player enters room          | Room object            | `end_condition`, `end_message`     |
| 2    | Successful pickup or use    | Object object          | `on_pickup_end`, `on_use_end`, `end_message` |
| 3    | All win items in inventory  | Top-level              | `win_condition`, `win_message`     |

`end_condition` / `on_pickup_end` / `on_use_end` accept `"win"` or `"lose"`.

---

## Default fallback messages summary

| Situation                             | Default message                         |
|---------------------------------------|-----------------------------------------|
| No exit in that direction             | You can't go that way.                  |
| Locked door, no door message          | This door is locked.                    |
| Door just unlocked                    | The door swings open.                   |
| Door already unlocked                 | It's already open.                      |
| Look with no description / no exit    | There's nothing to see that way.        |
| Inspect with no description           | There's nothing special about it.       |
| Pickup non-pickupable, no message     | You can't pick that up.                 |
| Use with no valid target reachable    | Nothing happens.                        |
| Use on a door that's not nearby       | You can't use that here.                |
| Teleport with no teleport exit        | There's no teleporter here.             |
| Room object list empty                | There's nothing here.                   |
| Inventory empty                       | You're not carrying anything.           |
| Win (no win_message / end_message)    | You have won!                           |
| Lose (no end_message)                 | Game over.                              |
| Door destroyed (no break_message)     | You smash through the door.             |
| Door already destroyed                | That door is already destroyed.         |
| No NPC in room                        | There's no one to talk to here.         |
| NPC with no dialogue                  | They have nothing more to say.          |
