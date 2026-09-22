# 클래스 내부 머리카락 공유 바인딩

생성 위치: `/Game/FPS/Customizable/Character/HairBindings/Trooper`, `Vanguard`.
이름 `GB_원래캐릭터_To_대상캐릭터`는 원래 캐릭터의 Hair Groom을 대상 얼굴에 사용하는 바인딩이다.

| 클래스 | 원래 캐릭터 | 사용할 Groom |
|---|---|---|
| Trooper | MHC_Frey | /Game/MetaHumans/MHC_Frey/Grooms/Middle_part |
| Trooper | MHC_Gavin | /Game/MetaHumans/MHC_Gavin/Grooms/GA_Gavin_Hair |
| Trooper | Mason | /Game/MetaHumans/Mason/Grooms/Hair_S_CurlyFade |
| Vanguard | MHC_Crowd80 | /Game/MetaHumans/MHC_Crowd80/Grooms/Hair_S_Casual |
| Vanguard | Dean | /Game/MetaHumans/Dean/Grooms/Hair_S_CoilBuzzCut |
| Vanguard | MHC_Shrank | /Game/MetaHumans/MHC_Shrank/Grooms/Hair_S_BuzzCut |

각 Groom에 대해 같은 클래스의 다른 얼굴 2개용 바인딩을 추가했다(총 12개).
자기 얼굴용은 기존 Assembly 바인딩을 사용한다. 눈썹, 속눈썹, 수염, 잔털과 Ranger는 수정하지 않았다.
Source Mesh는 Groom이 이미 맞춰진 원래 캐릭터의 Assembly Face Mesh이며 Target Mesh는 대상 Assembly Face Mesh다.
헤어를 교체할 때 Groom과 Binding을 함께 선택해야 한다. 다른 복사본 Groom과 혼용하지 않는다.
Mutable CO/UI 또는 캐릭터 BP에 자동 교체 연결을 추가한 작업은 아니다.

2026-09-22: 저장본 12개를 새 프로세스에서 읽어 Groom/Source/Target 및 비어 있지 않은 Guide 데이터를 검증, 오류와 경고 0.
화면에서 헤어라인과 움직임의 최종 시각 확인은 별도 필요하다.
전체 FPS 자동 검사는 기존 FPS.Shop.Catalog.InvalidConfigurationFailsClosed (ShopTests.cpp:255)의 배열 assertion으로 중단됐다.
