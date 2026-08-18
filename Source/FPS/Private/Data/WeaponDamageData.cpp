#include "Data/WeaponDamageData.h"

#include "GAS/RogueGASSettings.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWeaponDamageData, Log, All);

#define LOCTEXT_NAMESPACE "WeaponDamageData"

const FWeaponDamageEntry* UWeaponDamageData::FindEntry(const FGameplayTag WeaponTag)
{
	if (!WeaponTag.IsValid())
	{
		return nullptr;
	}

	const UWeaponDamageData* DamageData = GetDefault<URogueGASSettings>()->WeaponDamageData.LoadSynchronous();
	if (!DamageData)
	{
		UE_LOG(LogWeaponDamageData, Error,
			TEXT("[RogueDamage] WeaponDamageData 가 설정되지 않았습니다. "
			     "프로젝트 설정 > 게임 > Rogue GAS 를 확인하세요."));
		return nullptr;
	}

	const FWeaponDamageEntry* Entry = DamageData->Weapons.Find(WeaponTag);
	if (!Entry)
	{
		UE_LOG(LogWeaponDamageData, Warning,
			TEXT("[RogueDamage] WeaponDamageData 에 등록되지 않은 무기 태그입니다. Tag=%s"),
			*WeaponTag.ToString());
	}

	return Entry;
}

#if WITH_EDITOR
EDataValidationResult UWeaponDamageData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	for (const TPair<FGameplayTag, FWeaponDamageEntry>& Pair : Weapons)
	{
		const FGameplayTag&       WeaponTag = Pair.Key;
		const FWeaponDamageEntry& Entry     = Pair.Value;

		const FText TagText = FText::FromString(
			WeaponTag.IsValid() ? WeaponTag.ToString() : TEXT("(빈 태그)"));

		auto AddError = [&Context, &Result, &TagText](FText&& Message)
		{
			Context.AddError(FText::Format(LOCTEXT("EntryError", "[{0}] {1}"), TagText, MoveTemp(Message)));
			Result = EDataValidationResult::Invalid;
		};

		auto AddWarning = [&Context, &TagText](FText&& Message)
		{
			Context.AddMessage(EMessageSeverity::Warning,
				FText::Format(LOCTEXT("EntryWarning", "[{0}] {1}"), TagText, MoveTemp(Message)));
		};

		if (!WeaponTag.IsValid())
		{
			AddError(LOCTEXT("InvalidWeaponTag", "무기 태그가 비어 있습니다."));
		}

		if (Entry.BaseDamagePerPellet <= 0.f)
		{
			AddWarning(LOCTEXT("ZeroBaseDamage", "기본 피해량이 0 이라 이 무기는 피해를 주지 못합니다."));
		}

		if (Entry.MaximumRangeMeters > 0.f)
		{
			if (Entry.MaximumRangeMeters < Entry.EffectiveRangeMeters)
			{
				AddError(FText::Format(
					LOCTEXT("RangeInverted", "최대 사거리({0}m)가 유효 사거리({1}m)보다 짧습니다."),
					FText::AsNumber(Entry.MaximumRangeMeters),
					FText::AsNumber(Entry.EffectiveRangeMeters)));
			}

			if (Entry.MinimumDamage > Entry.BaseDamagePerPellet)
			{
				AddError(FText::Format(
					LOCTEXT("MinimumAboveBase", "최소 대미지({0})가 기본 피해량({1})보다 큽니다. 감쇠가 무시됩니다."),
					FText::AsNumber(Entry.MinimumDamage),
					FText::AsNumber(Entry.BaseDamagePerPellet)));
			}

			if (Entry.MaximumRangeMeters > Entry.EffectiveRangeMeters
				&& Entry.DamageFalloffPerMeter <= 0.f)
			{
				AddWarning(LOCTEXT("ZeroFalloffRate",
					"1m 당 감쇠량이 0 입니다. 최대 사거리에서 최소 대미지로 한 번에 떨어집니다."));
			}
		}

		for (const TPair<FGameplayTag, float>& ZonePair : Entry.HitZoneMultipliers)
		{
			if (!ZonePair.Key.IsValid())
			{
				AddError(LOCTEXT("InvalidHitZoneTag", "부위 배율에 빈 태그가 있습니다."));
				continue;
			}

			if (!FMath::IsFinite(ZonePair.Value) || ZonePair.Value < 0.f)
			{
				AddError(FText::Format(
					LOCTEXT("InvalidHitZoneMultiplier", "부위 배율 {0} 의 값이 올바르지 않습니다."),
					FText::FromString(ZonePair.Key.ToString())));
			}
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
