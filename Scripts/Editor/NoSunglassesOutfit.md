# Outfit 64 — 선글라스 미착용 소스

> 2026-09-23 정리: 아래 에디터 생성·진단·검증 명령은 제거되었으며 제작 기록으로 남긴다.
> 생성된 의상과 캐릭터 에셋은 유지한다. 도구는 Git 이력에서 복구할 수 있다.

이번 단계는 MetaHuman Character 의상 적용이다. Mutable CO의 착용 토글이나 게임 UI는 아직 연결하지 않았다.

## 사용 에셋

`/Game/Fab/Mercenary/NoSunglasses/WI_OA_outfit_64_NoSunglasses`

- `MHC_Crowd80`, `MHC_Shrank`, `Dean`의 Outfits 선택에 적용했다.
- 원본 OA/WI, 원본 Cloth Assets, 선글라스 메시·재질은 변경하지 않는다.
- 셔츠·바지·가방·부츠·목걸이는 유지하고 Frame/Frame1 렌더 패턴만 제외한다.
- A/B 크기는 동일한 Outfit GUID를 공유하는 두 사이즈다.
- 원본 A Cloth에는 이미 안경이 없고 B에는 남아 있었다. 양쪽 모두 5패턴, 22,583 렌더 정점으로 맞춘다.

## 생성된 데이터의 편집

CA 복사본은 원본 Dataflow를 재실행해 선글라스가 되살아나지 않도록 연결을 비운
베이크된 파생 에셋이다. OA는 해당 CA와 원본 source body를 이용해 사이즈 보간 데이터를 생성한다.
원본 그래프를 편집한 후 자동으로 갱신되는 연결이 아니므로, 원본 의상이 바뀌면 이 파생본도 다시 생성해야 한다.
캐릭터 자체의 얼굴·체형·헤어를 새로 만들거나 Assembly 결과 BP를 덮어쓰지 않는다.

## 이후 Mutable 연결

기본 의상은 이 미착용 소스를 사용한다. 보존된 `SKM_Sunglasses_a/b`를
체형에 맞춰 별도 컴포넌트/메시 분기로 추가하고 `Sunglasses` 선택으로 표시한다.
기본값은 미착용으로 한다. 얼굴 크기에 따른 선글라스 맞춤과 최종 LOD도 별도 검증한다.

## 검증

- `Scripts/Editor/verify_no_sunglasses_outfit.py`: 별도 WI, 올바른 파이프라인,
  프레임/렌즈 제거, 나머지 의상 정점 보존, 3명의 저장된 의상 선택을 검사.
- `FPS.Wardrobe.PreviewNoSunglasses <캐릭터 이름>`: 실제 MetaHuman 프리뷰 빌드에서
  맞춤 의상 메시가 생성되는지 검사. 에셋은 저장하지 않는다. 각 캐릭터는 별도
  프로세스에서 검사한다. 한 프로세스에서 연속 초기화하면 엔진의 슬롯 초기화 오류가
  발생할 수 있다. Preview는 Outfit, Production은 OutfitMesh 결과를 사용한다.
- 생성 명령은 대상 패키지가 있으면 덮어쓰지 않는다.
- 복구용 캐릭터 백업: `Saved/Codex/Backups/BeforeNoSunglasses_20260922/`.

Body Hidden Face Map은 임의로 넣지 않았다. 의상 경계의 피부 관통은 실제 화면에서 확인 후
해당 몸 UV에 맞는 마스크로 처리해야 한다. 머리 숨김 마스크는 넣지 않는다.

## 2026-09-22 확인 결과

- FPSEditor Development 빌드 성공.
- 저장본 검사: 실패 0. 세 캐릭터 모두 신규 WI 선택 확인.
- 캐릭터별 독립 프리뷰 검사: 세 명 모두 맞춤 Outfit 생성 및 22,583 정점 확인.
- 화면 캡처에 의한 관통/목걸이 위치의 최종 시각 검사는 별도 필요.
- 전체 FPS 자동 검사는 기존 `FPS.Shop.Catalog.InvalidConfigurationFailsClosed`에서
  배열 내부 요소를 같은 배열에 추가하는 assertion으로 중단되어 전체 통과는 확인하지 못했다.
  이번 의상 작업에서 상점 코드는 변경하지 않았다.
