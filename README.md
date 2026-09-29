# FireLane — Friendly Fire & Line-of-Fire Gate

**Unreal Engine 5.8 · Win64 · one runtime C++ module · Blueprint-first · full source**

Is one of my own people standing in that shot?

Every AI shooter with allies needs the answer and almost all of them get it from a single line trace,
which is wrong three ways at once: a bullet has width, a team-mate walks, and the answer arrives one
frame late.

FireLane sweeps a lane of the projectile's real danger radius from muzzle to target, projects every
friendly forward along its own velocity so the shot is judged against where people **will be**, and
separates a hard block from a graze so the AI is neither reckless nor mute.

When the lane is blocked it does not just say no. It returns the sideways step that would clear it,
and how long the blocker needs to walk out on his own — so the squad moves instead of standing there.

## Install in five minutes

1. Add a **Fire Lane** component to anything that shoots. Set its **Team Id**.
2. Register the player pawn with **Register Friendly**.
3. Call **Can Fire (Target)** before you pull the trigger.

## What is in the box

* `UFireLaneComponent` — the gate, with muzzle socket, team id and per-weapon overrides
* `UFireLaneSubsystem` — the registry of who counts as ours, and the per-frame trace budget
* `UFireLaneStatics` — the rules as pure functions, usable from EQS, a decorator or an ability
* `UFireLaneSettings` — Project Settings → Plugins → FireLane
* A demo level that runs **without pressing play**
* Five automation tests over the rules that fail quietly

## Documentation

https://wiki.teufel-engineering.com/en/FireLane/documentation

## Support

teufelsilvan@gmail.com

Copyright 2026 Silvan Teufel. All Rights Reserved.

<!-- SF-STORE-BLOCK:BEGIN -->
## 🛒 Source-available — see before you buy

This repository contains the **full source** of a commercial Unreal Engine plugin. It is **source-available, not open source**: read it, evaluate it, then buy a license to use it. See **the Fab Content License Agreement / Unreal Engine EULA (purchase required)**.

**Get it / Buy:**
- **Buy on Fab** (this plugin): https://www.fab.com/listings/f7750967-218a-40c1-bb86-44b59f65c02c
- Fab store — all our UE5 plugins: https://www.fab.com/sellers/Silvan%20Teufel

### 📬 **Free UE5 Snippet-Pack**

10 ready-to-use C++/Blueprint building blocks (subsystems, versioned saves, async nodes, editor tooling) — MIT licensed. Get it by joining the newsletter — plus a heads-up when something new ships. Double opt-in, unsubscribe in one click, no address sharing.

👉 **[Get the free pack](https://silvan.teufel-engineering.com/newsletter/plugins/?q=gh)**

_© 2026 Silvan Teufel. All rights reserved._
<!-- SF-STORE-BLOCK:END -->
