#include "test.h"

#include <base/color.h>

#include <engine/console.h>
#include <engine/kernel.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <generated/protocol.h>

#include <game/client/components/qmclient/map_progress.h>
#include <game/client/components/qmclient/modes.h>
#include <game/client/components/qmclient/translate/translate_ui_settings.h>

#include <gtest/gtest.h>

#include <string>

namespace
{
	QmMapProgress::CMap MakeProgressLine(int Width)
	{
		QmMapProgress::CMap Map(Width, 1);
		for(int Index = 0; Index < Width; ++Index)
			Map.SetTile(Index, QmMapProgress::MakeTile(Index == 0 ? TILE_START : (Index == Width - 1 ? TILE_FINISH : TILE_AIR)));
		return Map;
	}

	QmMapProgress::SEstimate MoveProgressPlayer(QmMapProgress::CPlayer &Player, const QmMapProgress::CMap &Map, int Index, int TeleCheckpoint = 0)
	{
		Player.Observe(Map.Tile(Index), Index);
		// 小地图也分多次推进，以覆盖换段和回传后重建距离场的行为。
		for(int Step = 0; Step < 128; ++Step)
			Player.Update(Map, Index, TeleCheckpoint, 128);
		return Player.Estimate();
	}
}

TEST(QmMapProgress, DragThroughFreezeUsesRouteLengthAndCanGoBack)
{
	auto Map = MakeProgressLine(11);
	for(int Index = 4; Index <= 6; ++Index)
		Map.SetTile(Index, QmMapProgress::MakeTile(TILE_FREEZE));
	Map.Finalize();
	QmMapProgress::CPlayer Player;
	EXPECT_FLOAT_EQ(MoveProgressPlayer(Player, Map, 0).m_Progress, 0.0f);
	const auto InWater = MoveProgressPlayer(Player, Map, 5);
	ASSERT_TRUE(InWater.m_Valid);
	EXPECT_FLOAT_EQ(InWater.m_Progress, 0.5f);
	EXPECT_FLOAT_EQ(MoveProgressPlayer(Player, Map, 7).m_Progress, 0.7f);
	EXPECT_FLOAT_EQ(MoveProgressPlayer(Player, Map, 3).m_Progress, 0.3f);
	EXPECT_FLOAT_EQ(MoveProgressPlayer(Player, Map, 10).m_Progress, 1.0f);
	EXPECT_FLOAT_EQ(MoveProgressPlayer(Player, Map, 0).m_Progress, 0.0f);
}

TEST(QmMapProgress, SafeDetourStillWinsButPenaltyDoesNotBecomeLength)
{
	QmMapProgress::CMap Map(5, 2);
	for(int Index = 0; Index < 10; ++Index)
		Map.SetTile(Index, QmMapProgress::MakeTile(TILE_AIR));
	Map.SetTile(0, QmMapProgress::MakeTile(TILE_START));
	Map.SetTile(4, QmMapProgress::MakeTile(TILE_FINISH));
	Map.SetTile(1, QmMapProgress::MakeTile(TILE_FREEZE));
	Map.SetTile(2, QmMapProgress::MakeTile(TILE_FREEZE));
	Map.Finalize();
	QmMapProgress::CField Field;
	Field.Start(Map, 0);
	while(!Field.Complete())
		Field.Step(Map, 1);
	EXPECT_EQ(Field.Length(0), 6);
	EXPECT_EQ(Field.Next(0), 5);
}

TEST(QmMapProgress, DeepFreezeIsPassableButDeathAndSolidAreNot)
{
	for(const int Tile : {TILE_DFREEZE, TILE_LFREEZE, TILE_DEATH, TILE_SOLID, TILE_NOHOOK})
	{
		auto Map = MakeProgressLine(5);
		Map.SetTile(2, QmMapProgress::MakeTile(Tile));
		Map.Finalize();
		QmMapProgress::CField Field;
		Field.Start(Map, 0);
		while(!Field.Complete())
			Field.Step(Map, 1);
		EXPECT_EQ(Field.Length(0), Tile == TILE_DFREEZE || Tile == TILE_LFREEZE ? 4 : -1);
	}
}

TEST(QmMapProgress, DirectTeleportLinksRoomsWithoutCountingItsWorldDistance)
{
	auto Map = MakeProgressLine(9);
	Map.SetTile(2, QmMapProgress::MakeTile(TILE_AIR, TILE_AIR, TILE_TELEINEVIL, 17));
	Map.SetTile(3, QmMapProgress::MakeTile(TILE_SOLID));
	Map.SetTile(6, QmMapProgress::MakeTile(TILE_AIR, TILE_AIR, TILE_TELEOUT, 17));
	Map.Finalize();
	QmMapProgress::CField Field;
	Field.Start(Map, 0);
	while(!Field.Complete())
		Field.Step(Map, 1);
	EXPECT_EQ(Field.Length(0), 4);
	EXPECT_EQ(Field.Next(2), 6);
	EXPECT_EQ(Field.Length(2), Field.Length(6));
}

TEST(QmMapProgress, CheckpointReturnUsesRecordedNumberAndFallsBackToEarlierExit)
{
	auto Map = MakeProgressLine(9);
	Map.SetTile(1, QmMapProgress::MakeTile(ENTITY_OFFSET + ENTITY_SPAWN));
	Map.SetTile(2, QmMapProgress::MakeTile(TILE_AIR, TILE_AIR, TILE_TELECHECKINEVIL, 0));
	Map.SetTile(3, QmMapProgress::MakeTile(TILE_SOLID));
	Map.SetTile(6, QmMapProgress::MakeTile(TILE_AIR, TILE_AIR, TILE_TELECHECKOUT, 2));
	Map.Finalize();
	QmMapProgress::CField Field;
	Field.Start(Map, 4);
	while(!Field.Complete())
		Field.Step(Map, 1);
	EXPECT_EQ(Field.Next(2), 6);
	EXPECT_EQ(Field.Length(0), 4);
	Field.Start(Map, 1);
	while(!Field.Complete())
		Field.Step(Map, 1);
	EXPECT_EQ(Field.Length(0), -1);
}

TEST(QmMapProgress, CheckpointReturnWithoutExitGoesToSpawn)
{
	auto Map = MakeProgressLine(9);
	Map.SetTile(2, QmMapProgress::MakeTile(TILE_AIR, TILE_AIR, TILE_TELECHECKIN, 0));
	Map.SetTile(3, QmMapProgress::MakeTile(TILE_SOLID));
	Map.SetTile(6, QmMapProgress::MakeTile(ENTITY_OFFSET + ENTITY_SPAWN));
	Map.Finalize();
	QmMapProgress::CField Field;
	Field.Start(Map, 0);
	while(!Field.Complete())
		Field.Step(Map, 1);
	EXPECT_EQ(Field.Next(2), 6);
	EXPECT_EQ(Field.Length(0), 4);
}

TEST(QmMapProgress, TimeCheckpointsDivideStagesAndBacktrackingDecreasesProgress)
{
	auto Map = MakeProgressLine(11);
	Map.SetTile(2, QmMapProgress::MakeTile(TILE_AIR, TILE_TIME_CHECKPOINT_FIRST));
	Map.SetTile(8, QmMapProgress::MakeTile(TILE_TIME_CHECKPOINT_FIRST + 1));
	Map.Finalize();
	ASSERT_EQ(Map.CheckpointCount(), 2);
	QmMapProgress::CPlayer Player;
	MoveProgressPlayer(Player, Map, 0);
	EXPECT_NEAR(MoveProgressPlayer(Player, Map, 2).m_Progress, 1.0f / 3.0f, 0.001f);
	EXPECT_NEAR(MoveProgressPlayer(Player, Map, 5).m_Progress, 0.5f, 0.001f);
	EXPECT_NEAR(MoveProgressPlayer(Player, Map, 8).m_Progress, 2.0f / 3.0f, 0.001f);
	EXPECT_NEAR(MoveProgressPlayer(Player, Map, 9).m_Progress, 5.0f / 6.0f, 0.001f);
	MoveProgressPlayer(Player, Map, 8);
	EXPECT_NEAR(MoveProgressPlayer(Player, Map, 7).m_Progress, 11.0f / 18.0f, 0.001f);
	EXPECT_NEAR(MoveProgressPlayer(Player, Map, 3).m_Progress, 7.0f / 18.0f, 0.001f);
	MoveProgressPlayer(Player, Map, 2);
	EXPECT_NEAR(MoveProgressPlayer(Player, Map, 1).m_Progress, 1.0f / 6.0f, 0.001f);
}

TEST(QmMapProgress, MissingAndReversedCheckpointOrderFallBackToWholeMap)
{
	for(const bool Reversed : {false, true})
	{
		auto Map = MakeProgressLine(11);
		Map.SetTile(2, QmMapProgress::MakeTile(TILE_TIME_CHECKPOINT_FIRST + 1));
		if(Reversed)
			Map.SetTile(8, QmMapProgress::MakeTile(TILE_TIME_CHECKPOINT_FIRST));
		Map.Finalize();
		QmMapProgress::CPlayer Player;
		MoveProgressPlayer(Player, Map, 0);
		const auto Estimate = MoveProgressPlayer(Player, Map, 5);
		ASSERT_TRUE(Estimate.m_Valid);
		EXPECT_FLOAT_EQ(Estimate.m_Progress, 0.5f);
	}
}

TEST(QmMapProgress, MidRunEnableCanEstimateWithoutHavingSeenStart)
{
	auto Map = MakeProgressLine(11);
	Map.Finalize();
	QmMapProgress::CPlayer Player;
	const auto Estimate = MoveProgressPlayer(Player, Map, 6);
	ASSERT_TRUE(Estimate.m_Valid);
	EXPECT_FLOAT_EQ(Estimate.m_Progress, 0.6f);
}

TEST(QmMapProgress, MainAndDummyCheckpointStatesAreIndependent)
{
	auto Map = MakeProgressLine(9);
	Map.SetTile(1, QmMapProgress::MakeTile(ENTITY_OFFSET + ENTITY_SPAWN));
	Map.SetTile(2, QmMapProgress::MakeTile(TILE_AIR, TILE_AIR, TILE_TELECHECKIN, 0));
	Map.SetTile(3, QmMapProgress::MakeTile(TILE_SOLID));
	Map.SetTile(6, QmMapProgress::MakeTile(TILE_AIR, TILE_AIR, TILE_TELECHECKOUT, 2));
	Map.Finalize();
	QmMapProgress::CPlayer Main;
	QmMapProgress::CPlayer Dummy;
	EXPECT_TRUE(MoveProgressPlayer(Main, Map, 1, 2).m_Valid);
	EXPECT_FALSE(MoveProgressPlayer(Dummy, Map, 1, 0).m_Valid);
	EXPECT_FALSE(MoveProgressPlayer(Main, Map, 1, 0).m_Valid);
	EXPECT_TRUE(MoveProgressPlayer(Dummy, Map, 1, 2).m_Valid);
}

TEST(QmMapProgress, OneWayRestrictionsAreRespectedAndEmptyMapsStayUnknown)
{
	auto Map = MakeProgressLine(5);
	Map.SetTile(2, QmMapProgress::MakeTile(TILE_AIR, TILE_AIR, 0, 0, CANTMOVE_RIGHT));
	Map.Finalize();
	QmMapProgress::CField Field;
	Field.Start(Map, 0);
	while(!Field.Complete())
		Field.Step(Map, 1);
	EXPECT_EQ(Field.Length(0), -1);
	QmMapProgress::CMap Empty(1, 1);
	Empty.Finalize();
	QmMapProgress::CPlayer Player;
	EXPECT_FALSE(MoveProgressPlayer(Player, Empty, 0).m_Valid);
}

static void ExpectColorNear(const ColorRGBA &Color, const ColorRGBA &Expected)
{
	EXPECT_NEAR(Color.r, Expected.r, 0.02f);
	EXPECT_NEAR(Color.g, Expected.g, 0.02f);
	EXPECT_NEAR(Color.b, Expected.b, 0.02f);
	EXPECT_NEAR(Color.a, Expected.a, 0.02f);
}

TEST(QmPredictionMode, UpdatePredictionDoesNotOverrideClientOptIn)
{
	const std::string Source = ReadTestSourceFile("src/game/client/gameclient.cpp");
	const size_t FunctionStart = Source.find("void CGameClient::UpdatePrediction()");
	ASSERT_NE(FunctionStart, std::string::npos);
	const size_t FunctionEnd = Source.find("\nvoid CGameClient::", FunctionStart + 1);
	ASSERT_NE(FunctionEnd, std::string::npos);
	const std::string FunctionBody = Source.substr(FunctionStart, FunctionEnd - FunctionStart);
	const std::string Assignment = "m_GameWorld.m_WorldConfig.m_PredictEvents =";
	const size_t FirstAssignment = FunctionBody.find(Assignment);
	ASSERT_NE(FirstAssignment, std::string::npos);
	EXPECT_EQ(FunctionBody.find(Assignment, FirstAssignment + Assignment.size()), std::string::npos);
	EXPECT_NE(FunctionBody.find("m_GameWorld.m_WorldConfig.m_PredictEvents = g_Config.m_ClPredictEvents && m_GameInfo.m_PredictEvents;"), std::string::npos);
}

TEST(QmGoresMode, ManualGuideRevealOverridesAutomaticGuideHiding)
{
	EXPECT_TRUE(ShouldHideGoresGuide(true, true, false));
	EXPECT_FALSE(ShouldHideGoresGuide(true, true, true));
	EXPECT_FALSE(ShouldHideGoresGuide(true, false, false));
	EXPECT_FALSE(ShouldHideGoresGuide(false, true, false));
}

TEST(QmGoresMode, DebugRouteDoesNotUseHideGuidesGate)
{
	EXPECT_TRUE(ShouldRenderGoresDebugRoute(true, true, true));
	EXPECT_FALSE(ShouldRenderGoresDebugRoute(false, true, true));
	EXPECT_FALSE(ShouldRenderGoresDebugRoute(true, false, true));
	EXPECT_FALSE(ShouldRenderGoresDebugRoute(true, true, false));
}

TEST(QmGoresMode, MovingWaterTilesRequireAxiomOrGoresContext)
{
	EXPECT_TRUE(ShouldEnableQmMovingWaterTiles("Gores", "", "", ""));
	EXPECT_TRUE(ShouldEnableQmMovingWaterTiles("", "DDNet Gores", "", ""));
	EXPECT_TRUE(ShouldEnableQmMovingWaterTiles("", "", "axiom-cn", ""));
	EXPECT_TRUE(ShouldEnableQmMovingWaterTiles("", "", "", "Axiom"));

	EXPECT_FALSE(ShouldEnableQmMovingWaterTiles("DDRaceNetwork", "DDNet", "kog", "DDNet"));
	EXPECT_FALSE(ShouldEnableQmMovingWaterTiles(nullptr, nullptr, nullptr, nullptr));
}

TEST(QmLocalSkinSource, DdnetAndAxiomKeepTeeMenuOverride)
{
	EXPECT_FALSE(ShouldUseServerControlledLocalSkin("DDRaceNetwork", "", "", ""));
	EXPECT_FALSE(ShouldUseServerControlledLocalSkin("ddnet", "", "", ""));
	EXPECT_FALSE(ShouldUseServerControlledLocalSkin("", "DDNet", "", ""));
	EXPECT_FALSE(ShouldUseServerControlledLocalSkin("Gores", "Gores", "axiom-cn", ""));
	EXPECT_FALSE(ShouldUseServerControlledLocalSkin("Gores", "Gores", "", "Axiom"));
}

TEST(QmLocalSkinSource, OtherServersUseServerControlledSkin)
{
	EXPECT_TRUE(ShouldUseServerControlledLocalSkin("InfClass", "InfClass", "", ""));
	EXPECT_TRUE(ShouldUseServerControlledLocalSkin("MMO", "MMO", "", ""));
	EXPECT_TRUE(ShouldUseServerControlledLocalSkin("Gores", "Gores", "kog", "KoG"));
	EXPECT_TRUE(ShouldUseServerControlledLocalSkin(nullptr, nullptr, nullptr, nullptr));
}

TEST(LocalSkinSource, DemoPlaybackUsesRecordedSnapshotForEitherLocalConnection)
{
	constexpr int MainClientId = 7;
	constexpr int DummyClientId = 19;

	EXPECT_EQ(ResolveLocalSkinConfigIndex(true, MainClientId, MainClientId, DummyClientId), -1);
	EXPECT_EQ(ResolveLocalSkinConfigIndex(true, DummyClientId, MainClientId, DummyClientId), -1);
}

TEST(LocalSkinSource, OnlinePlayUsesMatchingLocalConfiguration)
{
	constexpr int MainClientId = 7;
	constexpr int DummyClientId = 19;

	EXPECT_EQ(ResolveLocalSkinConfigIndex(false, MainClientId, MainClientId, DummyClientId), 0);
	EXPECT_EQ(ResolveLocalSkinConfigIndex(false, DummyClientId, MainClientId, DummyClientId), 1);
	EXPECT_EQ(ResolveLocalSkinConfigIndex(false, 23, MainClientId, DummyClientId), -1);
	EXPECT_EQ(ResolveLocalSkinConfigIndex(false, -1, -1, -1), -1);
}

TEST(QmGoresMode, LinkedFastInputDirectlyFollowsGoresMode)
{
	bool Changed = false;
	EXPECT_TRUE(ApplyQmGoresLinkedConfig(true, true, false, Changed));
	EXPECT_TRUE(Changed);

	EXPECT_FALSE(ApplyQmGoresLinkedConfig(false, true, true, Changed));
	EXPECT_TRUE(Changed);

	EXPECT_TRUE(ApplyQmGoresLinkedConfig(true, true, true, Changed));
	EXPECT_FALSE(Changed);
}

TEST(QmGoresMode, UnlinkedFastInputConfigIsNotChanged)
{
	bool Changed = false;
	EXPECT_TRUE(ApplyQmGoresLinkedConfig(false, false, true, Changed));
	EXPECT_FALSE(Changed);

	EXPECT_FALSE(ApplyQmGoresLinkedConfig(true, false, false, Changed));
	EXPECT_FALSE(Changed);
}

TEST(QmFastInputMode, NormalizesLegacyModesToBestInput)
{
	EXPECT_EQ(QmFastInputNormalizedMode(0), 0);
	EXPECT_EQ(QmFastInputNormalizedMode(1), 3);
	EXPECT_EQ(QmFastInputNormalizedMode(2), 3);
	EXPECT_EQ(QmFastInputNormalizedMode(3), 3);
	EXPECT_EQ(QmFastInputNormalizedMode(4), 4);
}

TEST(QmFastInputMode, ComputesFastBestAndSaikoOffsets)
{
	SQmFastInputSettings Settings;
	Settings.m_Enabled = true;

	Settings.m_Mode = 0;
	Settings.m_FastAmountMs = 40;
	EXPECT_FLOAT_EQ(QmEffectiveFastInputOffsetTicks(Settings), 2.0f);

	Settings.m_Mode = 3;
	Settings.m_BestOffset = 250;
	Settings.m_BestSmoothing = 50;
	Settings.m_BestLatencyComp = 20;
	EXPECT_FLOAT_EQ(QmEffectiveFastInputOffsetTicks(Settings), 2.25f);

	Settings.m_Mode = 4;
	Settings.m_SaikoPlusAmount = 175;
	EXPECT_FLOAT_EQ(QmEffectiveFastInputOffsetTicks(Settings), 1.75f);
}

TEST(QmFastInputMode, PredictionTicksUseSaikoPlusExtraLocalTickOnly)
{
	EXPECT_EQ(QmFastInputPredictionTicks(0.01f, 0), 1);
	EXPECT_EQ(QmFastInputPredictionTicks(1.25f, 3), 2);
	EXPECT_EQ(QmFastInputPredictionTicks(1.25f, 4), 3);
	EXPECT_EQ(QmFastInputPredictionTicksOthers(1.25f, 4), 2);
}

TEST(QmFastInputMode, AppliesOffsetWithoutNegativeIntra)
{
	int Tick = 100;
	float Intra = 0.20f;
	QmApplyFastInputOffset(1.25f, Tick, Intra);
	EXPECT_EQ(Tick, 101);
	EXPECT_FLOAT_EQ(Intra, 0.45f);
}

TEST(QmFastInputMode, ChoosesOthersToggleByMode)
{
	EXPECT_FALSE(QmEffectiveFastInputOthers(false, 0, true, true, true));
	EXPECT_TRUE(QmEffectiveFastInputOthers(true, 0, true, false, false));
	EXPECT_TRUE(QmEffectiveFastInputOthers(true, 3, false, true, false));
	EXPECT_TRUE(QmEffectiveFastInputOthers(true, 4, false, false, true));
	EXPECT_FALSE(QmEffectiveFastInputOthers(true, 3, true, false, true));
	EXPECT_FALSE(QmEffectiveFastInputOthers(true, 4, true, true, false));
}

TEST(QmFastInputMode, MarginUsesLargestFastInputContribution)
{
	SQmFastInputSettings Settings;
	Settings.m_Enabled = true;
	Settings.m_BasePredictionMarginMs = 10;

	Settings.m_Mode = 0;
	Settings.m_FastAmountMs = 40;
	EXPECT_EQ(QmFastInputBasePredictionMarginMs(Settings), 40);

	Settings.m_Mode = 3;
	Settings.m_BestOffset = 250;
	EXPECT_EQ(QmFastInputBasePredictionMarginMs(Settings), 50);

	Settings.m_Mode = 4;
	Settings.m_SaikoPlusAmount = 175;
	EXPECT_EQ(QmFastInputBasePredictionMarginMs(Settings), 35);
}

TEST(QmFastInputMode, AutoPredictionMarginKeepsStableBase)
{
	EXPECT_EQ(QmComputeAutoPredictionMargin(10, 0.0f, 10.0f, 10.0f, 0.0f, false), 10);
}

TEST(QmFastInputMode, AutoPredictionMarginAddsLatencyJitterAndConnectionProtection)
{
	EXPECT_EQ(QmComputeAutoPredictionMargin(10, 70.0f, 10.0f, 10.0f, 0.0f, false), 20);
	EXPECT_EQ(QmComputeAutoPredictionMargin(10, 0.0f, 10.0f, 10.0f, 14.0f, false), 19);
	EXPECT_EQ(QmComputeAutoPredictionMargin(10, 0.0f, 10.0f, 10.0f, 0.0f, true), 20);
}

TEST(QmFastInputMode, AutoPredictionMarginClampsToSupportedRange)
{
	EXPECT_EQ(QmComputeAutoPredictionMargin(0, 0.0f, 0.0f, 0.0f, 0.0f, false), 1);
	EXPECT_EQ(QmComputeAutoPredictionMargin(500, 0.0f, 0.0f, 0.0f, 0.0f, false), 300);
}

TEST(QmGoresMode, ActiveGoresClearsDummyHammerState)
{
	bool Changed = false;
	EXPECT_EQ(ApplyQmGoresDummyHammerConfig(true, 1, Changed), 0);
	EXPECT_TRUE(Changed);

	EXPECT_EQ(ApplyQmGoresDummyHammerConfig(true, 0, Changed), 0);
	EXPECT_FALSE(Changed);

	EXPECT_EQ(ApplyQmGoresDummyHammerConfig(false, 1, Changed), 1);
	EXPECT_FALSE(Changed);
}

TEST(QmGoresMode, DummyHammerOverrideRestoresOnlyAutomaticChanges)
{
	SQmFocusConfigOverrideState State;
	bool Changed = false;
	EXPECT_EQ(ApplyQmGoresDummyHammerOverride(State, true, true, 1, Changed), 0);
	EXPECT_TRUE(Changed);
	EXPECT_EQ(ApplyQmGoresDummyHammerOverride(State, true, true, 0, Changed), 0);
	EXPECT_FALSE(Changed);
	EXPECT_EQ(ApplyQmGoresDummyHammerOverride(State, false, true, 0, Changed), 1);
	EXPECT_TRUE(Changed);

	State = {};
	EXPECT_EQ(ApplyQmGoresDummyHammerOverride(State, true, true, 1, Changed), 0);
	EXPECT_EQ(ApplyQmGoresDummyHammerOverride(State, true, true, 1, Changed), 1);
	EXPECT_FALSE(Changed);
	EXPECT_EQ(ApplyQmGoresDummyHammerOverride(State, false, true, 1, Changed), 1);
	EXPECT_FALSE(Changed);
}

TEST(QmGoresMode, HammerWakeupRequiresHeldHammerAndExternalWakeup)
{
	EXPECT_TRUE(ShouldTriggerQmGoresHammerWakeup(true, true, true));
	EXPECT_FALSE(ShouldTriggerQmGoresHammerWakeup(false, true, true));
	EXPECT_FALSE(ShouldTriggerQmGoresHammerWakeup(true, false, true));
	EXPECT_FALSE(ShouldTriggerQmGoresHammerWakeup(true, true, false));
}

TEST(QmGoresMode, KeepsHammerRequestWhileFrozen)
{
	EXPECT_TRUE(ShouldKeepQmGoresHammerInFreeze(true, true, true));
	EXPECT_FALSE(ShouldKeepQmGoresHammerInFreeze(false, true, true));
	EXPECT_FALSE(ShouldKeepQmGoresHammerInFreeze(true, false, true));
	EXPECT_FALSE(ShouldKeepQmGoresHammerInFreeze(true, true, false));
}

TEST(QmGoresMode, HammerWakeupFireStateCreatesNewPressWhileHeld)
{
	EXPECT_EQ(QmGoresHammerWakeupFireState(0), 1);
	EXPECT_EQ(QmGoresHammerWakeupFireState(1), 3);
	EXPECT_EQ(QmGoresHammerWakeupFireState(2), 3);
	EXPECT_EQ(QmGoresHammerWakeupFireState(3), 5);
}

TEST(QmGoresMode, HammerWakeupReleaseClearsOnlyPendingAutomaticPress)
{
	EXPECT_TRUE(ShouldReleaseQmGoresHammerWakeupFire(true, 1));
	EXPECT_TRUE(ShouldReleaseQmGoresHammerWakeupFire(true, 3));
	EXPECT_FALSE(ShouldReleaseQmGoresHammerWakeupFire(false, 1));
	EXPECT_FALSE(ShouldReleaseQmGoresHammerWakeupFire(true, 2));

	EXPECT_EQ(QmGoresHammerWakeupReleaseFireState(1), 2);
	EXPECT_EQ(QmGoresHammerWakeupReleaseFireState(3), 4);
}

TEST(QmGoresMode, RestoreWeaponAfterHammerUsesRecordedWeapon)
{
	EXPECT_EQ(GoresRestoreWeaponAfterHammer(WEAPON_LASER, true), WEAPON_LASER);
	EXPECT_EQ(GoresRestoreWeaponAfterHammer(WEAPON_GRENADE, true), WEAPON_GRENADE);
	EXPECT_EQ(GoresRestoreWeaponAfterHammer(WEAPON_GUN, false), WEAPON_GUN);
}

TEST(QmGoresMode, FireKeydownPulseRequiresActiveCycleAndNonHammerWeapon)
{
	EXPECT_TRUE(ShouldPulseGoresHammerOnFire(true, true, false, false));
	EXPECT_FALSE(ShouldPulseGoresHammerOnFire(false, true, false, false));
	EXPECT_FALSE(ShouldPulseGoresHammerOnFire(true, false, false, false));
	EXPECT_FALSE(ShouldPulseGoresHammerOnFire(true, true, true, false));
	EXPECT_FALSE(ShouldPulseGoresHammerOnFire(true, true, false, true));
}

TEST(QmGoresMode, RestoresRecordedWeaponEvenWhenTwoWeaponCycleIsInactive)
{
	EXPECT_TRUE(ShouldRestoreGoresWeaponAfterHammer(true, true));
	EXPECT_FALSE(ShouldRestoreGoresWeaponAfterHammer(false, true));
	EXPECT_FALSE(ShouldRestoreGoresWeaponAfterHammer(true, false));
}

TEST(QmNameplateHookStrongWeak, ScopeFiltersExpectedPlayers)
{
	EXPECT_TRUE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_SELF, true, false, false));
	EXPECT_FALSE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_SELF, false, true, false));

	EXPECT_FALSE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_OTHERS, true, false, false));
	EXPECT_TRUE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_OTHERS, false, true, false));
	EXPECT_TRUE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_OTHERS, false, false, true));

	EXPECT_TRUE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_STRONG, false, true, false));
	EXPECT_FALSE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_STRONG, false, false, true));

	EXPECT_FALSE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_WEAK, false, true, false));
	EXPECT_TRUE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_WEAK, false, false, true));

	EXPECT_TRUE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_ALL, true, false, false));
	EXPECT_TRUE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_ALL, false, true, false));
	EXPECT_FALSE(ShouldShowQmHookStrongWeakScope(99, false, true, false));
}

TEST(QmNameplateNameScope, OwnCharactersRespectCurrentAndLocalScopes)
{
	// 当前：只有当前操控角色显示自己的昵称（= 旧 cl_nameplates_own 行为）。
	EXPECT_TRUE(ShouldShowQmNameplateName(QM_NAMEPLATE_SHOW_SCOPE_CURRENT, true, true));
	// 当前：分身（本机但非当前角色）不显示。
	EXPECT_FALSE(ShouldShowQmNameplateName(QM_NAMEPLATE_SHOW_SCOPE_CURRENT, false, true));
	// 当前 + 本地：主号与分身都显示。
	EXPECT_TRUE(ShouldShowQmNameplateName(QM_NAMEPLATE_SHOW_SCOPE_LOCAL, false, true));
	// 本地 + 他人：当前操控角色不算在内。
	EXPECT_FALSE(ShouldShowQmNameplateName(QM_NAMEPLATE_SHOW_SCOPE_OTHERS_LOCAL, true, true));
	// 他人：只看别人，本机角色一律不显示。
	EXPECT_FALSE(ShouldShowQmNameplateName(QM_NAMEPLATE_SHOW_SCOPE_OTHERS, true, true));
	EXPECT_FALSE(ShouldShowQmNameplateName(QM_NAMEPLATE_SHOW_SCOPE_OTHERS, false, true));
}

TEST(QmNameplateNameScope, OtherPlayersRespectOthersAndAllScopes)
{
	// 他人：任何非本机玩家都显示（= 旧 cl_nameplates 行为）。
	EXPECT_TRUE(ShouldShowQmNameplateName(QM_NAMEPLATE_SHOW_SCOPE_OTHERS, false, false));
	// 本地 + 他人：非本机玩家同样显示。
	EXPECT_TRUE(ShouldShowQmNameplateName(QM_NAMEPLATE_SHOW_SCOPE_OTHERS_LOCAL, false, false));
	// 全体：三类玩家全显示。
	EXPECT_TRUE(ShouldShowQmNameplateName(QM_NAMEPLATE_SHOW_SCOPE_ALL, false, false));
	// 只覆盖本机角色的档位不能显示别人。
	EXPECT_FALSE(ShouldShowQmNameplateName(QM_NAMEPLATE_SHOW_SCOPE_CURRENT, false, false));
	EXPECT_FALSE(ShouldShowQmNameplateName(QM_NAMEPLATE_SHOW_SCOPE_LOCAL, false, false));
	// 关：谁都不显示；越界档位按关闭处理。
	EXPECT_FALSE(ShouldShowQmNameplateName(QM_NAMEPLATE_SHOW_SCOPE_OFF, true, true));
	EXPECT_FALSE(ShouldShowQmNameplateName(QM_NAMEPLATE_SHOW_SCOPE_OFF, false, false));
	EXPECT_FALSE(ShouldShowQmNameplateName(99, true, true));
	EXPECT_FALSE(ShouldShowQmNameplateName(99, false, false));
}

TEST(QmNameplateTextEffects, PlayingScopeSupportsSelfOthersFriendsAndAll)
{
	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_SELF, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, false, true, false, false, 0));
	EXPECT_FALSE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_SELF, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, false, false, true, false, 1));

	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OTHERS, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, false, false, false, false, 2));
	EXPECT_FALSE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OTHERS, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, false, true, false, false, 0));

	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_FRIENDS, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, false, false, true, false, 3));
	EXPECT_FALSE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_FRIENDS, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, false, true, false, false, 0));

	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_SELF_FRIENDS, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, false, true, false, false, 0));
	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_SELF_FRIENDS, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, false, false, true, false, 3));
	EXPECT_FALSE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_SELF_FRIENDS, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, false, false, false, false, 4));

	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_ALL, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, false, false, false, false, 4));
}

TEST(QmNameplateTextEffects, SpectateScopeDoesNotUsePlayingScope)
{
	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OFF, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_TARGET, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, true, false, false, true, 7));
	EXPECT_FALSE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_ALL, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, true, false, false, true, 7));

	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OFF, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OTHERS, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, true, false, false, false, 8));
	EXPECT_FALSE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OFF, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OTHERS, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, true, false, false, true, 7));

	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OFF, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_TARGET_FRIENDS, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, true, false, true, false, 8));
	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OFF, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_TARGET_FRIENDS, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, true, false, false, true, 7));
}

TEST(QmNameplateTextEffects, DemoModesOverridePlayingAndSpectateScopes)
{
	EXPECT_FALSE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_ALL, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_ALL, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, 5, true, true, true, true, true, 5));

	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OFF, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_SMART, -1, true, true, false, false, true, 5));
	EXPECT_FALSE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_ALL, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_ALL, QM_NAMEPLATE_TEXT_DEMO_MODE_SMART, -1, true, true, false, false, false, 6));

	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OFF, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_MANUAL_TARGET, 5, true, true, false, false, false, 5));
	EXPECT_FALSE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_ALL, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_ALL, QM_NAMEPLATE_TEXT_DEMO_MODE_MANUAL_TARGET, 5, true, true, true, true, true, 6));

	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_FRIENDS, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_MANUAL_SCOPE, -1, true, true, false, true, false, 6));
	EXPECT_FALSE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_FRIENDS, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_ALL, QM_NAMEPLATE_TEXT_DEMO_MODE_MANUAL_SCOPE, -1, true, true, false, false, true, 5));
}

TEST(QmGoresMode, BudgetedWorkConsumesAtMostBudget)
{
	int Cursor = 0;
	EXPECT_TRUE(ConsumeQmBudgetedWork(Cursor, 10, 3));
	EXPECT_EQ(Cursor, 3);

	EXPECT_TRUE(ConsumeQmBudgetedWork(Cursor, 10, 4));
	EXPECT_EQ(Cursor, 7);

	EXPECT_FALSE(ConsumeQmBudgetedWork(Cursor, 10, 8));
	EXPECT_EQ(Cursor, 10);
}

TEST(QmGoresMode, BudgetedWorkDoesNotAdvanceWithoutPositiveBudget)
{
	int Cursor = 2;
	EXPECT_TRUE(ConsumeQmBudgetedWork(Cursor, 5, 0));
	EXPECT_EQ(Cursor, 2);

	EXPECT_TRUE(ConsumeQmBudgetedWork(Cursor, 5, -4));
	EXPECT_EQ(Cursor, 2);

	EXPECT_FALSE(ConsumeQmBudgetedWork(Cursor, 2, 10));
	EXPECT_EQ(Cursor, 2);
}

TEST(QmFocusMode, ConfigOverrideRestoresOnlyAutoHiddenValues)
{
	SQmFocusConfigOverrideState State;
	bool Changed = false;

	int Value = ApplyQmFocusConfigOverride(State, true, 1, 0, Changed);
	EXPECT_TRUE(Changed);
	EXPECT_EQ(Value, 0);
	EXPECT_TRUE(State.m_WasActive);
	EXPECT_EQ(State.m_SavedValue, 1);

	Value = ApplyQmFocusConfigOverride(State, false, 0, 0, Changed);
	EXPECT_TRUE(Changed);
	EXPECT_EQ(Value, 1);
	EXPECT_FALSE(State.m_WasActive);
}

TEST(QmFocusMode, ConfigOverrideKeepsUserChangesMadeWhileActive)
{
	SQmFocusConfigOverrideState State;
	bool Changed = false;

	EXPECT_EQ(ApplyQmFocusConfigOverride(State, true, 1, 0, Changed), 0);
	EXPECT_TRUE(Changed);

	const int UserChangedValue = 2;
	EXPECT_EQ(ApplyQmFocusConfigOverride(State, false, UserChangedValue, 0, Changed), UserChangedValue);
	EXPECT_FALSE(Changed);
	EXPECT_FALSE(State.m_WasActive);
}

TEST(QmFocusMode, HudScoreboardNamesAndNameplatesRequireFocusModeAndTheirOwnToggle)
{
	EXPECT_TRUE(ShouldHideFocusHud(true, true));
	EXPECT_FALSE(ShouldHideFocusHud(true, false));
	EXPECT_FALSE(ShouldHideFocusHud(false, true));

	EXPECT_TRUE(ShouldHideFocusScoreboard(true, true));
	EXPECT_FALSE(ShouldHideFocusScoreboard(true, false));
	EXPECT_FALSE(ShouldHideFocusScoreboard(false, true));

	EXPECT_TRUE(ShouldHideFocusNames(true, true));
	EXPECT_FALSE(ShouldHideFocusNames(true, false));
	EXPECT_FALSE(ShouldHideFocusNames(false, true));

	EXPECT_TRUE(ShouldHideFocusNameplates(true, true));
	EXPECT_FALSE(ShouldHideFocusNameplates(true, false));
	EXPECT_FALSE(ShouldHideFocusNameplates(false, true));
}

TEST(QmFocusMode, SpectatorHudStaysVisibleWhenFocusModeAutoHidesMainHud)
{
	EXPECT_TRUE(ShouldRenderFocusSpectatorHud(true, true, false, true, true));
	EXPECT_TRUE(ShouldRenderFocusSpectatorHud(true, true, true, true, true));
	EXPECT_FALSE(ShouldRenderFocusSpectatorHud(false, true, false, true, true));
	EXPECT_FALSE(ShouldRenderFocusSpectatorHud(true, false, false, true, true));
	EXPECT_FALSE(ShouldRenderFocusSpectatorHud(true, true, false, false, true));
	EXPECT_FALSE(ShouldRenderFocusSpectatorHud(true, true, false, true, false));
}

TEST(QmFocusMode, VisualEffectChildrenDoNotInheritTheLegacyVisualParentToggle)
{
	EXPECT_FALSE(ShouldHideFocusJumpEffects(true, false));
	EXPECT_TRUE(ShouldHideFocusJumpEffects(true, true));
	EXPECT_FALSE(ShouldHideFocusKillEffects(true, false));
	EXPECT_TRUE(ShouldHideFocusKillEffects(true, true));
	EXPECT_FALSE(ShouldHideFocusExplosionEffects(true, false));
	EXPECT_TRUE(ShouldHideFocusExplosionEffects(true, true));
	EXPECT_FALSE(ShouldHideFocusFreezeEffects(true, false));
	EXPECT_TRUE(ShouldHideFocusFreezeEffects(true, true));
	EXPECT_FALSE(ShouldHideFocusFreezeEffects(false, true));
	EXPECT_FALSE(ShouldHideFocusHammerEffects(true, false));
	EXPECT_TRUE(ShouldHideFocusHammerEffects(true, true));
	EXPECT_FALSE(ShouldHideFocusHammerEffects(false, true));
	EXPECT_FALSE(ShouldHideFocusMuzzleEffects(true, false));
	EXPECT_TRUE(ShouldHideFocusMuzzleEffects(true, true));
	EXPECT_FALSE(ShouldHideFocusMuzzleEffects(false, true));
	EXPECT_FALSE(ShouldHideFocusJumpEffects(false, true));
	EXPECT_FALSE(ShouldHideFocusKillEffects(false, true));
	EXPECT_FALSE(ShouldHideFocusExplosionEffects(false, true));
}

TEST(QmFocusMode, UncheckedJumpEffectsStayVisibleInFocusMode)
{
	EXPECT_FALSE(ShouldHideFocusJumpEffects(true, false));
}

TEST(QmFocusMode, MapProgressAndInfoMessagesUseTheirOwnChildToggles)
{
	EXPECT_FALSE(ShouldHideFocusMapProgress(true, false));
	EXPECT_TRUE(ShouldHideFocusMapProgress(true, true));
	EXPECT_FALSE(ShouldHideFocusInfoMessages(true, false));
	EXPECT_TRUE(ShouldHideFocusInfoMessages(true, true));
	EXPECT_FALSE(ShouldHideFocusMapProgress(false, true));
	EXPECT_FALSE(ShouldHideFocusInfoMessages(false, true));
}

TEST(QmFocusMode, IndependentMapProgressUsesItsOwnToggleAndBottomStyle)
{
	EXPECT_FALSE(ShouldRenderMapProgressBar(false, 0, false, true));
	EXPECT_TRUE(ShouldRenderMapProgressBar(true, 1, false, true));
	EXPECT_FALSE(ShouldRenderMapProgressBar(true, 1, true, true));
	EXPECT_FALSE(ShouldRenderMapProgressBar(true, 0, false, false));
	EXPECT_TRUE(ShouldRenderMapProgressBar(true, 0, false, true));
}

TEST(QmFocusMode, JumpSoundMuteIsIndependentFromJumpVisualEffects)
{
	EXPECT_TRUE(ShouldPlayFocusJumpSound(true, false, true));
	EXPECT_FALSE(ShouldPlayFocusJumpSound(true, true, true));
	EXPECT_TRUE(ShouldPlayFocusJumpSound(false, true, true));
	EXPECT_FALSE(ShouldPlayFocusJumpSound(true, false, false));
}

TEST(QmFocusMode, DeathOrSpawnSoundUsesDeathSoundMuteToggle)
{
	EXPECT_TRUE(ShouldPlayFocusDeathOrSpawnSound(true, false, true));
	EXPECT_FALSE(ShouldPlayFocusDeathOrSpawnSound(true, true, true));
	EXPECT_TRUE(ShouldPlayFocusDeathOrSpawnSound(false, true, true));
	EXPECT_FALSE(ShouldPlayFocusDeathOrSpawnSound(true, false, false));
}

TEST(QmFocusMode, HammerSoundMuteRequiresFocusModeAndHammerSoundToggle)
{
	EXPECT_FALSE(ShouldMuteFocusHammerSounds(true, false));
	EXPECT_TRUE(ShouldMuteFocusHammerSounds(true, true));
	EXPECT_FALSE(ShouldMuteFocusHammerSounds(false, true));
}

TEST(QmFocusMode, AirJumpDecisionSeparatesParticlesAndSound)
{
	SQmAirJumpEffectDecision Decision = GetQmAirJumpEffectDecision(true, false, true, true);
	EXPECT_TRUE(Decision.m_SpawnParticles);
	EXPECT_FALSE(Decision.m_PlaySound);

	Decision = GetQmAirJumpEffectDecision(true, true, false, true);
	EXPECT_FALSE(Decision.m_SpawnParticles);
	EXPECT_TRUE(Decision.m_PlaySound);

	Decision = GetQmAirJumpEffectDecision(true, false, false, false);
	EXPECT_TRUE(Decision.m_SpawnParticles);
	EXPECT_FALSE(Decision.m_PlaySound);
}

TEST(QmFocusMode, DirectionIndicatorsAndGuideLinesAreControlledSeparately)
{
	EXPECT_TRUE(ShouldHideFocusDirectionIndicators(true, true));
	EXPECT_FALSE(ShouldHideFocusDirectionIndicators(true, false));
	EXPECT_FALSE(ShouldHideFocusDirectionIndicators(false, true));

	EXPECT_TRUE(ShouldHideFocusGuideLines(true, true));
	EXPECT_FALSE(ShouldHideFocusGuideLines(true, false));
	EXPECT_FALSE(ShouldHideFocusGuideLines(false, true));
}

TEST(QmFocusMode, UncheckedDirectionAndGuideIndicatorsStayVisibleInFocusMode)
{
	EXPECT_FALSE(ShouldHideFocusDirectionIndicators(true, false));
	EXPECT_FALSE(ShouldHideFocusGuideLines(true, false));
}

TEST(QmFocusMode, ForceVisibleClientLinesRemainVisibleWhenChatIsHidden)
{
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(true, true, true, true, -2, true, false));
	EXPECT_FALSE(ShouldRenderAnyFocusFilteredChat(true, true, true, true, false));
	EXPECT_TRUE(ShouldRenderAnyFocusFilteredChat(true, true, true, true, true));
}

TEST(QmFocusMode, ChatFiltersSeparatePlayerSystemAndEchoMessages)
{
	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(true, false, false, false, 3, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(true, false, false, false, -1, false, true));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(true, false, false, false, -1, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(true, false, false, false, -2, false, false));

	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(false, true, false, false, -1, false, true));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, true, false, false, -1, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, true, false, false, 3, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, true, false, false, -2, false, false));

	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(false, false, true, false, -1, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, false, true, false, -1, false, true));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, false, true, false, 3, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, false, true, false, -2, false, false));

	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(false, false, false, true, -2, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, false, false, true, 3, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, false, false, true, -1, false, false));
}

TEST(QmFocusMode, UnknownChatLinesFollowSystemMessageVisibility)
{
	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(false, false, true, false, -3, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(true, true, false, true, -3, false, false));
}

TEST(QmFocusMode, ChatAreaRendersWhenAnyMessageClassIsVisible)
{
	EXPECT_TRUE(ShouldRenderAnyFocusFilteredChat(false, true, true, true, false));
	EXPECT_TRUE(ShouldRenderAnyFocusFilteredChat(true, false, true, true, false));
	EXPECT_TRUE(ShouldRenderAnyFocusFilteredChat(true, true, false, true, false));
	EXPECT_TRUE(ShouldRenderAnyFocusFilteredChat(true, true, true, false, false));
}

TEST(QmFocusMode, ConfigSnapshotKeepsExplicitVisualChildrenIndependent)
{
	SQmFocusModeConfig Config;
	Config.m_FocusActive = true;

	const SQmFocusModeDecisions Decisions = GetQmFocusModeDecisions(Config);
	EXPECT_TRUE(Decisions.m_AirJump.m_SpawnParticles);
	EXPECT_TRUE(Decisions.m_AirJump.m_PlaySound);
	EXPECT_FALSE(Decisions.m_HideKillEffects);
	EXPECT_FALSE(Decisions.m_HideExplosionEffects);
	EXPECT_FALSE(Decisions.m_HideFreezeEffects);
	EXPECT_FALSE(Decisions.m_HideHammerEffects);
	EXPECT_FALSE(Decisions.m_HideMuzzleEffects);

	Config.m_HideMuzzleEffects = true;
	EXPECT_TRUE(GetQmFocusModeDecisions(Config).m_HideMuzzleEffects);
}

TEST(QmFocusMode, ConfigSnapshotSeparatesNameTextFromWholeNameplate)
{
	SQmFocusModeConfig Config;
	Config.m_FocusActive = true;
	Config.m_HideNames = true;

	SQmFocusModeDecisions Decisions = GetQmFocusModeDecisions(Config);
	EXPECT_TRUE(Decisions.m_HideNames);
	EXPECT_FALSE(Decisions.m_HideNameplates);

	Config.m_HideNames = false;
	Config.m_HideNameplates = true;
	Decisions = GetQmFocusModeDecisions(Config);
	EXPECT_FALSE(Decisions.m_HideNames);
	EXPECT_TRUE(Decisions.m_HideNameplates);
}

TEST(QmFocusMode, ConfigSnapshotSeparatesChatMessageClasses)
{
	SQmFocusModeConfig Config;
	Config.m_FocusActive = true;
	Config.m_HidePlayerMessages = true;
	Config.m_HideSystemInfoMessages = false;
	Config.m_HideSystemPromptMessages = true;
	Config.m_HideEchoMessages = true;
	Config.m_HideHud = true;
	Config.m_HideScoreboard = true;
	Config.m_HideNames = true;
	Config.m_HideNameplates = true;

	const SQmFocusModeDecisions Decisions = GetQmFocusModeDecisions(Config);
	EXPECT_TRUE(Decisions.m_HideHud);
	EXPECT_TRUE(Decisions.m_HideScoreboard);
	EXPECT_TRUE(Decisions.m_HideNames);
	EXPECT_TRUE(Decisions.m_HideNameplates);
	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(Decisions.m_HidePlayerMessages, Decisions.m_HideSystemInfoMessages, Decisions.m_HideSystemPromptMessages, Decisions.m_HideEchoMessages, 0, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(Decisions.m_HidePlayerMessages, Decisions.m_HideSystemInfoMessages, Decisions.m_HideSystemPromptMessages, Decisions.m_HideEchoMessages, -1, false, true));
	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(Decisions.m_HidePlayerMessages, Decisions.m_HideSystemInfoMessages, Decisions.m_HideSystemPromptMessages, Decisions.m_HideEchoMessages, -1, false, false));
	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(Decisions.m_HidePlayerMessages, Decisions.m_HideSystemInfoMessages, Decisions.m_HideSystemPromptMessages, Decisions.m_HideEchoMessages, -2, false, false));
}

TEST(QmFocusMode, ConfigSnapshotMapProgressRequiresStyleAndGoresProgressAndChildToggle)
{
	SQmFocusModeConfig Config;
	Config.m_FocusActive = true;
	Config.m_MapProgressEnabled = true;
	Config.m_MapProgressStyle = 0;
	Config.m_PlayerStatsHudEnabled = false;
	Config.m_GoresMapProgressEnabled = true;
	Config.m_HideMapProgress = false;

	EXPECT_TRUE(GetQmFocusModeDecisions(Config).m_RenderMapProgressBar);

	Config.m_MapProgressStyle = 1;
	EXPECT_TRUE(GetQmFocusModeDecisions(Config).m_RenderMapProgressBar);

	Config.m_PlayerStatsHudEnabled = true;
	EXPECT_FALSE(GetQmFocusModeDecisions(Config).m_RenderMapProgressBar);

	Config.m_MapProgressStyle = 0;
	Config.m_PlayerStatsHudEnabled = false;
	Config.m_HideMapProgress = true;
	EXPECT_FALSE(GetQmFocusModeDecisions(Config).m_RenderMapProgressBar);
}

TEST(QmTranslateUiSettings, DefaultColorsMatchSettingsPreviewDefaults)
{
	ExpectColorNear(color_cast<ColorRGBA>(ColorHSLA(DefaultConfig::QmTranslateBtnColorDisabled, true)), ColorRGBA(0.16f, 0.16f, 0.16f, 0.82f));
	ExpectColorNear(color_cast<ColorRGBA>(ColorHSLA(DefaultConfig::QmTranslateBtnColorEnabled, true)), ColorRGBA(0.35f, 0.45f, 0.70f, 0.90f));
	ExpectColorNear(color_cast<ColorRGBA>(ColorHSLA(DefaultConfig::QmTranslateMenuBgColor, true)), ColorRGBA(0.12f, 0.12f, 0.12f, 0.95f));
	ExpectColorNear(color_cast<ColorRGBA>(ColorHSLA(DefaultConfig::QmTranslateMenuOptionSelected, true)), ColorRGBA(0.35f, 0.45f, 0.70f, 0.90f));
	ExpectColorNear(color_cast<ColorRGBA>(ColorHSLA(DefaultConfig::QmTranslateMenuOptionNormal, true)), ColorRGBA(0.20f, 0.20f, 0.20f, 0.90f));
}

TEST(QmTranslateUiSettings, LegacyRgbColorsRestoreDeclaredAlpha)
{
	bool Migrated = false;
	unsigned Disabled = 0x005A6B7Cu;
	unsigned Enabled = 0x00010203u;
	unsigned Background = 0x00A1B2C3u;
	unsigned Selected = 0x00000000u;
	unsigned Normal = 0x00D4E5F6u;
	EXPECT_TRUE(NTranslateUiSettings::MigrateLegacyColorAlphas(Migrated, Disabled, Enabled, Background, Selected, Normal,
		DefaultConfig::QmTranslateBtnColorDisabled, DefaultConfig::QmTranslateBtnColorEnabled, DefaultConfig::QmTranslateMenuBgColor,
		DefaultConfig::QmTranslateMenuOptionSelected, DefaultConfig::QmTranslateMenuOptionNormal,
		EColorInputAlphaMode::PACKED, EColorInputAlphaMode::PACKED, EColorInputAlphaMode::PACKED, EColorInputAlphaMode::PACKED, EColorInputAlphaMode::PACKED));
	EXPECT_TRUE(Migrated);
	EXPECT_EQ(Disabled, 0xD15A6B7Cu);
	EXPECT_EQ(Enabled, 0xE6010203u);
	EXPECT_EQ(Background, 0xF2A1B2C3u);
	EXPECT_EQ(Selected, 0xE6000000u);
	EXPECT_EQ(Normal, 0xE6D4E5F6u);
}

TEST(QmTranslateUiSettings, AlphaAwareColorsAreNotChanged)
{
	bool Migrated = false;
	unsigned Disabled = 0x7F5A6B7Cu;
	unsigned Enabled = 0x805A6B7Cu;
	unsigned Background = 0x995A6B7Cu;
	unsigned Selected = 0xA05A6B7Cu;
	unsigned Normal = 0xB15A6B7Cu;
	EXPECT_TRUE(NTranslateUiSettings::MigrateLegacyColorAlphas(Migrated, Disabled, Enabled, Background, Selected, Normal,
		DefaultConfig::QmTranslateBtnColorDisabled, DefaultConfig::QmTranslateBtnColorEnabled, DefaultConfig::QmTranslateMenuBgColor,
		DefaultConfig::QmTranslateMenuOptionSelected, DefaultConfig::QmTranslateMenuOptionNormal,
		EColorInputAlphaMode::EXPLICIT, EColorInputAlphaMode::EXPLICIT, EColorInputAlphaMode::EXPLICIT, EColorInputAlphaMode::EXPLICIT, EColorInputAlphaMode::EXPLICIT));
	EXPECT_EQ(Disabled, 0x7F5A6B7Cu);
	EXPECT_EQ(Enabled, 0x805A6B7Cu);
	EXPECT_EQ(Background, 0x995A6B7Cu);
	EXPECT_EQ(Selected, 0xA05A6B7Cu);
	EXPECT_EQ(Normal, 0xB15A6B7Cu);
}

TEST(QmTranslateUiSettings, PackedColorsWithNonZeroAlphaAreNotChanged)
{
	bool Migrated = false;
	unsigned Disabled = 0x7F5A6B7Cu;
	unsigned Enabled = 0x805A6B7Cu;
	unsigned Background = 0x995A6B7Cu;
	unsigned Selected = 0xA05A6B7Cu;
	unsigned Normal = 0xB15A6B7Cu;
	EXPECT_TRUE(NTranslateUiSettings::MigrateLegacyColorAlphas(Migrated, Disabled, Enabled, Background, Selected, Normal,
		DefaultConfig::QmTranslateBtnColorDisabled, DefaultConfig::QmTranslateBtnColorEnabled, DefaultConfig::QmTranslateMenuBgColor,
		DefaultConfig::QmTranslateMenuOptionSelected, DefaultConfig::QmTranslateMenuOptionNormal,
		EColorInputAlphaMode::PACKED, EColorInputAlphaMode::PACKED, EColorInputAlphaMode::PACKED, EColorInputAlphaMode::PACKED, EColorInputAlphaMode::PACKED));
	EXPECT_EQ(Disabled, 0x7F5A6B7Cu);
	EXPECT_EQ(Enabled, 0x805A6B7Cu);
	EXPECT_EQ(Background, 0x995A6B7Cu);
	EXPECT_EQ(Selected, 0xA05A6B7Cu);
	EXPECT_EQ(Normal, 0xB15A6B7Cu);
}

TEST(QmTranslateUiSettings, ImplicitAlphaInputsRestoreDeclaredAlpha)
{
	bool Migrated = false;
	unsigned Disabled = 0xFF5A6B7Cu;
	unsigned Enabled = 0xFF010203u;
	unsigned Background = 0xFFA1B2C3u;
	unsigned Selected = 0xFF000000u;
	unsigned Normal = 0xFFD4E5F6u;
	EXPECT_TRUE(NTranslateUiSettings::MigrateLegacyColorAlphas(Migrated, Disabled, Enabled, Background, Selected, Normal,
		DefaultConfig::QmTranslateBtnColorDisabled, DefaultConfig::QmTranslateBtnColorEnabled, DefaultConfig::QmTranslateMenuBgColor,
		DefaultConfig::QmTranslateMenuOptionSelected, DefaultConfig::QmTranslateMenuOptionNormal,
		EColorInputAlphaMode::OMITTED, EColorInputAlphaMode::OMITTED, EColorInputAlphaMode::OMITTED, EColorInputAlphaMode::OMITTED, EColorInputAlphaMode::OMITTED));
	EXPECT_EQ(Disabled, 0xD15A6B7Cu);
	EXPECT_EQ(Enabled, 0xE6010203u);
	EXPECT_EQ(Background, 0xF2A1B2C3u);
	EXPECT_EQ(Selected, 0xE6000000u);
	EXPECT_EQ(Normal, 0xE6D4E5F6u);
}

TEST(QmTranslateUiSettings, ConfigManagerRecordsColorAlphaInputModes)
{
	struct SConfigRestore
	{
		CConfig m_Config = g_Config;
		~SConfigRestore() { g_Config = m_Config; }
	} ConfigRestore;
	CTestInfo TestInfo;
	std::unique_ptr<IStorage> pStorage = TestInfo.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);
	std::unique_ptr<IKernel> pKernel(IKernel::Create());
	pKernel->RegisterInterface(pStorage.get(), false);
	IConsole *pConsole = CreateConsole(CFGFLAG_CLIENT).release();
	pKernel->RegisterInterface(pConsole);
	IConfigManager *pConfigManager = CreateConfigManager();
	pKernel->RegisterInterface(pConfigManager);
	pConsole->Init();
	pConfigManager->Init();

	const auto MigrateDisabledColor = [pConfigManager]() {
		bool Migrated = false;
		unsigned Disabled = g_Config.m_QmTranslateBtnColorDisabled;
		unsigned Enabled = DefaultConfig::QmTranslateBtnColorEnabled;
		unsigned Background = DefaultConfig::QmTranslateMenuBgColor;
		unsigned Selected = DefaultConfig::QmTranslateMenuOptionSelected;
		unsigned Normal = DefaultConfig::QmTranslateMenuOptionNormal;
		EXPECT_TRUE(NTranslateUiSettings::MigrateLegacyColorAlphas(Migrated, Disabled, Enabled, Background, Selected, Normal,
			DefaultConfig::QmTranslateBtnColorDisabled, DefaultConfig::QmTranslateBtnColorEnabled, DefaultConfig::QmTranslateMenuBgColor,
			DefaultConfig::QmTranslateMenuOptionSelected, DefaultConfig::QmTranslateMenuOptionNormal,
			pConfigManager->ColorValueInputAlphaMode("qm_translate_btn_color_disabled"), EColorInputAlphaMode::EXPLICIT, EColorInputAlphaMode::EXPLICIT, EColorInputAlphaMode::EXPLICIT, EColorInputAlphaMode::EXPLICIT));
		return Disabled;
	};

	pConsole->ExecuteLine("qm_translate_btn_color_disabled $5A6B7C");
	EXPECT_EQ(pConfigManager->ColorValueInputAlphaMode("qm_translate_btn_color_disabled"), EColorInputAlphaMode::OMITTED);
	const unsigned OmittedRgb = g_Config.m_QmTranslateBtnColorDisabled;
	EXPECT_EQ(MigrateDisabledColor(), (OmittedRgb & ~NTranslateUiSettings::COLOR_ALPHA_MASK) | (DefaultConfig::QmTranslateBtnColorDisabled & NTranslateUiSettings::COLOR_ALPHA_MASK));

	pConsole->ExecuteLine("qm_translate_btn_color_disabled $ABC");
	EXPECT_EQ(pConfigManager->ColorValueInputAlphaMode("qm_translate_btn_color_disabled"), EColorInputAlphaMode::OMITTED);
	const unsigned OmittedShortRgb = g_Config.m_QmTranslateBtnColorDisabled;
	EXPECT_EQ(MigrateDisabledColor(), (OmittedShortRgb & ~NTranslateUiSettings::COLOR_ALPHA_MASK) | (DefaultConfig::QmTranslateBtnColorDisabled & NTranslateUiSettings::COLOR_ALPHA_MASK));

	pConsole->ExecuteLine("qm_translate_btn_color_disabled $5A6B7C7F");
	EXPECT_EQ(pConfigManager->ColorValueInputAlphaMode("qm_translate_btn_color_disabled"), EColorInputAlphaMode::EXPLICIT);
	const unsigned ExplicitAlpha = g_Config.m_QmTranslateBtnColorDisabled;
	EXPECT_EQ(MigrateDisabledColor(), ExplicitAlpha);

	pConsole->ExecuteLine("qm_translate_btn_color_disabled $ABCD");
	EXPECT_EQ(pConfigManager->ColorValueInputAlphaMode("qm_translate_btn_color_disabled"), EColorInputAlphaMode::EXPLICIT);
	const unsigned ExplicitShortAlpha = g_Config.m_QmTranslateBtnColorDisabled;
	EXPECT_EQ(MigrateDisabledColor(), ExplicitShortAlpha);

	pConsole->ExecuteLine("qm_translate_btn_color_disabled $5A6B7C00");
	EXPECT_EQ(pConfigManager->ColorValueInputAlphaMode("qm_translate_btn_color_disabled"), EColorInputAlphaMode::EXPLICIT);
	const unsigned ExplicitTransparent = g_Config.m_QmTranslateBtnColorDisabled;
	EXPECT_EQ(ExplicitTransparent & NTranslateUiSettings::COLOR_ALPHA_MASK, 0u);
	EXPECT_EQ(MigrateDisabledColor(), ExplicitTransparent);

	pConsole->ExecuteLine("qm_translate_btn_color_disabled red");
	EXPECT_EQ(pConfigManager->ColorValueInputAlphaMode("qm_translate_btn_color_disabled"), EColorInputAlphaMode::OMITTED);
	const unsigned NamedColor = g_Config.m_QmTranslateBtnColorDisabled;
	EXPECT_EQ(MigrateDisabledColor(), (NamedColor & ~NTranslateUiSettings::COLOR_ALPHA_MASK) | (DefaultConfig::QmTranslateBtnColorDisabled & NTranslateUiSettings::COLOR_ALPHA_MASK));

	pConsole->ExecuteLine("qm_translate_btn_color_disabled -16777216");
	EXPECT_EQ(pConfigManager->ColorValueInputAlphaMode("qm_translate_btn_color_disabled"), EColorInputAlphaMode::SIGNED_PACKED);
	EXPECT_EQ(MigrateDisabledColor() & NTranslateUiSettings::COLOR_ALPHA_MASK, DefaultConfig::QmTranslateBtnColorDisabled & NTranslateUiSettings::COLOR_ALPHA_MASK);

	pConsole->ExecuteLine("qm_translate_btn_color_disabled 2153407356");
	EXPECT_EQ(pConfigManager->ColorValueInputAlphaMode("qm_translate_btn_color_disabled"), EColorInputAlphaMode::PACKED);
	const unsigned UnsignedPackedAlpha = g_Config.m_QmTranslateBtnColorDisabled;
	EXPECT_NE(UnsignedPackedAlpha & NTranslateUiSettings::COLOR_ALPHA_MASK, 0u);
	EXPECT_EQ(MigrateDisabledColor(), UnsignedPackedAlpha);

	pConsole->ExecuteLine("qm_translate_btn_color_disabled +2153407356");
	EXPECT_EQ(pConfigManager->ColorValueInputAlphaMode("qm_translate_btn_color_disabled"), EColorInputAlphaMode::PACKED);
	EXPECT_EQ(MigrateDisabledColor(), g_Config.m_QmTranslateBtnColorDisabled);
}

TEST(QmTranslateUiSettings, MigrationMarkerPreservesIntentionalTransparentColor)
{
	bool Migrated = true;
	unsigned Disabled = 0x005A6B7Cu;
	unsigned Enabled = 0x00010203u;
	unsigned Background = 0x00A1B2C3u;
	unsigned Selected = 0x00000000u;
	unsigned Normal = 0x00D4E5F6u;
	EXPECT_FALSE(NTranslateUiSettings::MigrateLegacyColorAlphas(Migrated, Disabled, Enabled, Background, Selected, Normal,
		DefaultConfig::QmTranslateBtnColorDisabled, DefaultConfig::QmTranslateBtnColorEnabled, DefaultConfig::QmTranslateMenuBgColor,
		DefaultConfig::QmTranslateMenuOptionSelected, DefaultConfig::QmTranslateMenuOptionNormal));
	EXPECT_TRUE(Migrated);
	EXPECT_EQ(Disabled, 0x005A6B7Cu);
	EXPECT_EQ(Enabled, 0x00010203u);
	EXPECT_EQ(Background, 0x00A1B2C3u);
	EXPECT_EQ(Selected, 0x00000000u);
	EXPECT_EQ(Normal, 0x00D4E5F6u);
}
