# FireLane — Friendly Fire & Line-of-Fire Gate

One question, answered properly: **is one of my own people standing in that shot?**

Every AI shooter with allies needs this, and almost every one of them answers it with a single line
trace from the muzzle to the target. That answer is wrong three different ways at once, and this
document is mostly about which three.

---

## 0. Supported engine and platforms

* Unreal Engine **5.8**
* **Win64**. One runtime C++ module, no third-party code, full source included.
* No dependency on AIModule, NavigationSystem or GameplayAbilities. FireLane answers a question; it
  never moves anybody.

---

## 1. The five-minute install

1. Add a **Fire Lane** component to every actor that shoots.
2. Set **Team Id** on it. Anything with the same id will not be shot at.
3. Before you pull the trigger, call **Can Fire (Target Actor)**.

That is the whole minimum. Everything below is about doing better than the minimum.

### The player, and anything else that is not a shooter

A shooter registers itself. The player pawn, an escorted VIP and a friendly turret usually have no
FireLaneComponent, so register them by hand — once, at BeginPlay:

```
Register Friendly (Actor = Player Pawn, Team Id = 0)
```

`Unregister Friendly` when they stop mattering. Registrations whose actor has gone are dropped
automatically, so a destroyed actor is not a leak or a crash — just do not rely on that for a pawn
that merely *changed sides*; call `Set Team Id` (component) or re-register instead.

### Where the shot starts

By default the lane starts at the owner's origin, which for a character is **between the feet**. A
lane from the floor is blocked by every kerb in the level. Either set `MuzzleOffset` (owner space,
default 60 cm up) or, better, point at the real thing:

```
Set Muzzle (Component = WeaponMesh, Socket Name = Muzzle)
```

---

## 2. What "in the lane" actually means

### It is a volume, not a line

A bullet has width, a rocket has a blast radius, and a shotgun has a cone. `LaneRadius` is **half the
width of the volume you would be unhappy to find a friend in** — not the projectile's visual size.
For a hitscan rifle 40–60 cm is sensible. For a rocket launcher it should be roughly the explosion
radius, because that is what actually hurts.

### It is judged where people **will be**, not where they are

This is the setting that separates a gate that works from one that reads well in a code review.

`LookaheadSeconds` (default 0.35) projects every friendly forward along its own velocity before the
lane is judged. Without it, the gate gives permission to fire at a man who is one stride from the
lane — and he takes that stride while the bullet is in the air. The check passed, the code was
correct, and you shot your own medic.

Set it to roughly the flight time of your fastest common projectile plus a reaction beat. Zero
switches the idea off.

### There is a middle state

`EFireLaneVerdict` has five values, and `Grazing` is the one that matters:

| Verdict | Meaning |
|---|---|
| `Clear` | Nothing of ours in the lane, and if it was checked, nothing solid either. |
| `Grazing` | Nobody is *in* the lane, but somebody is inside `GrazeMargin` of it. |
| `BlockedByFriendly` | One of ours is in the lane. |
| `BlockedByGeometry` | A wall. Not a safety problem — a wasted shot. |
| `MuzzleBlocked` | Somebody is directly in front of the barrel, whatever the lane says. |

A gate that only knows clear and blocked has to be tuned either so tight that the squad never fires
or so loose that it shoots its own people. `Grazing` is where you put the judgement: let the sniper
take a grazing shot, make the machine gunner hold. `bAllowGrazingShots` on the component is the
one-line version of that decision.

### Behind the barrel is not in the lane

A team-mate standing half a metre *behind* the shooter has never blocked a shot. A dot product
against an infinite ray says he has, which is why a hand-rolled version holds fire whenever a squad
bunches up behind its own gunner. `DistanceToLane` treats the lane as a **segment** and reports
`OutOfLaneDistance()` for anything before the muzzle or past the end.

`TargetOvershoot` extends the far end, for weapons that keep going.

---

## 3. Blocked is not stop

Every hand-rolled version of this check says no and stops there, which is why AI squads stand around
looking stupid. `FFireLaneResult` carries two ways out:

**`SuggestedSidestep`** — a world-space offset from the shooter that would open the lane. Horizontal
by construction. It gets *longer* the further down the lane the blocker stands, because moving the
shooter pivots the lane around the target: a fixed "strafe right by two metres" clears the near case
and not the far one. When the required step exceeds `MaxSidestep` the field is zero, which means
"this is not solvable by stepping — go around".

**`SecondsUntilClear`** — how long until the blocker walks out on his own, at the rate he is
currently leaving. **Negative means never**: he is standing still, or coming further in. That
difference is the whole reason to return a number rather than a bool, because "wait 0.3 s" and "find
another angle" are different orders.

```
On Blocked (Blocker, Suggested Sidestep, Seconds Until Clear)
    if Seconds Until Clear > 0 and < 0.5   ->  hold, he is moving out
    else if Suggested Sidestep != 0        ->  step there
    else                                   ->  reposition
```

---

## 4. The trace budget

The friendly test is arithmetic. It is never rationed and never will be.

The **geometry** leg needs a trace, and traces are the part that scales with squad size: sixty AI
asking every frame is sixty traces a frame, for a question whose answer barely changes.
`MaxGeometryTracesPerFrame` (default 32) caps it for the whole world.

A check that arrives after the budget is spent still gets its friendly verdict and sets
`bGeometryChecked = false`. **A `Clear` verdict with that flag false means "clear of our people", not
"clear of the world".** `FireLane.Dump` reports how often that happened; a high count means the
budget is too small for the squad you actually field.

`VerdictStaleAfter` (default 0.1 s) caches the last answer for the same question, so a behaviour tree
that asks three times in one frame pays once.

Set `bCheckGeometry` to false if your project already knows about line of sight and only wants the
friendly-fire half.

---

## 5. What FireLane is not

* **It is not a targeting system.** It does not choose who to shoot at. Pair it with
  [ThreatTable](https://wiki.teufel-engineering.com/en/ThreatTable/documentation) if you want that.
* **It does not move anybody.** The sidestep is a vector. What walks it is your behaviour tree, your
  StateTree or your character movement component — deliberately, because every project already has an
  opinion about that and a plugin that insisted on one would be unusable in most of them.
* **It does not model ricochet, penetration or spread.** One lane, one radius. A shotgun is a wider
  lane, not a cone of lanes.
* **It is not a damage system.** Nothing here prevents friendly fire from *landing*; it prevents the
  AI from *choosing* to fire. If you want the damage stopped as well, that check belongs in your
  damage pipeline.

---

## 6. Console commands

| Command | What it does |
|---|---|
| `FireLane.Debug [0\|1]` | Draw every lane that gets checked, coloured by verdict, with the suggested sidestep as a cyan arrow. |
| `FireLane.Dump` | How many checks ran, how many were blocked by one of ours, and how many had to answer without a trace. |

---

## 7. API reference

### `UFireLaneComponent`

| Member | Notes |
|---|---|
| `CheckLane(AActor*)` | The full answer for a shot at an actor. |
| `CheckLaneToLocation(FVector)` | The same, for a point — a suspected position, a grenade's landing spot. |
| `CanFire(AActor*)` | One pin, for a behaviour tree. Honours `bAllowGrazingShots`. |
| `GetLastResult()` | The previous answer, without re-checking. |
| `SetMuzzle(USceneComponent*, FName)` | Where the shot starts. |
| `SetTeamId(int32)` | Changes sides and re-registers. |
| `OnBlocked` | `(Blocker, SuggestedSidestep, SecondsUntilClear)` |
| `LaneRadiusOverride`, `GrazeMarginOverride`, `LookaheadSecondsOverride`, `TargetOvershootOverride` | Left at −1 they follow Project Settings. This is how a rocket launcher opts out of the project's rifle tuning. |

### `UFireLaneStatics`

`GetFireLane`, `RegisterFriendly`, `UnregisterFriendly`, `GetFireLaneStats`, and the pure rules:

| Function | Notes |
|---|---|
| `OutOfLaneDistance()` | The sentinel for "not in the lane at all". |
| `ProjectPosition(Location, Velocity, Seconds)` | Straight line. A negative time is clamped, not obeyed. |
| `DistanceToLane(Point, Muzzle, Target, Overshoot, OutAlong)` | Segment, not ray. |
| `ClassifyLane(ClosestApproach, LaneRadius, GrazeMargin)` | Inclusive at the radius: exactly at the edge is *inside*. |
| `SidestepOffset(Muzzle, Target, Friendly, LaneRadius, Clearance)` | Horizontal. Zero when already open. |
| `IsWithinMuzzleClearance(DistanceToMuzzle, MuzzleClearance)` | A clearance of zero switches the rule off. |
| `SecondsUntilLaneClears(ClosestApproach, ApproachRate, LaneRadius)` | −1 means never. |

These are public because a project that wants the arithmetic inside an EQS test, a behaviour tree
decorator or a gameplay ability should not have to reimplement it — a second implementation is a
second set of bugs. **`ClosestApproach` must already have the friendly's collision radius subtracted**,
or a wide character is judged as a point and shot through the shoulder.

### `UFireLaneSettings`

Project Settings → Plugins → FireLane. `LaneRadius`, `GrazeMargin`, `MuzzleClearance`,
`TargetOvershoot`, `LookaheadSeconds`, `MaxSidestep`, `SidestepClearance`, `bCheckGeometry`,
`GeometryChannel`, `MaxGeometryTracesPerFrame`, `VerdictStaleAfter`, `bDrawDebugLanes`.

---

## 8. The demo level

`Content/FireLane/Maps/L_FireLaneDemo` runs **without pressing play** — the demo director ticks in the
editor viewport. A shooter, a target, and a team-mate who keeps walking through the shot. The faint
sphere ahead of him is where he will be when the bullet arrives, and it is what the gate judges.

The demo is configured as a **rocket launcher** (lane radius 150) rather than a rifle, for one honest
reason: a 55 cm lane over a fourteen-metre shot is a hairline on screen. Correct, and invisible.

---

## 9. Troubleshooting

**The AI never fires.** `LaneRadius` plus `GrazeMargin` is probably wider than your corridors. Run
`FireLane.Debug 1` and look at the tube. Also check whether `bAllowGrazingShots` should be on.

**The AI shoots its own people anyway.** Three candidates, in order of likelihood: the victim was
never registered (`FireLane.Dump` counts registrations); `LookaheadSeconds` is too short for your
projectile speed; or the damage is coming from splash, which FireLane does not model unless
`LaneRadius` is set to the blast radius.

**`Clear` verdicts through walls.** The trace budget was spent. Check `bGeometryChecked` on the
result, and raise `MaxGeometryTracesPerFrame`.

**The suggested step is always zero.** Either the lane is not blocked by a friendly (the field is only
filled for `BlockedByFriendly`), or the required step is longer than `MaxSidestep`.

**The gate holds fire for someone standing behind the shooter.** That cannot happen with this plugin —
if you are seeing it, the muzzle is behind the actor's origin. Set the muzzle component properly.
