#pragma once

#include "CoreMinimal.h"

namespace CharacterPresentationTags
{
	// 클래스 선택용으로 별도 생성된 비복제 Pawn만 사용한다. 플레이용 Pawn에는 지정하지 않는다.
	inline const FName ClassSelectionPreview(TEXT("ClassSelectionPreview"));
}
