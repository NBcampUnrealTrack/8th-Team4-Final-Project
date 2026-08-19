#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "CreditWalletComponent.generated.h"

UENUM(BlueprintType)
enum class ECreditChangeReason : uint8
{
	InitialGrant UMETA(DisplayName = "Initial Grant"),
	NewMatchReset UMETA(DisplayName = "New Match Reset"),
	RoundWin UMETA(DisplayName = "Round Win"),
	RoundLoss UMETA(DisplayName = "Round Loss"),
	Purchase UMETA(DisplayName = "Purchase")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnCreditsChangedSignature,
	int32, PreviousCredits,
	int32, NewCredits);

/**
 * 플레이어 한 명의 크레딧 잔액.
 * 잔액과 원자적 증감만 책임지며, 지급 시점과 보상 금액은 알지 않는다.
 */
UCLASS(ClassGroup = (Economy))
class FPS_API UCreditWalletComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCreditWalletComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Economy|Credits")
	int32 GetCredits() const { return Credits; }

	UFUNCTION(BlueprintPure, Category = "Economy|Credits")
	bool CanAfford(int32 Cost) const;

	bool CanAddCredits(int32 Amount) const;

	// 아래 변경 함수는 서버의 Economy/Shop 경계에서만 호출한다.
	bool ResetCredits(int32 NewCredits, ECreditChangeReason Reason);
	bool AddCredits(int32 Amount, ECreditChangeReason Reason);
	bool TrySpendCredits(int32 Cost, ECreditChangeReason Reason = ECreditChangeReason::Purchase);

private:
	UFUNCTION()
	void OnRep_Credits(int32 PreviousCredits);

	bool HasServerAuthority() const;
	void SetCreditsInternal(int32 NewCredits, ECreditChangeReason Reason);

private:
	UPROPERTY(ReplicatedUsing = OnRep_Credits, VisibleInstanceOnly, Category = "Economy|Credits")
	int32 Credits = 0;

public:
	UPROPERTY(BlueprintAssignable, Category = "Economy|Credits")
	FOnCreditsChangedSignature OnCreditsChanged;
};
