// ShooterRoomTypes.h
#pragma once

#include "CoreMinimal.h"
#include "GameMode/Types/TeamTypes.h"
#include "ShooterRoomTypes.generated.h"

/**
 * 사설 방 화면 한 칸에 그릴 값
 */
USTRUCT(BlueprintType)
struct FShooterRoomSlotView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	int32 PlayerId = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	FString PlayerName;
	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	EGameTeam Team = EGameTeam::None;

	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	bool bIsHost = false;
	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	bool bIsReady = false;
	UPROPERTY(BlueprintReadOnly, Category = "Lobby")
	bool bIsLocal = false;
};

// 한 칸을 어떻게 갱신할지
UENUM(BlueprintType)
enum class EShooterRoomSlotPresentation : uint8
{
	// 같은 사람
	None,
	// 사람이 바뀌었거나 호스트 여부가 바뀜
	Snap,
	// 같은 사람의 레디 변화
	Animate
};

/**
 * 방 화면의 순수 규칙. 월드·위젯 없이 도는 정적 함수만 둠
 */
struct FShooterRoomRules
{
	static constexpr int32 MaxRoomNameLength = 24;

	// 입력이 비면 "{이름} 님의 게임". 기본 이름은 자르지 않음
	static FString SanitizeRoomName(const FString& Input, const FString& LocalPlayerName);

	// PlayerId 오름차순
	static TArray<FShooterRoomSlotView> GetTeamPlayers(const TArray<FShooterRoomSlotView>& AllViews, EGameTeam Team);

	// 칸 번호가 아니라 PlayerId 로 비교
	static EShooterRoomSlotPresentation DecideReadyPresentation(
		int32 CachedPlayerId,
		bool bCachedReady,
		bool bCachedIsHost,
		const FShooterRoomSlotView& NewView);
};

inline FString FShooterRoomRules::SanitizeRoomName(const FString& Input, const FString& LocalPlayerName)
{
	const FString Trimmed = Input.TrimStartAndEnd();
	if (!Trimmed.IsEmpty())
	{
		// 입력한 방 제목만 잘림
		return Trimmed.Left(MaxRoomNameLength);
	}

	const FString OwnerName = LocalPlayerName.TrimStartAndEnd();
	return FText::Format(
		NSLOCTEXT("ShooterLobby", "DefaultRoomName", "{0} 님의 게임"),
		OwnerName.IsEmpty()
			? NSLOCTEXT("ShooterLobby", "FallbackPlayerName", "플레이어")
			: FText::FromString(OwnerName)).ToString();
}

inline TArray<FShooterRoomSlotView> FShooterRoomRules::GetTeamPlayers(
	const TArray<FShooterRoomSlotView>& AllViews,
	const EGameTeam Team)
{
	TArray<FShooterRoomSlotView> Result = AllViews.FilterByPredicate(
		[Team](const FShooterRoomSlotView& View) { return View.Team == Team; });

	Result.Sort([](const FShooterRoomSlotView& Lhs, const FShooterRoomSlotView& Rhs)
	{
		return Lhs.PlayerId < Rhs.PlayerId;
	});

	return Result;
}

inline EShooterRoomSlotPresentation FShooterRoomRules::DecideReadyPresentation(
	const int32 CachedPlayerId,
	const bool bCachedReady,
	const bool bCachedIsHost,
	const FShooterRoomSlotView& NewView)
{
	// 사람이 바뀌었거나 호스트 여부가 바뀌면 칸 전체를 다시 그림
	if (NewView.PlayerId != CachedPlayerId || NewView.bIsHost != bCachedIsHost)
	{
		return EShooterRoomSlotPresentation::Snap;
	}

	return (NewView.bIsReady != bCachedReady)
		? EShooterRoomSlotPresentation::Animate
		: EShooterRoomSlotPresentation::None;
}
