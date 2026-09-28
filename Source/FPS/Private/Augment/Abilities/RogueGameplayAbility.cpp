// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/Abilities/RogueGameplayAbility.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "Augment/AugmentDefinition.h"
#include "Combat/CombatComponent.h"
#include "Engine/World.h"
#include "GAS/Damage/RogueDamageSubsystem.h"
#include "GAS/Damage/RogueGameplayEffectContext.h"
#include "Augment/RogueAugmentStatResolver.h"
#include "GAS/RogueAugmentAttributeSet.h"
#include "GAS/RogueGASSettings.h"
#include "GAS/RogueTargetFilter.h"
#include "Tags/GameplayTags.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAugmentAbility, Log, All);

namespace
{
	// 설정에 등록된 GE 애셋을 로드 (쿨타임/지속 시간)
	UClass* LoadConfiguredEffectClass(const TSoftClassPtr<UGameplayEffect>& SoftClass, const TCHAR* FieldName)
	{
		UClass* Loaded = SoftClass.LoadSynchronous();
		if (!Loaded)
		{
			UE_LOG(LogAugmentAbility, Error,
				TEXT("[AugmentAbility] %s 를 로드하지 못했습니다. DefaultGame.ini 의 [/Script/FPS.RogueGASSettings] 등록과 ")
				TEXT("애셋 경로를 확인하세요 (Duration Policy = Has Duration, Duration = SetByCaller %s)"),
				FieldName, *RogueTags::Data_Magnitude_Duration.GetTag().ToString());
		}

		return Loaded;
	}
}

URogueGameplayAbility::URogueGameplayAbility()
{
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RogueTags::Augment_Ability);
	SetAssetTags(AssetTags);
}

UClass* URogueGameplayAbility::GetCooldownEffectClass()
{
	return LoadConfiguredEffectClass(GetDefault<URogueGASSettings>()->CooldownEffectClass, TEXT("CooldownEffectClass"));
}

UClass* URogueGameplayAbility::GetStatusEffectClass()
{
	return LoadConfiguredEffectClass(GetDefault<URogueGASSettings>()->StatusEffectClass, TEXT("StatusEffectClass"));
}

void URogueGameplayAbility::CommitExecute(const FGameplayAbilitySpecHandle Handle,
                                          const FGameplayAbilityActorInfo* ActorInfo,
                                          const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::CommitExecute(Handle, ActorInfo, ActivationInfo);

	if (!bBroadcastSkillUsed)
	{
		return;
	}

	// 이벤트 발신은 서버에서만
	if (!HasAuthority(&ActivationInfo))
	{
		return;
	}

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!Avatar)
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag   = RogueTags::Event_Skill_Used;
	Payload.Instigator = Avatar;
	Payload.Target     = Avatar;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Avatar, RogueTags::Event_Skill_Used, Payload);
}

bool URogueGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                               const FGameplayAbilityActorInfo* ActorInfo,
                                               const FGameplayTagContainer* SourceTags,
                                               const FGameplayTagContainer* TargetTags,
                                               FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 이벤트 기반 발동은 액션 블록 상태를 무시
	if (!bBroadcastSkillUsed || bIgnoreActionBlock)
	{
		return true;
	}

	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC)
	{
		return true;
	}

	if (ASC->HasMatchingGameplayTag(RogueTags::State_Combat_ActionBlocked))
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(RogueTags::State_Combat_ActionBlocked);
		}

		return false;
	}

	return true;
}

void URogueGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                            const FGameplayAbilityActorInfo* ActorInfo,
                                            const FGameplayAbilityActivationInfo ActivationInfo,
                                            const FGameplayEventData* TriggerEventData)
{
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	UAbilitySystemComponent* SourceASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	if (!Avatar || !SourceASC)
	{
		EndSelf(true);
		return;
	}

	const FAugmentGrantedAbility* Entry = FindGrantedEntry(Handle, ActorInfo);
	if (!Entry)
	{
		UE_LOG(LogAugmentAbility, Error,
			TEXT("[AugmentAbility] %s 를 부여한 증강 정의에서 자기 GrantedAbilities 항목을 찾지 못했습니다. ")
			TEXT("PDA 의 AbilityClass 가 이 클래스를 가리키는지 확인하세요"), *GetName());

		EndSelf(true);
		return;
	}

	// 무기 조건이 안 맞으면 발동 X
	if (!Entry->RequiredWeaponTags.IsEmpty()
		&& !ResolveEventWeaponTag(Avatar, TriggerEventData).MatchesAny(Entry->RequiredWeaponTags))
	{
		EndSelf(false);
		return;
	}

	if (!PassesDistanceCondition(SourceASC, TriggerEventData, *Entry))
	{
		EndSelf(false);
		return;
	}

	// 액티브 증강은 대상이 아직 결정되지 않음
	AActor* EventTarget = nullptr;
	if (TriggerEventData)
	{
		EventTarget = ResolveEventTarget(Avatar, TriggerEventData, *Entry);

		const bool bNeedsEventTarget = Entry->bApplyToEventTarget
			|| (Entry->TargetScope == ERogueTargetScope::Radius && Entry->bAreaCentersOnEventTarget);

		if (!EventTarget && bNeedsEventTarget)
		{
			EndSelf(false);
			return;
		}
	}

	ActiveEntry = Entry;

	// 손에 드는 증강은 사용 입력을 기다림
	if (bHoldToUse)
	{
		BeginHold();
		return;
	}

	UsedInputTag = RogueTags::Event_Utility_Input_Primary;

	ExecuteAbility(EventTarget);
}

void URogueGameplayAbility::ExecuteAbility(AActor* EventTarget)
{
	AActor* Avatar = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;

	if (!Avatar || !ActiveEntry)
	{
		EndSelf(true);
		return;
	}

	if (!BuildEffectPayload(*ActiveEntry, Avatar, EventTarget,
			CurrentSpecHandle, CurrentActorInfo, ActivePayload))
	{
		EndSelf(true);
		return;
	}

	// 조건을 다 통과한 뒤에만 쿨타임/코스트를 소모
	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		EndSelf(true);
		return;
	}

	ExecuteCue(ActivationCueTag);

	// 시전 지연이 있으면 대기 후 발동, 없으면 바로 발동
	if (CastDelaySeconds > 0.f)
	{
		if (UAbilityTask_WaitDelay* Task = UAbilityTask_WaitDelay::WaitDelay(this, CastDelaySeconds))
		{
			if (TelegraphCueTag.IsValid())
			{
				K2_AddGameplayCueWithParams(TelegraphCueTag, MakeCueParams(), /*bRemoveOnAbilityEnd*/ true);
			}

			Task->OnFinish.AddDynamic(this, &ThisClass::FinishDeliver);
			Task->ReadyForActivation();
			return;
		}
	}

	FinishDeliver();
}

void URogueGameplayAbility::FinishDeliver()
{
	if (!ActiveEntry)
	{
		EndSelf(true);
		return;
	}

	ExecuteCue(DeliverCueTag);

	// 대상 탐색과 효과 적용은 서버에서만
	if (HasAuthority(&CurrentActivationInfo))
	{
		PayHealthCost(CurrentActorInfo, ActivePayload.Level);

		Deliver(*ActiveEntry, ActivePayload);
	}

	// 반복 발동 증강은 지속 시간 동안 종료하지 않고 재발동
	if (RepeatDuration > 0.f)
	{
		BeginRepeatDeliver();
		return;
	}

	// 손에 든 증강은 사용 후 큐 제거
	if (bHoldToUse && EquipCueTag.IsValid())
	{
		K2_RemoveGameplayCue(EquipCueTag);
	}

	// 사용 모션 시간이 있으면 대기 후 회복
	if (bHoldToUse && RecoverySeconds > 0.f)
	{
		if (UAbilityTask_WaitDelay* Task = UAbilityTask_WaitDelay::WaitDelay(this, RecoverySeconds))
		{
			Task->OnFinish.AddDynamic(this, &ThisClass::HandleRecoveryFinished);
			Task->ReadyForActivation();
			return;
		}
	}

	// 클라 종료가 서버의 발동 대기를 끊지 않게 복제하지 않음
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
		/*bReplicateEndAbility*/ HasAuthority(&CurrentActivationInfo), false);
}

void URogueGameplayAbility::BeginRepeatDeliver()
{
	if (RepeatCueTag.IsValid())
	{
		K2_AddGameplayCueWithParams(RepeatCueTag, MakeCueParams(), /*bRemoveOnAbilityEnd*/ true);
	}

	if (UAbilityTask_WaitDelay* Task = UAbilityTask_WaitDelay::WaitDelay(this, RepeatDuration))
	{
		Task->OnFinish.AddDynamic(this, &ThisClass::HandleRecoveryFinished);
		Task->ReadyForActivation();
	}

	// 판정은 서버에서만
	if (HasAuthority(&CurrentActivationInfo))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(RepeatTimer, this, &ThisClass::RepeatDeliver, RepeatInterval, true);
		}
	}
}

void URogueGameplayAbility::RepeatDeliver()
{
	AActor* Avatar = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;

	// 매번 현재 시선 기준으로 다시 조립
	if (!ActiveEntry || !URogueTargetFilter::IsAlive(Avatar)
		|| !BuildEffectPayload(*ActiveEntry, Avatar, nullptr, CurrentSpecHandle, CurrentActorInfo, ActivePayload))
	{
		EndSelf(true);
		return;
	}

	Deliver(*ActiveEntry, ActivePayload);
}

void URogueGameplayAbility::HandleRecoveryFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
		/*bReplicateEndAbility*/ HasAuthority(&CurrentActivationInfo), false);
}

FGameplayCueParameters URogueGameplayAbility::MakeCueParams() const
{
	AActor* Avatar = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;

	FGameplayCueParameters CueParams;
	CueParams.Instigator   = Avatar;
	CueParams.EffectCauser = Avatar;
	CueParams.SourceObject = GetSourceObject(CurrentSpecHandle, CurrentActorInfo);

	CueParams.Location = ActivePayload.Origin;
	CueParams.Normal   = ActivePayload.Direction;

	return CueParams;
}

void URogueGameplayAbility::ExecuteCue(const FGameplayTag& CueTag)
{
	if (!CueTag.IsValid())
	{
		return;
	}

	K2_ExecuteGameplayCueWithParams(CueTag, MakeCueParams());
}

void URogueGameplayAbility::BeginHold()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

	UAbilityTask_WaitInputPress* WaitCancel = UAbilityTask_WaitInputPress::WaitInputPress(this);
	if (!ASC || !WaitCancel)
	{
		EndSelf(true);
		return;
	}

	// 기존의 증강 어빌리티 취소 후 장착
	static const FGameplayTagContainer AugmentTags(RogueTags::Augment_Ability);
	ASC->CancelAbilities(&AugmentTags, nullptr, this);
	ASC->AddLooseGameplayTag(RogueTags::State_Combat_UtilityHeld, 1,
		EGameplayTagReplicationState::TagOnly);

	bHoldTagApplied = true;

	WaitCancel->OnPress.AddDynamic(this, &ThisClass::HandleHoldCancelPress);
	WaitCancel->ReadyForActivation();

	// 손에 든 동안 유지
	if (EquipCueTag.IsValid())
	{
		K2_AddGameplayCueWithParams(EquipCueTag, MakeCueParams(), /*bRemoveOnAbilityEnd*/ true);
	}

	// 장착 모션이 있으면 대기
	if (EquipSeconds > 0.f)
	{
		if (UAbilityTask_WaitDelay* Task = UAbilityTask_WaitDelay::WaitDelay(this, EquipSeconds))
		{
			Task->OnFinish.AddDynamic(this, &ThisClass::HandleEquipFinished);
			Task->ReadyForActivation();
			return;
		}
	}

	HandleEquipFinished();
}

void URogueGameplayAbility::HandleEquipFinished()
{
	UAbilityTask_WaitGameplayEvent* WaitUse = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, RogueTags::Event_Utility_Input,
		/*OptionalExternalTarget*/ nullptr,
		/*OnlyTriggerOnce*/ true,
		/*OnlyMatchExact*/ false);

	if (!WaitUse)
	{
		EndSelf(true);
		return;
	}

	WaitUse->EventReceived.AddDynamic(this, &ThisClass::HandleUseInput);
	WaitUse->ReadyForActivation();
}

void URogueGameplayAbility::HandleUseInput(FGameplayEventData EventData)
{
	UsedInputTag = EventData.EventTag;

	ExecuteAbility(nullptr);
}

void URogueGameplayAbility::HandleHoldCancelPress(float TimeWaited)
{
	CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

void URogueGameplayAbility::EndSelf(bool bCancelled)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bCancelled);
}

void URogueGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
                                       const FGameplayAbilityActorInfo* ActorInfo,
                                       const FGameplayAbilityActivationInfo ActivationInfo,
                                       bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RepeatTimer);
	}

	if (bHoldTagApplied)
	{
		bHoldTagApplied = false;

		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(RogueTags::State_Combat_UtilityHeld, 1,
				EGameplayTagReplicationState::TagOnly);
		}
	}

	ActiveEntry = nullptr;
	ActivePayload = FRogueEffectPayload();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

FGameplayTag URogueGameplayAbility::ResolveEventWeaponTag(
	AActor* Avatar, const FGameplayEventData* TriggerEventData) const
{
	// 대미지 이벤트는 컨텍스트의 무기 태그 사용
	if (TriggerEventData)
	{
		const FRogueGameplayEffectContext* Context =
			FRogueGameplayEffectContext::ExtractFrom(TriggerEventData->ContextHandle);

		if (Context && Context->WeaponTag.IsValid())
		{
			return Context->WeaponTag;
		}
	}

	// 사격/재장전/교체 이벤트와 입력 발동은 현재 장착 무기 태그 사용
	const UCombatComponent* Combat = IsValid(Avatar)
		? Avatar->FindComponentByClass<UCombatComponent>()
		: nullptr;

	return Combat ? Combat->GetCurrentWeaponTag() : FGameplayTag();
}

bool URogueGameplayAbility::PassesDistanceCondition(const UAbilitySystemComponent* SourceASC,
                                                    const FGameplayEventData* TriggerEventData,
                                                    const FAugmentGrantedAbility& Entry)
{
	if (!Entry.bUseDistanceRange)
	{
		return true;
	}

	const FRogueGameplayEffectContext* Context = TriggerEventData
		? FRogueGameplayEffectContext::ExtractFrom(TriggerEventData->ContextHandle)
		: nullptr;

	// 히트스캔이 아닌 공격은 거리 조건 적용 X
	if (!Context || Context->HitGroups.IsEmpty())
	{
		return false;
	}

	// 사거리 증강은 상한만 추가
	float MaxDistance = Entry.MaxDistanceMeters;
	if (Entry.bDistanceScalesWithRangeBonus && SourceASC)
	{
		if (const URogueAugmentAttributeSet* Augment = SourceASC->GetSet<URogueAugmentAttributeSet>())
		{
			MaxDistance += FMath::Max(0.f, Augment->GetEffectiveRangeBonus());
		}
	}

	return Context->HitDistanceMeters >= Entry.MinDistanceMeters
		&& Context->HitDistanceMeters <= MaxDistance;
}

// 이벤트가 실어 온 대상
AActor* URogueGameplayAbility::ResolveEventTarget(AActor* Avatar,
                                                  const FGameplayEventData* TriggerEventData,
                                                  const FAugmentGrantedAbility& Entry) const
{
	if (!TriggerEventData)
	{
		return nullptr;
	}

	AActor* TargetActor = const_cast<AActor*>(TriggerEventData->Target.Get());
	if (!TargetActor || !URogueTargetFilter::IsAlive(TargetActor))
	{
		return nullptr;
	}

	if (Entry.bHostileTargetOnly
		&& URogueTargetFilter::GetRelation(Avatar, TargetActor) != ERogueTargetRelation::Hostile)
	{
		return nullptr;
	}

	return TargetActor;
}

// 이 어빌리티를 부여한 계열의 GrantedAbilities 항목
const FAugmentGrantedAbility* URogueGameplayAbility::FindGrantedEntry(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UAugmentDefinition* Definition = Cast<UAugmentDefinition>(GetSourceObject(Handle, ActorInfo));
	if (!Definition)
	{
		return nullptr;
	}

	const FSoftObjectPath SelfPath(GetClass());

	return Definition->GrantedAbilities.FindByPredicate(
		[&SelfPath](const FAugmentGrantedAbility& Granted)
		{
			return Granted.AbilityClass.ToSoftObjectPath() == SelfPath;
		});
}

// EffectsToApply 를 전부 로드
bool URogueGameplayAbility::LoadEffectClasses(const FAugmentGrantedAbility& Entry,
                                              TArray<TSubclassOf<UGameplayEffect>>& OutClasses) const
{
	OutClasses.Reserve(Entry.EffectsToApply.Num());

	for (int32 Index = 0; Index < Entry.EffectsToApply.Num(); ++Index)
	{
		UClass* EffectClass = Entry.EffectsToApply[Index].LoadSynchronous();
		if (!EffectClass)
		{
			UE_LOG(LogAugmentAbility, Error,
				TEXT("[AugmentAbility] %s 의 EffectsToApply[%d] 를 로드하지 못했습니다"),
				*GetName(), Index);

			OutClasses.Reset();
			return false;
		}

		OutClasses.Add(EffectClass);
	}

	return true;
}

// 페이로드 조립
bool URogueGameplayAbility::BuildEffectPayload(const FAugmentGrantedAbility& Entry, AActor* Avatar,
                                               AActor* EventTarget,
                                               const FGameplayAbilitySpecHandle Handle,
                                               const FGameplayAbilityActorInfo* ActorInfo,
                                               FRogueEffectPayload& OutPayload) const
{
	if (!LoadEffectClasses(Entry, OutPayload.EffectClasses))
	{
		return false;
	}

	OutPayload.Scope         = Entry.TargetScope;
	OutPayload.Query         = Entry.TargetQuery;
	OutPayload.Instigator    = Avatar;
	OutPayload.InstigatorASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	OutPayload.Causer        = Avatar;
	OutPayload.Level         = GetAbilityLevel(Handle, ActorInfo);
	OutPayload.SingleTarget  = (Entry.bApplyToEventTarget && EventTarget) ? EventTarget : Avatar;
	OutPayload.bSplitIntoIntegerTicks = Entry.bSplitIntoIntegerTicks;
	OutPayload.FalloffAtEdge    = Entry.DamageFalloffAtEdge;
	OutPayload.DamageSourceTag  = RogueTags::Damage_Source_Skill;

	if (const UAugmentDefinition* Definition = Cast<UAugmentDefinition>(GetSourceObject(Handle, ActorInfo)))
	{
		if (const FAugmentLevelData* LevelData = Definition->GetLevelData())
		{
			OutPayload.Magnitudes = LevelData->SetByCallerMagnitudes;
		}
	}

	OutPayload.FlatDamage = OutPayload.Magnitudes.FindRef(RogueTags::Data_Magnitude_Damage);

	// GE, 대미지가 둘 다 없으면 효과가 없는 것. 액터를 남기는 계열은 액터가 목적이다
	if (OutPayload.EffectClasses.IsEmpty() && OutPayload.FlatDamage <= 0.f
		&& Entry.SpawnActorClass.IsNull())
	{
		UE_LOG(LogAugmentAbility, Error,
			TEXT("[AugmentAbility] %s 를 부여한 계열에 EffectsToApply 도 %s 도 없습니다"),
			*GetName(), *RogueTags::Data_Magnitude_Damage.GetTag().ToString());
		return false;
	}

	if (Entry.TargetScope == ERogueTargetScope::AimSweep)
	{
		if (!IsValid(Avatar))
		{
			return false;
		}

		FVector EyeLocation;
		FRotator EyeRotation;
		Avatar->GetActorEyesViewPoint(EyeLocation, EyeRotation);

		OutPayload.Origin    = EyeLocation;
		OutPayload.Direction = EyeRotation.Vector();
		OutPayload.Length    = OutPayload.Magnitudes.FindRef(RogueTags::Data_Magnitude_Range) * 100.f;
		OutPayload.Radius    = OutPayload.Magnitudes.FindRef(RogueTags::Data_Magnitude_Radius) * 100.f;

		if (OutPayload.Length <= 0.f || OutPayload.Radius <= 0.f)
		{
			UE_LOG(LogAugmentAbility, Warning,
				TEXT("[AugmentAbility] %s 가 조준 스윕이지만 레벨 수치에 %s 나 %s 가 없습니다"),
				*GetName(), *RogueTags::Data_Magnitude_Range.GetTag().ToString(),
				*RogueTags::Data_Magnitude_Radius.GetTag().ToString());
			return false;
		}

		return true;
	}

	if (Entry.TargetScope != ERogueTargetScope::Radius)
	{
		return true;
	}

	const AActor* Center = (Entry.bAreaCentersOnEventTarget && EventTarget) ? EventTarget : Avatar;
	if (!IsValid(Center))
	{
		UE_LOG(LogAugmentAbility, Warning,
			TEXT("[AugmentAbility] %s 의 범위 중심이 없습니다"), *GetName());
		return false;
	}

	OutPayload.Origin = Center->GetActorLocation();
	OutPayload.Radius = OutPayload.Magnitudes.FindRef(RogueTags::Data_Magnitude_Radius) * 100.f;

	if (OutPayload.Radius <= 0.f)
	{
		UE_LOG(LogAugmentAbility, Warning,
			TEXT("[AugmentAbility] %s 가 범위 적용이지만 레벨 수치에 %s 가 없습니다"),
			*GetName(), *RogueTags::Data_Magnitude_Radius.GetTag().ToString());
		return false;
	}

	return true;
}

// 자기 체력을 대가로 지불
float URogueGameplayAbility::PayHealthCost(const FGameplayAbilityActorInfo* ActorInfo, const int32 Level) const
{
	static const FString Context(TEXT("URogueGameplayAbility::SelfCostHealth"));

	const float Cost = SelfCostHealth.GetValueAtLevel(Level, &Context);
	if (Cost <= 0.f)
	{
		return 0.f;
	}

	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const UWorld* World = Avatar ? Avatar->GetWorld() : nullptr;

	URogueDamageSubsystem* Subsystem = World ? World->GetSubsystem<URogueDamageSubsystem>() : nullptr;
	if (!Subsystem)
	{
		UE_LOG(LogAugmentAbility, Error,
			TEXT("[AugmentAbility] %s 가 RogueDamageSubsystem 을 얻지 못해 체력을 지불하지 못했습니다"), *GetName());
		return 0.f;
	}

	return Subsystem->ApplyHealthCost(Avatar, Cost);
}

int32 URogueGameplayAbility::GetMaxCharges(int32 Level) const
{
	// 문맥 문자열을 넘기면 커브 행을 못 찾을 때 경고
	static const FString Context(TEXT("URogueGameplayAbility::MaxCharges"));

	// 0 이나 음수면 어빌리티가 잠김
	return FMath::Max(1, MaxCharges.AsInteger(Level, &Context));
}

FGameplayTag URogueGameplayAbility::ResolveCooldownTag(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo) const
{
    const FGameplayTag SlotCooldowns[] = {RogueTags::Cooldown_Augment_Slot1,
        RogueTags::Cooldown_Augment_Slot2, RogueTags::Cooldown_Augment_Slot3};
    if (CooldownTag != SlotCooldowns[0] && CooldownTag != SlotCooldowns[1] && CooldownTag != SlotCooldowns[2])
    {
        return CooldownTag;
    }
    UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
    const FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
    const UAugmentDefinition* Definition = Spec ? Cast<UAugmentDefinition>(Spec->SourceObject.Get()) : nullptr;
    if (!Definition || Definition->AugmentCategory != EAugmentCategory::Active)
    {
        return CooldownTag;
    }
    const FGameplayTag OriginalSlot = Definition->GetActiveInputSlotTag();
    const FGameplayTagContainer& SpecTags = Spec->GetDynamicSpecSourceTags();
    // Ordinary match grants retain their data-defined cooldown exactly, even if a
    // designer deliberately shared a cooldown group between different abilities.
    if (!OriginalSlot.IsValid() || SpecTags.HasTagExact(OriginalSlot)) { return CooldownTag; }
    static const FName InputNames[] = {TEXT("InputTag.Ability.Slot1"), TEXT("InputTag.Ability.Slot2"), TEXT("InputTag.Ability.Slot3")};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(InputNames); ++Index)
    {
        const FGameplayTag Input = FGameplayTag::RequestGameplayTag(InputNames[Index], false);
        if (Input.IsValid() && SpecTags.HasTagExact(Input)) { return SlotCooldowns[Index]; }
    }
    return CooldownTag;
}

bool URogueGameplayAbility::CheckCooldown(const FGameplayAbilitySpecHandle Handle,
                                          const FGameplayAbilityActorInfo* ActorInfo,
                                          FGameplayTagContainer* OptionalRelevantTags) const
{
	const FGameplayTag EffectiveCooldownTag = ResolveCooldownTag(Handle, ActorInfo);
	if (!EffectiveCooldownTag.IsValid())
	{
		return true;
	}

	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC)
	{
		return true;
	}

	const FGameplayTagContainer CooldownTags(EffectiveCooldownTag);

	// 현재 스택이 최대 충전 수보다 적으면 쿨타임이 남아있어도 사용 가능
	const int32 SpentCharges =
		ASC->GetAggregatedStackCount(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownTags));

	if (SpentCharges < GetMaxCharges(GetAbilityLevel(Handle, ActorInfo)))
	{
		return true;
	}

	// 실패 태그 및 쿨타임 태그 반환
	if (OptionalRelevantTags)
	{
		const FGameplayTag& FailCooldownTag = UAbilitySystemGlobals::Get().ActivateFailCooldownTag;
		if (FailCooldownTag.IsValid())
		{
			OptionalRelevantTags->AddTag(FailCooldownTag);
		}

		OptionalRelevantTags->AppendMatchingTags(ASC->GetOwnedGameplayTags(), CooldownTags);
	}

	return false;
}

const FGameplayTagContainer* URogueGameplayAbility::GetCooldownTags() const
{
	CooldownTagContainer.Reset();

	const FGameplayTag EffectiveCooldownTag = ResolveCooldownTag(CurrentSpecHandle, CurrentActorInfo);
	if (EffectiveCooldownTag.IsValid())
	{
		CooldownTagContainer.AddTag(EffectiveCooldownTag);
	}

	return &CooldownTagContainer;
}

void URogueGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
                                          const FGameplayAbilityActorInfo* ActorInfo,
                                          const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// 태그가 없으면 쿨타임이 없는 어빌리티
	const FGameplayTag EffectiveCooldownTag = ResolveCooldownTag(Handle, ActorInfo);
	if (!EffectiveCooldownTag.IsValid())
	{
		return;
	}

	const int32 Level = GetAbilityLevel(Handle, ActorInfo);

	static const FString Context(TEXT("URogueGameplayAbility::BaseCooldown"));
	const float LevelCooldown = BaseCooldown.GetValueAtLevel(Level, &Context);

	// 쿨타임이 0초면 쿨타임이 없는 어빌리티
	if (LevelCooldown <= 0.f)
	{
		return;
	}

	// 어트리뷰트는 ASC 소유자(PlayerState)에 존재
	const AActor* StatSource = ActorInfo ? ActorInfo->OwnerActor.Get() : nullptr;

	// 쿨타임이 0 이하면 0초로 처리
	float Duration = URogueAugmentStatResolver::GetFinalCooldown(StatSource, LevelCooldown);
	if (Duration <= 0.f)
	{
		return;
	}

	// 이전 쿨타임이 남아있으면 그만큼 더해, 
	// 두 번째 사용의 쿨타임이 첫 번째 사용의 쿨타임이 끝나면 적용
	Duration += FMath::Max(0.f, GetCooldownTimeRemaining(ActorInfo));

	UClass* CooldownEffectClass = GetCooldownEffectClass();
	if (!CooldownEffectClass)
	{
		return;
	}

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
		Handle, ActorInfo, ActivationInfo,
		CooldownEffectClass, Level);

	if (!SpecHandle.IsValid())
	{
		return;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(RogueTags::Data_Magnitude_Duration, Duration);

	// 어빌리티별 쿨타임 태그
	SpecHandle.Data->DynamicGrantedTags.AddTag(EffectiveCooldownTag);

	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}
