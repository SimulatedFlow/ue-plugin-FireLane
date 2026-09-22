# FireLane — Fab Store Listing

## Headline

**Your AI stops shooting its own people — and steps aside instead of standing there.**

## Pitch

Every AI shooter with allies needs one question answered: is one of my own people standing in that
shot? Almost every one of them answers it with a single line trace from the muzzle to the target,
and that answer is wrong three ways at once. A bullet has width. A team-mate walks. And the check
describes the world as it was, not as it will be when the projectile arrives.

**FireLane** answers it properly. The lane is a **volume** of the projectile's real danger radius,
not a line. Every friendly is **projected forward along its own velocity** before the lane is judged,
so the shot is refused while the man is still a stride away rather than after he has taken it. And
the verdict has a middle: a hard block and a graze are different answers, so your squad is neither
reckless nor mute.

When the lane is blocked, FireLane does not just say no. It returns the **sideways step that would
open it** — longer when the blocker stands further down the lane, because moving the shooter pivots
the lane about the target — and **how many seconds until the blocker walks out on his own**, with a
negative number meaning *never*. "Wait a beat" and "go around" are different orders, and your
behaviour tree finally has the information to tell them apart.

The friendly test is arithmetic and costs nothing. The geometry trace runs on a **per-frame budget
for the whole world**, so sixty shooters cost what you decide they cost — and a verdict that had to
answer without a trace says so instead of pretending.

## Feature bullets

- **A lane, not a line.** A capsule of the projectile's danger radius. For a rocket, set it to the
  blast radius — that is what actually hurts.
- **Judged where people will be.** Configurable lookahead along each friendly's velocity. This is the
  difference between a gate that works and one that merely reads correctly.
- **Five verdicts, not two.** Clear, Grazing, BlockedByFriendly, BlockedByGeometry, MuzzleBlocked. The
  graze band is where you put the judgement call.
- **Blocked is not stop.** A suggested sidestep vector and a seconds-until-clear estimate come back
  with every block.
- **Behind the barrel is not in the lane.** The lane is a segment. A team-mate standing behind the
  shooter has never blocked a shot, and this plugin knows it.
- **A trace budget for the whole world.** Rationed honestly, and flagged when it runs out.
- **Per-weapon overrides.** A rocket launcher opts out of the project's rifle tuning with four fields.
- **Pure, testable rules.** The component and the automation tests call the same functions, so they
  cannot drift apart.
- **A demo level that runs without pressing play.**
- **Full C++ source. No third-party code. Blueprint-first.**

## Technical specs

Unreal Engine 5.8 · Win64 · one runtime module · no AIModule / NavigationSystem / GAS dependency ·
Blueprint-callable throughout · 5 automation tests.

## Target audience

Anyone building a shooter, a tactics game or a squad-based action game where the AI has friends:
FPS/TPS, extraction shooters, milsim, tactical RPGs, horde games with allied NPCs.

## Suggested price

**17,18 €**

## Suggested tags

AI, Shooter, Friendly Fire, Squad, Blueprint, C++

# ==================== TECHNICAL DETAILS (Fab form) ====================

**Features:**
- Line-of-fire gate as a swept volume with a configurable danger radius
- Velocity lookahead so friendlies are judged where they will be
- Five-state verdict with a separate graze band
- Suggested sidestep vector and seconds-until-clear on every block
- Per-frame geometry trace budget for the whole world, with honest reporting when it is spent
- Muzzle clearance rule, target overshoot, per-component overrides
- Console commands FireLane.Debug and FireLane.Dump
- Demo level that ticks in the editor viewport without PIE

**Code Modules:** FireLane (Runtime, PreDefault)
**Number of Blueprints:** 0 (code plugin; the demo level contains no Blueprint logic)
**Number of C++ Classes:** 5 (actor component, world subsystem, Blueprint function library, developer
settings, demo director) plus the module
**Network Replicated:** No (server-side decision; replicate the verdict, not the check)
**Supported Development Platforms:** Windows
**Supported Target Build Platforms:** Windows
**Supported Engine Versions:** 5.8
**Third-party software:** none
**Documentation:** https://wiki.teufel-engineering.com/en/FireLane/documentation
**Support:** teufelsilvan@gmail.com
