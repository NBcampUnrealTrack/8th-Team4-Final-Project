// UITags.cpp
#include "Tags/UITags.h"

namespace UITags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Layer_Game, "UI.Layer.Game", "Player HUD. 항상 떠 있고 쌓지 않음");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Layer_GameMenu, "UI.Layer.GameMenu", "게임 중 띄우는 오버레이, HUD 위, Menu/Modal 아래로 스코어보드 사용");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Layer_Menu, "UI.Layer.Menu", "현재 보고있는 화면, 뒤로가기 스택 위함");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Layer_Modal, "UI.Layer.Modal", "현재 화면 위에 띄웠다 되돌아가는 팝업");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(UI_Layer_Transition, "UI.Layer.Transition", "모든 레이어를 덮는 전환 연출. 아래 화면이 준비된 채로 걷힌다");
}
