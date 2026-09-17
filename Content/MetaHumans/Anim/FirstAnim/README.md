# 1인칭 애니메이션 작업 위치

2026-09-17 정리. 콘텐츠 브라우저에서는 `Content > MetaHumans > Anim > FirstAnim`으로 들어오세요.

## 먼저 볼 폴더

```text
FirstAnim/
├─ Weapons/
│  ├─ AR/          # TR15
│  ├─ Pistol/      # WK-11 Viper
│  ├─ Shotgun/     # Herrington 11-87
│  └─ Sniper/      # SRM-12
│     ├─ Animations/  # 현재 스나이퍼 손 보정 클립
│     ├─ Montages/    # 발사/장전/Draw 등의 재생 구간
│     ├─ Settings/    # TSV_SRM-12: Idle·손 오프셋·구도
│     └─ BaseClips/   # 보정 전 복사본. 현재 손 보정은 Animations를 수정
├─ Common/         # 공통 Idle·이동·점프 등의 클립/몽타주
├─ Blueprints/     # 현재 총기용 AnimBP와 IK 레이어
├─ Components/     # 현재 리그 전환·포즈 설정·반동
├─ Rig/            # 팔 메시·스켈레톤·소켓·리타게터
├─ Integration/    # 기존 게임플레이와 애니메이션을 연결하는 컴포넌트/어빌리티
├─ Melee/          # 유지한 손/근접무기 애니메이션
├─ Fallback/       # 아직 다른 에셋이 참조하는 예전 경로용 자료
└─ Development/    # 이전 실험용 BP. 실제 수정은 위 폴더부터 확인
```

각 총기 폴더의 `Animations`는 동작 자체, `Montages`는 구간과 재생 속도, `Settings`는 무기별 포즈/구도 설정입니다. 이름 자체는 바꾸지 않고 위치와 참조만 갱신했습니다.

## 어떤 것을 수정하면 되나요?

| 목적 | 이 폴더 기준 위치 |
|---|---|
| 스나이퍼 왼손 동작 | `Weapons/Sniper/Animations/A_FP_SRM-12_*` |
| 스나이퍼 볼트 동작 타이밍 | `Weapons/Sniper/Montages/AM_FP_SRM-12_Fire` |
| 스나이퍼 빈 탄창 장전 | `Weapons/Sniper/Montages/AM_FP_SRM-12_Reload_Empty` |
| 총기별 Idle/구도 | `Weapons/<총기>/Settings/TSV_*` |
| 샷건 전체 장전 조합 | `Weapons/Shotgun/Montages/AM_FP_Herrington_11-87_Reload_*_Full` |
| 총과 손의 부착 기준 | `Rig/SK_TacticalMH_Arms`의 소켓 |
| 총기 애니메이션 흐름 | `Blueprints/ABP_TacticalShooter_UE5` |
| 총별 설정 선택 | `Components/BPC_TacticalPresentation` |
| 원본형 팔 리그/기존 리그 전환 | `Components/BPC_TacticalRigRouter` |
| 발사·장전 요청을 몽타주로 연결 | `Integration/Components/BP_AssetPresentationRelay` |

무기 모델·무기 BP는 기존 `Content/FPS/Weapon`에 있습니다. KINEMATION 원본 팩도 원래 위치에 유지했습니다. 이번 작업은 1인칭 정리이므로 3인칭 폴더는 옮기지 않았습니다.

`Fallback`에 남은 파일은 참조가 있어 안전하게 삭제할 수 없었던 것들입니다. 폴더 이름이 예전 자료라고 해서 직접 지우면 근접무기나 리그 복귀가 끊길 수 있습니다. 현재 총기 모션을 수정할 때는 `Weapons`, `Common`, `Blueprints`부터 확인하세요.

스나이퍼 소켓은 `VB ik_hand_gun_pivot` 기준이며 Mesh1P 크기는 1.0입니다. 위치만 정리했으므로 발사/볼트 1.85초, 장전 완료 3.2초와 기존 손 보정은 유지됩니다.

## 정리 결과와 복구 자료

- 이동 195개.
- 외부 참조가 없는 구형 1인칭 애니메이션 77개 삭제(39,251,195바이트, 약 37.4MiB).
- 참조가 남은 구형 애니메이션 50개 보존.
- 이전 위치의 참조 없는 팔 메시 잔여 파일 1개도 별도 백업 후 제거.
- 비어 있는 하위 폴더 38개 정리.
- 백업: `Saved/Codex/Backups/FirstPersonOrganization_20260917/`.
- 전체 이전→새 경로 및 삭제 목록: `Saved/Codex/FirstPersonOrganizationPlan.json`.
- 백업 파일 해시와 기존 참조 목록: 백업 폴더의 `manifest.json`.
- 삭제한 애니메이션은 백업의 `Content/`에서 원래 상대 경로로 복구할 수 있습니다. 백업 때문에 프로젝트 전체 디스크 용량이 즉시 줄어드는 것은 아닙니다.

전체 이동을 되돌릴 때는 에디터를 종료하고, 현재 상태를 따로 백업한 뒤 변경 전 백업 파일 292개를 원래 Content 경로로 복구해야 합니다. 이후 새로 생긴 195개 목적지 파일은 경로 목록에 따라 정리해야 합니다. 일부 파일만 되돌리면 참조가 섞일 수 있으므로 전체 복구는 한 묶음으로 진행하세요.

## 검증 범위

215개 패키지의 경로/참조 검사 통과. 이동한 블루프린트 로드와 컴파일 검사에서 새로운 오류가 없었습니다. 이전 버전부터 남아 있던 Fallback의 뼈 이름/몽타주 길이 경고는 이번 정리에서 수정하지 않았습니다.

세 캐릭터의 스나이퍼 격리 런타임 검사 각각 보고 기준 900프레임, 실패 0. 실제 테스트 맵의 모든 무기 입력/네트워크 플레이를 재검증한 것은 아닙니다.

임시 C++ 도구 제거 후 FPSEditor 최종 빌드 성공. 임시 도구 없는 새 프로세스에서도 215개 패키지 참조 검사 및 스나이퍼 소켓·크기·손 오프셋·몽타주·장전 시간 검사 모두 통과했습니다. 최종 로그는 `Saved/Logs/FirstPersonOrganizationHelperFreeFinal.log`입니다. 이번 작업에서 게임플레이 C++ 변경은 없으며, 작업 전의 WeaponBase.cpp 파일 해시도 그대로 유지했습니다.

이전 문서·작업 스크립트에 적힌 경로는 과거 기록일 수 있습니다. 현재 위치는 이 안내서와 `FirstPersonOrganizationPlan.json`을 기준으로 보세요. 스나이퍼 저장 상태 검사 스크립트 `Saved/Codex/verify_sniper_aligned_assets.py`는 새 경로로 갱신했습니다.
