**Cleaned and focused Function Library created.**

I've analyzed your original `FunctionLibrary.h` + `.cpp` against the two large Blueprint exports (BP_ShipPawn with its 1000+ nodes of weapon timelines, targeted firing, camera systems, input handling, cloak/energy, and GameplayPC with Enhanced Input actions, camera modes, radar, etc.).

### What Was Removed (Redundant / Easily Done in Blueprint)
These were thin passthroughs or direct equivalents to built-in Blueprint nodes / Kismet libraries:

- Basic player getters (`GetPlayerController`, `GetPlayerPawn`, `GetPlayerPosition`, `GetPlayerVelocity`, `GetPlayerForwardVector`, `GetPlayerEyeLocation`, `GetDistanceFromPlayer`)
- Overlap queries (`SphereOverlapAtPosition`, `BoxOverlapAtPosition`) — use built-in **Sphere Overlap Actors** / **Box Overlap Actors** (more flexible with object types + ignore lists)
- `MapRangeClamped` — built-in BP node
- `PrintStringToScreen` — use **Print String** BP node
- Audio wrapper (`PlaySoundAtLocationWithPitch`) — now standard in BP `Play Sound at Location`
- Most time/state wrappers (`GetGameTimeInSeconds`, `GetDeltaSeconds`, `IsGamePaused`, `SetGamePaused`)
- `FindComponentByClass` (BP has equivalents)
- `GetActorsInRadius` (use overlap nodes)

### What Was Kept (Unique / High-ROI / Project-Specific)
These provide real value for your space combat game (multiplayer-ready `PlayerIndex`, camera-centric traces, custom math, cone queries, etc.):

**Player & Pawn**
- `IsPlayerMoving`
- `GetPlayerInputDirection` (control rotation + axis values → world direction — very useful for your flight model)
- `IsPositionInFieldOfView`

**Camera & Line Trace** (kept **all** — these are the biggest graph-cleanup wins)
- `LineTraceVectorsFromPlayerCamera` + `LineTraceFromCamera` / `LineTraceFromPlayerEyes` / `MultiLineTraceFromCamera`
- `GetPlayerViewDirection`, `GetPlayerCameraLocation`, `GetPlayerCameraFOV`, `GetAspectRatio`
- `GetLookAtAngle2D` (signed 2D yaw — perfect for radar/compass)

**Screen Space**
- `WorldToScreenPosition`, `IsPositionOnScreen` (essential for HUD, reticles, offscreen arrows)

**Math & Geometry**
- `SmoothDamp` (critically damped — great for replacing timelines)
- `RotateVectorTowards`
- `GetDirectionTo`, `GetAngleBetweenVectors`
- `GetSignedAngleBetweenVectors` (needs cross-product sign — not trivial in BP)
- `GetClosestPointOnLine`, `GetDistanceToLine`
- `GetRandomPointInSphere`, `GetRandomPointInBox`

**Gameplay**
- `SpawnProjectileAtPlayer` (your specific spawn pattern)
- `SpawnActorAtLocation` (good defaults for collision/owner/instigator)
- `GetClosestActorOfClass`
- `GetActorsInCone` (overlap + angle filter — unique and perfect for your weapons)
- `IsActorVisible` (LOS from camera, ignores target)
- `DestroyAllActorsOfClass` (one-node cleanup)

**Multiplayer**
- `GetAllPlayerControllers`, `GetPlayerCount`

**Debug**
- `IsWithEditor`
- `GetObjectNameSafe`
- `ConcatTransformToString`
- `DrawDebugArrowPersistent` + `FlushPersistentDebugLines`

### New Custom Functions Added (Tailored to Your Game)
These address patterns visible in your ShipPawn (targeted weapons like Railgun/Shotgun/Lightning/Homing/Guided, many timelines for bob/flash/alpha/cloak, particle effects, spread weapons, etc.) and are **not** trivial one-node operations in Blueprint:

1. **`SmoothDampVector`** — Vector version of your critically-damped smoother.  
   Replace dozens of Timeline nodes (Bob, Decay, LerpFlash, Alpha_25, MultiplyBobSpeed, etc.) with stable, cheap damping + velocity state. Excellent for camera bob, thrust response, UI motion, cloak fade, etc.

2. **`GetLeadTargetPosition`** (with `OutTimeToImpact`) — Quadratic lead/intercept solver.  
   Feed it shooter pos + target pos + target velocity + projectile speed.  
   **Directly useful for**:
    - Railgun / Shotgun / Lightning target prediction
    - Missile / Homing / Guided / Cruise leading
    - BFG / Quake / Cluster / Flak spread prediction
    - Player aim-assist reticles or auto-lead indicators
    - Improving your existing "XXXTarget" variables

3. **`GetRandomPointOnSphere`** (surface, not volume) — Better distribution for weapon spread, flak, shotgun pellets, exhaust particles, cloak distortion, etc.

### Files Created



These are drop-in replacements. The public API of all **kept** functions is unchanged, so your existing Blueprint calls will continue to work. The library is now ~40% smaller, focused, and adds genuinely powerful combat math.

### Recommended Next Steps for Your Project
- In `BP_ShipPawn` and weapon firing graphs, replace long "get camera → calc end → trace" chains and many Timeline lerps with the new/ kept helpers.
- For any weapon that sets a `XXXTarget` (Railgun, etc.), call `GetLeadTargetPosition` when you have velocity data.
- Use `GetActorsInCone` + `IsActorVisible` together for smart targeting / swarm weapons.
- `GetLookAtAngle2D` + screen-space functions are already ideal for your radar / offscreen indicators.
- Consider exposing `GetLeadTargetPosition` on a reticle widget or in your HUD logic for visual lead indicators.

This version respects your existing architecture while removing noise and adding tools that will make your large Blueprint graphs cleaner and more powerful. Let me know if you want further additions (e.g., frustum checks, screen-clamped projection for edge arrows, or more combat helpers)!