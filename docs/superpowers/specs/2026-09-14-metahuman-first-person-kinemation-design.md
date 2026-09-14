# MetaHuman First-Person KINEMATION Integration Design

## Goal

Display the existing `SKM_MetaHuman_Arms` mesh in first person while preserving the Tactical Shooter Pack's procedural idle, walk, sprint, jump, aiming, sway, recoil, and weapon-action animation behavior.

The first milestone targets the WK-11 Viper pistol. After that path is verified, the same integration can be extended to the remaining weapon sets.

## Existing Structure

- `BP_MHRanger.Mesh1P` displays `SKM_MetaHuman_Arms`.
- `Mesh1P` currently runs `MH_ABP_FirstPerson`, derived from `UShooterAnimInstance`.
- `UShooterAnimInstance` reads movement, aim, weapon, reload, and cycling state from `AShooterCharacter` and `UCombatComponent`.
- Tactical Shooter Pack animations target its UE5 Manny skeleton.
- `ABP_TacticalShooter_UE5` combines static/additive sequences with animation curves and procedural IK layers. Individual locomotion sequences are not complete visible poses by themselves.
- The project's `WeaponData` selects first-person idle, aim-idle, equip, reload, and fire assets per gameplay tag.

## Chosen Architecture

Use a hidden Manny skeletal mesh as a first-person animation driver and transfer its evaluated final pose to `SKM_MetaHuman_Arms` through a runtime IK Retargeter.

```text
AShooterCharacter / UCombatComponent
                |
                v
KINEMATION adapter values
                |
                v
Hidden Manny driver mesh
ABP_TacticalShooter_UE5-compatible AnimBP
                |
                v
Runtime IK Retargeter
Manny -> MetaHuman Arms
                |
                v
Visible BP_MHRanger.Mesh1P
MH_ABP_FirstPerson
```

This keeps the KINEMATION source assets unchanged, preserves curve-driven procedural motion, and allows later MetaHuman arm variants to reuse the same source driver.

## Assets and Components

### Source Driver

Add a dedicated skeletal mesh component to the shooter character or `BP_MHRanger`:

- Name: `KinemationDriver1P`
- Skeletal mesh: Tactical Shooter Pack UE5 Manny
- Animation class: a project-owned duplicate/adaptation of `ABP_TacticalShooter_UE5`
- Rendering: hidden, no shadows, no collision
- Animation ticking: Always Tick Pose and Refresh Bones

The driver remains separate from `Mesh1P`. It exists only to evaluate the source skeleton and procedural graph.

### Retarget Assets

Create project-owned assets outside the marketplace directory:

- Manny source IK Rig
- MetaHuman Arms target IK Rig
- Manny-to-MetaHuman-Arms IK Retargeter

The target rig maps the common root, pelvis, spine, clavicle, arm, hand, and finger chains that exist in `SKM_MetaHuman_Arms`. Retarget pose adjustments compensate for shoulder width, arm length, wrist rotation, and hand placement.

### Visible MetaHuman Arms

Duplicate `MH_ABP_FirstPerson` for the integration work. Its base pose comes from `Retarget Pose From Mesh`, sourcing `KinemationDriver1P`. Project-owned slots or final camera-space corrections may remain after the retarget node when they are still needed.

## State Adapter

The marketplace AnimBP must not depend directly on the demo character or demo weapon system. A project-owned adapter maps existing game state into the values needed by the KINEMATION graph.

Initial mappings:

| KINEMATION input | Project source |
| --- | --- |
| Velocity/direction | `AShooterCharacter::GetVelocity()` transformed to local space |
| Is In Air | `UCharacterMovementComponent::IsFalling()` |
| Aim pitch/yaw | control rotation minus actor rotation |
| Is Firing | `State.Combat.IsFiring` or the existing ranged-attack trigger |
| Is Aiming | `UCombatComponent::bAiming` |
| Reloading | `AWeaponBase::IsReloading()` |
| Weapon ready/cycling | existing `UCombatComponent` state |

The first milestone maps only the values required to demonstrate pistol idle, walk, sprint, jump, aim, and fire. Reload/equip integration follows after locomotion and retarget pose quality are verified.

## Weapon Actions

The current `WeaponData` remains authoritative for gameplay timing and weapon selection. KINEMATION animation playback is presentation-only and must not change fire rate, reload duration, ammo state, replication, or ability activation.

For the WK-11 Viper milestone:

- Idle and procedural locomotion run continuously on the driver.
- Fire is triggered from the existing successful ranged-attack path.
- Reload and equip continue to use existing gameplay state; their KINEMATION presentation is connected after the basic retarget test passes.

## Tick and Visibility Rules

- `KinemationDriver1P` must update even while not rendered.
- `Mesh1P` must evaluate after the driver so it receives the current-frame source pose.
- Establish an explicit tick prerequisite from `Mesh1P` to `KinemationDriver1P` if component attachment does not guarantee the order.
- The driver must never become visible in shipping gameplay.

## Failure Handling and Diagnostics

- If the target remains in reference pose, verify that the runtime retarget node has a valid source mesh and retargeter.
- If the pose updates one frame late, verify the component tick prerequisite.
- If shoulders, wrists, or hands are distorted, adjust the target retarget pose rather than editing marketplace animations.
- If locomotion states do not change, debug the adapted velocity and in-air values on the live animation instance.
- If procedural movement is absent while states change, verify curve propagation and the KINEMATION linked IK layers on the source driver.
- Missing MetaHuman-only bones remain controlled by the target reference pose or its post-process graph.

## Verification

### Milestone 1: Pose Transfer

- MetaHuman arms display the KINEMATION pistol idle pose.
- Source Manny remains invisible.
- No reference-pose snapping occurs during PIE.

### Milestone 2: Procedural Locomotion

- Walking, sprinting, and jumping produce the same weapon/camera motion as the Tactical Shooter demo.
- Jump start, loop, and end states respond to the live character movement state.

### Milestone 3: Combat

- ADS aligns the pistol sights.
- Successful fire triggers the expected KINEMATION presentation once per shot.
- Reload/equip presentation follows existing gameplay timing without changing ammo or ability behavior.

### Regression Checks

- Third-person `ABP_MetaHuman` behavior remains unchanged.
- Other character classes continue to use their existing meshes and animation blueprints.
- Multiplayer visibility rules still show only first-person arms to the owning player.
- Existing weapon gameplay and replication tests continue to pass.

## Non-Goals

- Replacing third-person locomotion in this phase.
- Editing marketplace source assets.
- Retargeting and baking every source sequence.
- Supporting every Tactical Shooter weapon before the WK-11 Viper path is verified.
