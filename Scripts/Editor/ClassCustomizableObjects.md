# Trooper / Vanguard Mutable 소스

> 2026-09-23 정리: 아래 생성·검증 명령과 Python 스크립트는 작업 이력이며 현재는 제거되었다.
> 생성된 CO·재질·설정 에셋과 런타임 커스터마이징 코드는 유지한다. 도구 복원이 필요하면 Git 이력을 사용한다.

## 2026-09-22 클래스 선택 업데이트 (아래 최초 생성 기록보다 우선)

- Trooper: `MHC_Frey / MHC_Gavin / Mason`.
- Vanguard: `MHC_Crowd80 / Dean / MHC_Shrank`.
- 두 CO의 BodyType은 이제 위 세 이름이며 `/Game/MetaHumans/<이름>`의 조립된 Body/Face/Clothing을 사용한다.
- MutableLevel의 클래스 버튼으로 전환한다. 외형은 1/2/3, 클래스 내부 공유 머리는 X/1/2/3이다.
- `DA_StudioTrooper`, `DA_StudioVanguard`: 클래스당 9개 Hair 조합 및 외형별 고정 눈썹/속눈썹/잔털/수염.
- 새 의상은 원본 재질을 유지하며 이전 청바지/스카프 색상 그룹은 더 이상 노출하지 않는다. 해당 색상 버튼은 비활성화한다. 피부/눈/헤어 색상 및 Ranger 기존 의상 색상은 유지한다.
- 클래스별 미저장 Draft는 세션 동안 보존하고, 계정 변경 시 삭제한다. 저장은 기존 버전 1 계정+클래스 슬롯을 사용한다. 게임 BP/매치 적용은 변경하지 않는다.
- `verify_class_customizable_objects.py`는 이제 `FPS.ClassVariants.Verify`로 여섯 외형을 생성해 검사한다.
- `verify_class_studios.py`는 9개 헤어 조합과 고정 Groom 원본 일치를 검사한다.
- `FPS.Customization.Classes`는 저장 격리, Draft 전환, 계정 변경, 36개 실제 프리뷰 조합을 검사한다.
- 재생성은 `build_class_variants.py`, 설정/레벨 연결은 `configure_class_studios.py`다. 두 CO를 재작성하므로 백업 후 명시적으로 실행한다.
- 자동 검사는 실제 화면의 미적 판단을 대신하지 않는다.
- 남은 시각 검증 문제: Trooper/Frey의 `Middle_part`는 실제 화면에서 보이지 않는다. CO/바인딩 연결과 렌더 루트 데이터는 유효하지만, DX12/SM5·LOD0·바인딩 제외 비교에서도 동일했다. DX11 문제로 단정할 수 없으며 원본 Groom/재질을 변경하지 않은 상태다. 36개 조합 검증 성공은 모든 헤어의 실제 렌더링 성공을 뜻하지 않는다.

## 최초 생성 당시 기록 (아래 Base/의상 색상 설명은 이전 상태)

## 추가된 에셋

- `/Game/FPS/Customizable/CO_Trooper`: Mason 기본 Body / Face / Outfit.
- `/Game/FPS/Customizable/CO_Vanguard`: Shrank 기본 Body / Face / Outfit.
- 각 CO의 `BodyType` 선택지는 현재 `Base` 하나다. 추가 체형은 아직 만들지 않았다.
- Ranger CO, 캐릭터 BP, MutableLevel의 설정·UI·저장 형식은 변경하지 않았다.
- 헤어·그룸 및 실제 게임 캐릭터 연결은 이번 생성 범위에 포함하지 않는다.

## 에디터에서 확인

콘텐츠 브라우저의 `FPS/Customizable`에서 CO를 열고 Compile 후 미리보기를 확인한다.
Body / Face / Outfit 컴포넌트를 사용한다. Face의 LOD별 재질 슬롯이 달라지므로
각 원본 LOD의 섹션을 명시적으로 연결했다.

공통으로 `SkinColor`, `SkinValue`, `EyeColor`를 제공한다.
현재 의상의 실제 재질 구분에 맞춰 아래 색상 그룹만 노출한다.

| 클래스 | 의상 색상 그룹 |
|---|---|
| Trooper | `Bottom` — 청바지 |
| Vanguard | `Scarf` — 머리 덮개, `Top` — 옷 전체, `Boots` — 신발 |

각 그룹은 `Color`, `Value`, `UseOriginal` 접미사를 사용한다.
`UseOriginal=1`이면 원래 색, `0`이면 선택 색상이다. `Value` 기본값은 1이다.
Vanguard의 옷은 원본이 하나의 재질이므로 상·하의를 따로 칠하는 기능은 없다.
전용 색상 재질은 `FPS/Customizable/Character/Materials/<클래스>`에 있으며 원본 재질은 수정하지 않았다.

## 나중에 체형 두 개 추가하기

1. 새 체형의 Body / Face / Outfit 메시와 재질을 준비한다.
2. 해당 CO의 `BodyType` Enum Parameter에 새 이름 두 개를 추가한다.
3. 각 컴포넌트와 LOD의 Material Switch에 해당 체형의 메시·재질 분기를 연결한다.
4. 체형별 모든 LOD를 생성해 얼굴/의상 누락 및 피부·눈·의상 색상을 확인한다.
5. 헤어 바인딩, 프리뷰 설정, 저장 값 및 인게임 연결은 이후 별도 통합한다.

## 재현 및 검증

`FPS.ClassCO.Create`는 최초 생성용 에디터 콘솔 명령이다. 대상 CO 또는 전용 재질 패키지가
이미 있으면 덮어쓰지 않고 중단한다. 평소 편집/검증에 다시 실행할 필요는 없다.

FPSEditor 빌드 후 `Scripts/Editor/verify_class_customizable_objects.py`를
Unreal Python commandlet으로 실행하면 존재 여부, 소스 연결, CO 컴파일,
Body/Face/Outfit 메시 및 LOD, 생성 재질의 색상 변경과 원본 색상 복귀를 확인한다.
최종 성공 표시는 `CLASS_CO_ALL_VERIFIED`다. 실패는 commandlet 오류로 전달한다.

렌더링 재질 컴파일도 확인하려면 `-AllowCommandletRendering -RenderOffscreen`을 사용한다.
이 자동 검사는 실제 화면에서의 미적 판단이나 게임 통합 테스트를 대신하지 않는다.

## 클래스 전환 시 헤어 렌더링 주의

MutableLevel의 이전 Ranger 설정은 Groom 컴포넌트에 `Use Cards`를 강제했다.
카드가 없는 Frey의 `Middle_part`로 전환하면 바인딩이 유효해도 머리가 표시되지 않았다.
현재 `ApplyGrooms`는 선택한 Groom에 카드 메시/LOD가 있을 때만 카드를 사용하고,
그 외에는 원래 스트랜드를 사용한다. 원본 Groom·재질·바인딩은 수정하지 않는다.
클래스/헤어 변경마다 이 설정을 갱신하므로 기존 저장 레벨에도 코드 수정이 적용된다.

## 클래스·체형 전환 시 얼굴 재질

MetaHuman 얼굴의 후처리 애니메이션은 컴포넌트에 동적 재질을 만든다.
캐릭터마다 재질 슬롯 순서가 달라, 메시만 교체하면 이전 치아·숨김 재질이
새 눈이나 피부 슬롯에 남을 수 있다. 미리보기 갱신 시 이전 재질 오버라이드를
먼저 비우고 메시를 교체한 다음, 현재 Mutable 인스턴스의 재질을 적용한다.
캐시된 메시의 기본 재질만 사용하는 것도 피한다. 공유 원본 메시·재질은 수정하지 않는다.

`FPS.MutableLevel.ClassScreenshots`를 `-ClassRoundTrip`과 함께 실행하면
얼굴 보기에서 Ranger → Trooper → Vanguard → Ranger → 외형 3을 캡처한다.
`FPS.Customization.Classes.PreviewCombinations`는 메시/바인딩뿐 아니라
현재 인스턴스의 재질이 각 슬롯에 적용됐는지도 검사한다.

## 정적 미리보기의 얼굴 포즈

`bUseReferencePoseGrooms`를 사용하는 정적 미리보기는 얼굴의 Post Process
Animation Blueprint를 컴포넌트에서 비활성화한다. Mutable 생성 얼굴에 적용된
후처리 때문에 Mason·Shrank의 눈꺼풀·입·턱이 변형되는 현상을 분리 재현했다.
후처리를 끈 생성 얼굴은 동일 조건의 원본 얼굴 기준 포즈와 시각적으로 일치했다.
이 처리는 실제 게임 캐릭터나 원본 Skeletal Mesh/AnimBP를 수정하지 않는다.
향후 표정 애니메이션을 지원할 때는 캐릭터별 얼굴 데이터 연결을 별도로 검증해야 한다.
Mason 목·가슴 경계 및 모든 헤어 바인딩의 외형 검증까지 해결한 것은 아니다.

## 2026-09-22: 현재 3체형별 눈·의상 색상 연결

위 최초 생성용 의상 표 대신 현재 조립된 세 체형에는 다음 그룹을 사용한다.

| 클래스 | 활성 의상 그룹 |
|---|---|
| Trooper (Frey/Gavin/Mason) | Top, Bottom, Vest, Boots |
| Vanguard (Crowd80/Dean/Shrank) | Top, Bottom, Attachment, Boots |

Vanguard는 같은 원본 재질을 공유해도 메시 섹션이 분리되어 있어 하의·부착물·부츠를
각각 제어한다. 원본 메시/재질은 그대로 두고 클래스·부위·원본 재질별 전용 복사본을 만든다.
의상 원본 색 복원은 `UseOriginal=1`, 선택 색 적용은 `0`이다.

새 체형의 베이크 눈 재질은 `Iris Color Multiply`만 연결해도 화면 색상이 바뀌지 않았다.
각 체형의 좌우 눈 MIC를 `Character/Materials/ClassEyes`에 복제하고
`Use Baked Material=false`로 설정하여 Ranger와 같은 색상 조절 경로를 사용한다.
Ranger CO와 원본 MetaHuman 재질은 변경하지 않는다.

재실행 순서: 에디터 종료 및 두 CO/설정 백업 → FPSEditor 빌드 →
`apply_class_color_controls.py` (모든 체형 분기 재생성 포함) →
`verify_class_color_controls.py` → `FPS.Customization` 자동 검사.
`ClassVariants.Build`도 이제 색상 연결을 재생성하며, 전용 눈 MIC가 먼저 존재해야 한다.
`-ClassColors -ExecCmds="FPS.MutableLevel.ClassScreenshots"`로 아홉 외형의
녹색 눈·적색/청색 의상을 얼굴/전신 보기에서 캡처한다. 저장 프로필은 수정하지 않는다.

## 2026-09-23: 체형별 얼굴 카메라와 위치 이동

얼굴 보기는 고정 높이 대신 현재 얼굴의 좌우 눈 본 중점을 기준으로 정렬한다.
Mutable 생성과 바닥 정렬 완료 뒤에도 카메라를 갱신하므로 체형 전환 시 새 얼굴을 따른다.
좌클릭 드래그는 회전, 우클릭 드래그는 화면 기준 상하좌우 이동, 휠은 확대/축소다.
전신/얼굴 보기 버튼은 수동 이동 및 확대 거리를 초기화한다. 카메라 조작은 외형 저장에 포함되지 않는다.
`FPS.Customization.Camera.FramingAndPan`과 기존 36조합 검사로 높이·확대·이동·초기화를 확인한다.
`-CameraFraming -ExecCmds="FPS.MutableLevel.ClassScreenshots"`는 9외형의 기본 얼굴 보기/확대 화면을 캡처한다.
