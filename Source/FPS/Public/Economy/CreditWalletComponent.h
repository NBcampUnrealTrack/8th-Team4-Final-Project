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


 // 플레이어의 크레딧 잔액을 관리한다.
 // 잔액 변경만 책임지며 보상 규칙과 지급 시점은 알지 않는다.

UCLASS(ClassGroup = (Economy))
class FPS_API UCreditWalletComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCreditWalletComponent();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Economy|Credits")
	int32 GetCredits() const { return Credits; }

	UFUNCTION(BlueprintPure, Category = "Economy|Credits")
	bool CanAfford(int32 Cost) const;

	bool CanAddCredits(int32 Amount) const;

	// 잔액 변경은 서버 권한을 가진 시스템에서만 수행한다.
	bool ResetCredits(int32 NewCredits, ECreditChangeReason Reason);
	bool AddCredits(int32 Amount, ECreditChangeReason Reason);
	bool TrySpendCredits(
		int32 Cost,
		ECreditChangeReason Reason = ECreditChangeReason::Purchase);

private:
	UFUNCTION()
	void OnRep_Credits(int32 PreviousCredits);

	bool HasServerAuthority() const;
	void SetCreditsInternal(int32 NewCredits, ECreditChangeReason Reason);

private:
	UPROPERTY(
		ReplicatedUsing = OnRep_Credits,
		VisibleInstanceOnly,
		Category = "Economy|Credits")
	int32 Credits = 0;

public:
	UPROPERTY(BlueprintAssignable, Category = "Economy|Credits")
	FOnCreditsChangedSignature OnCreditsChanged;
};