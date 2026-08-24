#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GameMode/BombRoundRules.h"
#include "GameMode/InitialPlayerReadinessTracker.h"
#include "GameMode/RoundTypes.h"
#include "GameMode/SurrenderVoteTypes.h"

namespace
{
	void TestResolution(
		FAutomationTestBase& Test,
		const TCHAR* What,
		const FBombRoundResolution& Resolution,
		const EGameTeam ExpectedWinner,
		const ERoundEndReason ExpectedReason)
	{
		Test.TestTrue(
			FString::Printf(TEXT("%s ends the round"), What),
			Resolution.ShouldEndRound());
		Test.TestEqual(
			FString::Printf(TEXT("%s winner"), What),
			static_cast<int32>(Resolution.WinningTeam),
			static_cast<int32>(ExpectedWinner));
		Test.TestEqual(
			FString::Printf(TEXT("%s reason"), What),
			static_cast<int32>(Resolution.EndReason),
			static_cast<int32>(ExpectedReason));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBombMutualEliminationRuleTest,
	"FPS.GameFlow.Bomb.Rules.MutualElimination",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBombMutualEliminationRuleTest::RunTest(const FString& Parameters)
{
	const FBombRoundResolution BeforePlant = BombRoundRules::ResolveElimination(
		0,
		0,
		false,
		EGameTeam::TeamA,
		EGameTeam::TeamB);
	TestResolution(
		*this,
		TEXT("Mutual elimination before plant"),
		BeforePlant,
		EGameTeam::TeamB,
		ERoundEndReason::MutualElimination);

	const FBombRoundResolution AfterPlant = BombRoundRules::ResolveElimination(
		0,
		0,
		true,
		EGameTeam::TeamA,
		EGameTeam::TeamB);
	TestResolution(
		*this,
		TEXT("Mutual elimination after plant"),
		AfterPlant,
		EGameTeam::TeamB,
		ERoundEndReason::MutualElimination);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBombPlantedAttackerEliminationRuleTest,
	"FPS.GameFlow.Bomb.Rules.PlantedAttackerElimination",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBombPlantedAttackerEliminationRuleTest::RunTest(const FString& Parameters)
{
	const FBombRoundResolution Resolution = BombRoundRules::ResolveElimination(
		0,
		1,
		true,
		EGameTeam::TeamA,
		EGameTeam::TeamB);

	TestFalse(
		TEXT("Planted objective keeps running after only the attackers are eliminated"),
		Resolution.ShouldEndRound());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBombRoundTimeExpiredRuleTest,
	"FPS.GameFlow.Bomb.Rules.RoundTimeExpired",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBombRoundTimeExpiredRuleTest::RunTest(const FString& Parameters)
{
	const FBombRoundResolution Resolution = BombRoundRules::ResolveTimeExpired(
		1,
		1,
		false,
		EGameTeam::TeamA,
		EGameTeam::TeamB);
	TestResolution(
		*this,
		TEXT("Round time expiration"),
		Resolution,
		EGameTeam::TeamB,
		ERoundEndReason::RoundTimeExpired);

	const FBombRoundResolution PlantedResolution = BombRoundRules::ResolveTimeExpired(
		1,
		1,
		true,
		EGameTeam::TeamA,
		EGameTeam::TeamB);
	TestFalse(
		TEXT("Round time does not resolve an already planted objective"),
		PlantedResolution.ShouldEndRound());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRoundRoleSwitchRuleTest,
	"FPS.GameFlow.Round.Rules.SideSwitchAtRoundSeven",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRoundRoleSwitchRuleTest::RunTest(const FString& Parameters)
{
	const FRoundContext RoundSix = FRoundContext::Make(
		6,
		7,
		ETeamRole::Attackers,
		ETeamRole::Defenders);
	TestEqual(
		TEXT("Team A attacks through round six"),
		static_cast<int32>(RoundSix.TeamARole),
		static_cast<int32>(ETeamRole::Attackers));
	TestEqual(
		TEXT("Team B defends through round six"),
		static_cast<int32>(RoundSix.TeamBRole),
		static_cast<int32>(ETeamRole::Defenders));

	const FRoundContext RoundSeven = FRoundContext::Make(
		7,
		7,
		ETeamRole::Attackers,
		ETeamRole::Defenders);
	TestEqual(
		TEXT("Team A switches to defense in round seven"),
		static_cast<int32>(RoundSeven.TeamARole),
		static_cast<int32>(ETeamRole::Defenders));
	TestEqual(
		TEXT("Team B switches to attack in round seven"),
		static_cast<int32>(RoundSeven.TeamBRole),
		static_cast<int32>(ETeamRole::Attackers));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FInitialPlayerReadinessRuleTest,
	"FPS.GameFlow.Round.Readiness.RequiresEveryAssignedPlayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInitialPlayerReadinessRuleTest::RunTest(const FString& Parameters)
{
	FInitialPlayerReadinessSnapshot Snapshot;
	Snapshot.AssignedPlayerCount = 1;
	Snapshot.ReadyPlayerCount = 1;
	TestFalse(
		TEXT("A host alone cannot satisfy the two-player gate"),
		Snapshot.AreAllAssignedPlayersReady(2));

	Snapshot.AssignedPlayerCount = 2;
	Snapshot.ReadyPlayerCount = 1;
	TestFalse(
		TEXT("Every assigned player must be ready"),
		Snapshot.AreAllAssignedPlayersReady(2));

	Snapshot.ReadyPlayerCount = 2;
	TestTrue(
		TEXT("Two assigned and ready players satisfy the gate"),
		Snapshot.AreAllAssignedPlayersReady(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSurrenderMajorityRuleTest,
	"FPS.GameFlow.Surrender.Rules.StrictMajority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurrenderMajorityRuleTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("One voter requires one approval"), SurrenderVoteRules::CalculateRequiredApprovals(1), 1);
	TestEqual(TEXT("Two voters require two approvals"), SurrenderVoteRules::CalculateRequiredApprovals(2), 2);
	TestEqual(TEXT("Three voters require two approvals"), SurrenderVoteRules::CalculateRequiredApprovals(3), 2);
	TestEqual(TEXT("Four voters require three approvals"), SurrenderVoteRules::CalculateRequiredApprovals(4), 3);
	TestEqual(TEXT("Five voters require three approvals"), SurrenderVoteRules::CalculateRequiredApprovals(5), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSurrenderWinnerRuleTest,
	"FPS.GameFlow.Surrender.Rules.OpposingTeamWins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurrenderWinnerRuleTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("Team A surrender awards Team B"),
		static_cast<int32>(SurrenderVoteRules::GetOpposingTeam(EGameTeam::TeamA)),
		static_cast<int32>(EGameTeam::TeamB));
	TestEqual(
		TEXT("Team B surrender awards Team A"),
		static_cast<int32>(SurrenderVoteRules::GetOpposingTeam(EGameTeam::TeamB)),
		static_cast<int32>(EGameTeam::TeamA));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSurrenderVotingPhaseRuleTest,
	"FPS.GameFlow.Surrender.Rules.EligiblePhases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurrenderVotingPhaseRuleTest::RunTest(const FString& Parameters)
{
	// GameMode also requires HasMatchStarted before accepting a request.
	TestTrue(TEXT("Round starting is phase-eligible"), SurrenderVoteRules::IsVotingPhase(EGameFlowPhase::RoundStarting));
	TestTrue(TEXT("Augment selection is phase-eligible"), SurrenderVoteRules::IsVotingPhase(EGameFlowPhase::AugmentSelecting));
	TestTrue(TEXT("Combat is phase-eligible"), SurrenderVoteRules::IsVotingPhase(EGameFlowPhase::RoundInProgress));
	TestFalse(TEXT("Waiting is not phase-eligible"), SurrenderVoteRules::IsVotingPhase(EGameFlowPhase::WaitingForPlayers));
	TestFalse(TEXT("Round ending is not phase-eligible"), SurrenderVoteRules::IsVotingPhase(EGameFlowPhase::RoundEnding));
	TestFalse(TEXT("Match end is not phase-eligible"), SurrenderVoteRules::IsVotingPhase(EGameFlowPhase::MatchEnded));
	return true;
}

#endif
