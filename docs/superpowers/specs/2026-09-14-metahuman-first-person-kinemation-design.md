# MetaHuman First-Person TR15 Integration Design

## Goal

Integrate the Tactical Shooter Pack TR15 presentation into the existing rifle gameplay without replacing gameplay authority. Ranger, Trooper, and Vanguard share `SKM_MetaHuman_Arms_Skeleton`, so one retargeted first-person arm set is reused by all three classes.

The integration reuses as much marketplace content as is practical: TR15 arm actions, weapon body animation, magazine animation, sights and attachments, action sounds where compatible, and the existing project ADS input/state.

## Verified Source Assets

- Character source skeleton: `SKM_Manny_Skeleton`.
- Target skeleton: `SKM_MetaHuman_Arms_Skeleton`.
- TR15 character sequences are ordinary non-additive bone animations and can be baked through an IK Retargeter.
- TR15 character actions: draw, holster, inspect, magazine check, tactical reload, empty reload, and two idle variants.
- TR15 has no standalone character fire, aim-idle, walk, or sprint action sequence.
- Locomotion and ADS in the marketplace demo are procedural; a plain sequence retarget does not reproduce them.
- The source `BP_TR15` assembles `SKM_TR15_Body`, `SKM_Mag_TR15`, front/rear sight components, and attachments. It uses `ABP_TR15` for the body and `ABP_Mag_TR15` for the magazine.

## Chosen Architecture

Keep `AWeaponBase`, GAS abilities, `UCombatComponent`, and `DA_WeaponData` authoritative. Add a presentation bridge that plays matching arm and weapon animations only after gameplay has accepted an action.

```text
Right mouse / Fire / Reload / Equip
                 |
                 v
Existing GAS + Combat gameplay state
                 |
       +---------+----------+
       |                    |
       v                    v
MetaHuman Mesh1P       Current Weapon Mesh1P
retargeted montage     TR15 weapon montage
       |                    |
       +---------+----------+
                 |
       synchronized timing
```

## MetaHuman Arm Assets

Create project-owned assets under `Content/MetaHumans/Anim/KINEMATION/TR15`:

- Source Manny IK Rig.
- Target MetaHuman Arms IK Rig.
- Manny-to-MetaHuman-Arms IK Retargeter.
- Retargeted TR15 idle pose/loop.
- Retargeted draw, tactical reload, empty reload, inspect, magazine-check, and holster sequences/montages.

Retarget chains cover root/pelvis, spine, clavicles, arms, hands, and fingers. Target pose corrections are made in the retargeter instead of changing marketplace source assets.

## TR15 Weapon Presentation

`BP_Weapon_AR` remains an `AWeaponBase` weapon. Its first-person visual composition mirrors the source `BP_TR15`:

- Body: `SKM_TR15_Body`, using `ABP_TR15`.
- Magazine: `SKM_Mag_TR15`, using `ABP_Mag_TR15`.
- Front and rear iron sights from the TR15 asset folder.
- Existing project muzzle/tracer/damage behavior remains untouched.

The weapon actor receives optional first-person presentation montages for equip, reload, and fire. Arm actions and weapon actions start from the same accepted gameplay event. The weapon montage never changes ammunition, RPM, hit detection, replication, or reload completion.

Tactical reload is the first reload variant because the current gameplay/DA has one reload slot. Empty reload support is retained as a follow-up data field only if the gameplay state exposes an empty-magazine branch cleanly.

## ADS / Right Mouse

Right mouse already activates `UGA_ADS` and sets `UCombatComponent::bAiming`. No new input binding is added.

TR15 does not contain a standalone ADS montage. ADS presentation therefore uses:

- The current `bAiming` transition in `MH_ABP_FirstPerson`.
- Existing MetaHuman `MH_FP_MM_RifleAim` initially, because it is already targeted to the shared arms skeleton.
- TR15 body and sight alignment through the first-person grip offset.
- Existing camera FOV interpolation from `UCombatComponent`.

After the action set is stable, the marketplace procedural IK/ADS layers can be adapted into `MH_ABP_FirstPerson` as a separate milestone. This prevents the action retarget from being blocked by a full procedural AnimBP port.

## Camera Animation

TR15 camera sequences use a dedicated camera skeleton and are not directly playable as character montages. The initial integration keeps the project's existing FOV and recoil camera behavior. Marketplace camera sequences are evaluated in a later isolated milestone, either by converting their transforms into project camera-animation assets or by using a hidden camera-animation driver. They must not be attached directly to the gameplay camera without blend-in/out and ADS cancellation handling.

## Data Ownership

- `DA_WeaponData.FirstPersonAnims[Weapon.Type.Rifle]` selects MetaHuman idle/aim/sprint assets.
- `DA_WeaponData.FirstPersonMontages[Weapon.Type.Rifle]` selects MetaHuman arm equip/reload/fire montages.
- `BP_Weapon_AR` owns TR15 visual components and weapon-body presentation montages.
- `AWeaponBase` exposes the small presentation API used by `UShooterAnimInstance`.

This keeps character-skeleton assets in the DA and weapon-skeleton assets on the weapon that owns those meshes.

## Known Existing Issue

`MH_FP_AM_Rifle_Fire` currently represents 0.167 seconds while its referenced sequence is 0.292 seconds. The segment must be updated and resaved or replaced before using it as the final fire montage.

## Verification

- Ranger, Trooper, and Vanguard reuse the same retargeted TR15 arm set.
- Equip and tactical reload start arm and weapon-body montages together.
- Weapon magazine motion stays synchronized during reload.
- Right mouse transitions to the existing MetaHuman aim pose and aligns the TR15 sights.
- Successful fire triggers the arm fire fallback and TR15 weapon fire montage once per accepted shot.
- Reload duration, ammunition completion, fire rate, damage, replication, first/third-person visibility, and third-person animation remain unchanged.
- Marketplace source assets remain unmodified.

## Deferred Scope

- Full KINEMATION procedural idle/walk/sprint/jump graph port.
- Converted skeletal-camera action animations.
- Inspect and magazine-check gameplay inputs.
- Empty-versus-tactical reload selection.
- Third-person TR15 presentation.
