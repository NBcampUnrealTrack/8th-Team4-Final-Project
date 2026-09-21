// ShooterProfileImageRulesTests.cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GameFramework/OnlineReplStructs.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemTypes.h"
#include "UI/Subsystem/ShooterProfileImageSubsystem.h"

THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END

namespace ShooterProfileImageRulesTest
{
	// 공개 팩토리로만 만듦
	FUniqueNetIdRepl MakeStringId(const FString& IdText, const FName Type)
	{
		const FUniqueNetIdRef Id = FUniqueNetIdString::Create(IdText, Type);
		return FUniqueNetIdRepl(Id);
	}

	FString ToDecimalText(const CSteamID& SteamId)
	{
		return FString::Printf(TEXT("%llu"), SteamId.ConvertToUint64());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FShooterProfileImageToSteamIdTest,
	"FPS.UI.ProfileImage.Rules.ToSteamId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShooterProfileImageToSteamIdTest::RunTest(const FString& Parameters)
{
	using namespace ShooterProfileImageRulesTest;

	// uint64 전용 TestEqual 이 없어 int64 로 비교함
	const auto ExpectSteamId = [this](const TCHAR* What, const FUniqueNetIdRepl& UniqueId, const uint64 Expected)
	{
		TestEqual(What,
			static_cast<int64>(FShooterProfileImageRules::ToSteamId(UniqueId)),
			static_cast<int64>(Expected));
	};

	const CSteamID UserId(1u, k_EUniversePublic, k_EAccountTypeIndividual);
	const CSteamID ServerId(1u, k_EUniversePublic, k_EAccountTypeGameServer);
	const FString UserText = ToDecimalText(UserId);

	ExpectSteamId(TEXT("기본값 id 는 0"), FUniqueNetIdRepl(), 0);
	ExpectSteamId(TEXT("NULL 타입이면 숫자여도 0"), MakeStringId(UserText, NULL_SUBSYSTEM), 0);
	ExpectSteamId(TEXT("유효한 v2 id 는 0"),
		FUniqueNetIdRepl(FUniqueNetIdWrapper(UE::Online::FAccountId(UE::Online::EOnlineServices::Null, 1))), 0);
	ExpectSteamId(TEXT("Steam 개인 계정 id 는 그 값"), MakeStringId(UserText, STEAM_SUBSYSTEM), UserId.ConvertToUint64());
	ExpectSteamId(TEXT("숫자가 아니면 0"), MakeStringId(TEXT("abc"), STEAM_SUBSYSTEM), 0);
	ExpectSteamId(TEXT("부호가 붙으면 0"), MakeStringId(TEXT("+") + UserText, STEAM_SUBSYSTEM), 0);
	ExpectSteamId(TEXT("앞에 공백이 있으면 0"), MakeStringId(TEXT(" ") + UserText, STEAM_SUBSYSTEM), 0);
	ExpectSteamId(TEXT("\"0\" 은 0"), MakeStringId(TEXT("0"), STEAM_SUBSYSTEM), 0);
	ExpectSteamId(TEXT("20자리 overflow 는 0"), MakeStringId(TEXT("99999999999999999999"), STEAM_SUBSYSTEM), 0);
	ExpectSteamId(TEXT("게임 서버 id 는 0"), MakeStringId(ToDecimalText(ServerId), STEAM_SUBSYSTEM), 0);

	return true;
}

#endif
