#include "test.h"

#include <base/color.h>

#include <engine/console.h>
#include <engine/kernel.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <generated/protocol.h>

#include <game/client/components/emoticon.h>
#include <game/client/components/jump_hint_utils.h>
#include <game/client/components/qmclient/emoticon_projectile.h>
#include <game/client/components/qmclient/friend_enter_tracker.h>
#include <game/client/components/qmclient/map_progress.h>
#include <game/client/components/qmclient/modes.h>
#include <game/client/components/qmclient/translate/translate_ui_settings.h>

#include <gtest/gtest.h>

#include <limits>
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

TEST(QmFastInputMode, NormalizesLegacyBestModesToFastInput)
{
	EXPECT_EQ(QmFastInputNormalizedMode(0), 0);
	EXPECT_EQ(QmFastInputNormalizedMode(1), 0);
	EXPECT_EQ(QmFastInputNormalizedMode(2), 0);
	EXPECT_EQ(QmFastInputNormalizedMode(3), 0);
	EXPECT_EQ(QmFastInputNormalizedMode(4), 4);
}

TEST(QmFastInputMode, ComputesFastAndSaikoOffsets)
{
	SQmFastInputSettings Settings;
	Settings.m_Enabled = true;

	Settings.m_Mode = 0;
	Settings.m_FastAmountMs = 40;
	EXPECT_FLOAT_EQ(QmEffectiveFastInputOffsetTicks(Settings), 2.0f);

	// 历史 Best(3) 已删除，必须回落到 Fast 的偏移。
	Settings.m_Mode = 3;
	EXPECT_FLOAT_EQ(QmEffectiveFastInputOffsetTicks(Settings), 2.0f);

	Settings.m_Mode = 4;
	Settings.m_SaikoPlusAmount = 175;
	EXPECT_FLOAT_EQ(QmEffectiveFastInputOffsetTicks(Settings), 1.75f);
}

TEST(QmFastInputMode, PredictionTicksUseSaikoPlusExtraLocalTickOnly)
{
	EXPECT_EQ(QmFastInputPredictionTicks(0.01f, 0), 1);
	EXPECT_EQ(QmFastInputPredictionTicks(1.25f, 0), 2);
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
	EXPECT_FALSE(QmEffectiveFastInputOthers(false, 0, true, true));
	EXPECT_TRUE(QmEffectiveFastInputOthers(true, 0, true, false));
	EXPECT_TRUE(QmEffectiveFastInputOthers(true, 3, true, false));
	EXPECT_TRUE(QmEffectiveFastInputOthers(true, 4, false, true));
	EXPECT_FALSE(QmEffectiveFastInputOthers(true, 3, false, true));
	EXPECT_FALSE(QmEffectiveFastInputOthers(true, 4, true, false));
}

TEST(QmFastInputMode, MarginUsesLargestFastInputContribution)
{
	SQmFastInputSettings Settings;
	Settings.m_Enabled = true;
	Settings.m_BasePredictionMarginMs = 10;

	Settings.m_Mode = 0;
	Settings.m_FastAmountMs = 40;
	EXPECT_EQ(QmFastInputBasePredictionMarginMs(Settings), 40);

	// 历史 Best(3) 已删除，边距同样回落到 Fast 的贡献值。
	Settings.m_Mode = 3;
	EXPECT_EQ(QmFastInputBasePredictionMarginMs(Settings), 40);

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
	SQmConfigOverrideState State;
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

TEST(QmConfigOverride, ConfigOverrideRestoresOnlyAutoHiddenValues)
{
	SQmConfigOverrideState State;
	bool Changed = false;

	int Value = ApplyQmConfigOverride(State, true, 1, 0, Changed);
	EXPECT_TRUE(Changed);
	EXPECT_EQ(Value, 0);
	EXPECT_TRUE(State.m_WasActive);
	EXPECT_EQ(State.m_SavedValue, 1);

	Value = ApplyQmConfigOverride(State, false, 0, 0, Changed);
	EXPECT_TRUE(Changed);
	EXPECT_EQ(Value, 1);
	EXPECT_FALSE(State.m_WasActive);
}

TEST(QmConfigOverride, ConfigOverrideKeepsUserChangesMadeWhileActive)
{
	SQmConfigOverrideState State;
	bool Changed = false;

	EXPECT_EQ(ApplyQmConfigOverride(State, true, 1, 0, Changed), 0);
	EXPECT_TRUE(Changed);

	const int UserChangedValue = 2;
	EXPECT_EQ(ApplyQmConfigOverride(State, false, UserChangedValue, 0, Changed), UserChangedValue);
	EXPECT_FALSE(Changed);
	EXPECT_FALSE(State.m_WasActive);
}

TEST(QmMapProgress, IndependentMapProgressUsesItsOwnToggleAndBottomStyle)
{
	EXPECT_FALSE(ShouldRenderMapProgressBar(false, 0, false, true));
	EXPECT_TRUE(ShouldRenderMapProgressBar(true, 1, false, true));
	EXPECT_FALSE(ShouldRenderMapProgressBar(true, 1, true, true));
	EXPECT_FALSE(ShouldRenderMapProgressBar(true, 0, false, false));
	EXPECT_TRUE(ShouldRenderMapProgressBar(true, 0, false, true));
}

TEST(QmLoadingProgress, UsesSharedTeeProgressVisuals)
{
	const std::string MenusSource = ReadTestSourceFile("src/game/client/components/menus.cpp");
	const size_t LoadingPos = MenusSource.find("void CMenus::RenderLoading");
	ASSERT_NE(LoadingPos, std::string::npos);
	const size_t LoadingEnd = MenusSource.find("void CMenus::FinishLoading", LoadingPos);
	ASSERT_NE(LoadingEnd, std::string::npos);
	const std::string LoadingBody = MenusSource.substr(LoadingPos, LoadingEnd - LoadingPos);
	EXPECT_NE(LoadingBody.find("RenderProgressBarWithTee"), std::string::npos);
	EXPECT_EQ(LoadingBody.find("Ui()->RenderProgressBar"), std::string::npos);
	EXPECT_EQ(LoadingBody.find("m_QmPlayerStatsMapProgress &&"), std::string::npos);
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

TEST(QmJumpHint, DefaultsAreChineseAndDisabled)
{
	EXPECT_EQ(DefaultConfig::QmJumpHint, 0);
	EXPECT_EQ(DefaultConfig::QmJumpHintDefaultsMigrated, 0);
	EXPECT_STREQ(DefaultConfig::QmJumpHintText, "三格边缘跳:\\n左起跳: .34|.31|.16\\n左二段跳: .41|.28|.25|.13\\n右起跳: .63|.66|.81\\n右二段跳: .56|.69|.72|.84");
	EXPECT_STREQ(JUMP_HINT_DEFAULT_TEXT, DefaultConfig::QmJumpHintText);
}

TEST(QmJumpHint, UpgradeReplacesOriginalEnglishAndDisablesOnce)
{
	int Migrated = 0;
	int Enabled = 1;
	char aText[512] = "3 Tiles Edge Jump:\\nLeft Jump: .34|.31|.16\\nLeft Double Jump: .41|.28|.25|.13\\nRight Jump: .63|.66|.81\\nRight Double Jump: .56|.69|.72|.84";
	MigrateJumpHintDefaults(Migrated, Enabled, aText, sizeof(aText));
	EXPECT_EQ(Migrated, 1);
	EXPECT_EQ(Enabled, 0);
	EXPECT_STREQ(aText, JUMP_HINT_DEFAULT_TEXT);

	// 用户重新开启后，后续启动不得再次关闭。
	Enabled = 1;
	MigrateJumpHintDefaults(Migrated, Enabled, aText, sizeof(aText));
	EXPECT_EQ(Enabled, 1);
	EXPECT_STREQ(aText, JUMP_HINT_DEFAULT_TEXT);
}

TEST(QmJumpHint, UpgradePreservesCustomTextIncludingModifiedEnglish)
{
	for(const char *pText : {"我的三跳提示\\n保留这一行", "3 Tiles Edge Jump:\\nLeft Jump: .34", ""})
	{
		int Migrated = 0;
		int Enabled = 1;
		char aText[512];
		str_copy(aText, pText, sizeof(aText));
		MigrateJumpHintDefaults(Migrated, Enabled, aText, sizeof(aText));
		EXPECT_EQ(Migrated, 1);
		EXPECT_EQ(Enabled, 0);
		EXPECT_STREQ(aText, pText);
	}
}

TEST(QmJumpHint, CompletedMigrationPreservesSubsequentUserChoices)
{
	int Migrated = 1;
	int Enabled = 1;
	char aText[512] = "3 Tiles Edge Jump:\\nLeft Jump: .34|.31|.16\\nLeft Double Jump: .41|.28|.25|.13\\nRight Jump: .63|.66|.81\\nRight Double Jump: .56|.69|.72|.84";
	const std::string UserText = aText;
	MigrateJumpHintDefaults(Migrated, Enabled, aText, sizeof(aText));
	EXPECT_EQ(Enabled, 1);
	EXPECT_STREQ(aText, UserText.c_str());
}

TEST(QmEmoticon, InvalidRequestsConsumePendingSuperEmote)
{
	for(const int Emoticon : {-1, static_cast<int>(NUM_EMOTICONS), std::numeric_limits<int>::min(), std::numeric_limits<int>::max()})
	{
		bool SuperPending = true;
		EXPECT_EQ(QmEmoticon::ConsumeEffect(Emoticon, true, SuperPending), QmEmoticon::EEffect::INVALID);
		EXPECT_FALSE(SuperPending);
		EXPECT_EQ(QmEmoticon::ConsumeEffect(0, true, SuperPending), QmEmoticon::EEffect::PROJECTILE);
	}
}

TEST(QmEmoticon, ValidBoundaryIdsConsumeSuperEmoteOnce)
{
	for(const int Emoticon : {0, NUM_EMOTICONS - 1})
	{
		bool SuperPending = true;
		EXPECT_EQ(QmEmoticon::ConsumeEffect(Emoticon, false, SuperPending), QmEmoticon::EEffect::SUPER_HEAD);
		EXPECT_FALSE(SuperPending);
		EXPECT_EQ(QmEmoticon::ConsumeEffect(Emoticon, false, SuperPending), QmEmoticon::EEffect::NONE);
	}
}

TEST(QmEmoticon, LocalAndRemoteEffectsAgreeForEveryLaunchMode)
{
	struct SCase
	{
		bool m_Launch;
		bool m_Super;
		QmEmoticon::EEffect m_Expected;
	};
	const SCase aCases[] = {
		{false, false, QmEmoticon::EEffect::NONE},
		{false, true, QmEmoticon::EEffect::SUPER_HEAD},
		{true, false, QmEmoticon::EEffect::PROJECTILE},
		{true, true, QmEmoticon::EEffect::SUPER_PROJECTILE},
	};
	for(const auto &Case : aCases)
	{
		bool SuperPending = Case.m_Super;
		EXPECT_EQ(QmEmoticon::ConsumeEffect(4, Case.m_Launch, SuperPending), Case.m_Expected);
		EXPECT_EQ(QmEmoticon::ResolveRemoteEffect(4, Case.m_Launch, Case.m_Super, true, false, true, true), Case.m_Expected);
	}
}

TEST(QmEmoticon, GlobalAndPerPlayerMuteSuppressAllRemoteEffects)
{
	for(const bool Launch : {false, true})
	{
		for(const bool Super : {false, true})
		{
			EXPECT_EQ(QmEmoticon::ResolveRemoteEffect(4, Launch, Super, false, false, true, true), QmEmoticon::EEffect::NONE);
			EXPECT_EQ(QmEmoticon::ResolveRemoteEffect(4, Launch, Super, true, true, true, true), QmEmoticon::EEffect::NONE);
		}
	}
}

TEST(QmEmoticon, RemoteVisibilityFiltersHeadAndProjectileIndependently)
{
	for(const bool ShowSuper : {false, true})
	{
		for(const bool ShowLaunch : {false, true})
		{
			EXPECT_EQ(QmEmoticon::ResolveRemoteEffect(4, false, true, true, false, ShowSuper, ShowLaunch), ShowSuper ? QmEmoticon::EEffect::SUPER_HEAD : QmEmoticon::EEffect::NONE);
			EXPECT_EQ(QmEmoticon::ResolveRemoteEffect(4, true, false, true, false, ShowSuper, ShowLaunch), ShowLaunch ? QmEmoticon::EEffect::PROJECTILE : QmEmoticon::EEffect::NONE);
			EXPECT_EQ(QmEmoticon::ResolveRemoteEffect(4, true, true, true, false, ShowSuper, ShowLaunch), ShowLaunch ? QmEmoticon::EEffect::SUPER_PROJECTILE : QmEmoticon::EEffect::NONE);
		}
	}
}

TEST(QmFriendEnterTracker, InitialRosterIsSilentAndNewFriendEntersOnce)
{
	qm_friend_notify::CEnterTracker Tracker;
	const qm_friend_notify::CEnterTracker::CClient Existing{1, "Existing", "Clan", true, false};
	const qm_friend_notify::CEnterTracker::CClient NewFriend{2, "NewFriend", "Clan", true, false};
	EXPECT_TRUE(Tracker.Update({Existing}, 0.0, false).empty());
	EXPECT_EQ(Tracker.Update({Existing, NewFriend}, 0.2, false), std::vector<std::string>{"NewFriend"});
	EXPECT_TRUE(Tracker.Update({Existing, NewFriend}, 0.4, false).empty());
}

TEST(QmFriendEnterTracker, SameSlotIdentityAndFriendChangesAreSilent)
{
	qm_friend_notify::CEnterTracker Tracker;
	EXPECT_TRUE(Tracker.Update({{1, "Stranger", "OldClan", false, false}}, 0.0, false).empty());
	EXPECT_TRUE(Tracker.Update({{1, "Friend", "OldClan", true, false}}, 0.2, false).empty());
	EXPECT_TRUE(Tracker.Update({{1, "Friend", "NewClan", true, false}}, 0.4, false).empty());
	EXPECT_TRUE(Tracker.Update({{1, "Friend", "NewClan", false, false}}, 0.6, false).empty());
	EXPECT_TRUE(Tracker.Update({{1, "Friend", "NewClan", true, false}}, 0.8, false).empty());
}

TEST(QmFriendEnterTracker, ShortSnapshotAbsenceDoesNotReannounceFriend)
{
	qm_friend_notify::CEnterTracker Tracker;
	const qm_friend_notify::CEnterTracker::CClient Friend{1, "Friend", "Clan", true, false};
	EXPECT_TRUE(Tracker.Update({Friend}, 0.0, false).empty());
	EXPECT_TRUE(Tracker.Update({}, 1.0, false).empty());
	EXPECT_TRUE(Tracker.Update({}, 3.8, false).empty());
	EXPECT_TRUE(Tracker.Update({Friend}, 3.9, false).empty());
	EXPECT_TRUE(Tracker.Update({}, 4.0, false).empty());
	EXPECT_TRUE(Tracker.Update({Friend}, 6.9, false).empty());
}

TEST(QmFriendEnterTracker, ConfirmedAbsenceAllowsReentryAtGraceBoundary)
{
	qm_friend_notify::CEnterTracker Tracker;
	const qm_friend_notify::CEnterTracker::CClient Friend{1, "Friend", "Clan", true, false};
	EXPECT_TRUE(Tracker.Update({Friend}, 0.0, false).empty());
	EXPECT_TRUE(Tracker.Update({}, 1.0, false).empty());
	EXPECT_EQ(Tracker.Update({Friend}, 4.0, false), std::vector<std::string>{"Friend"});
	EXPECT_TRUE(Tracker.Update({Friend}, 4.2, false).empty());
}

TEST(QmFriendEnterTracker, ObservationPauseDoesNotCountAsAbsence)
{
	qm_friend_notify::CEnterTracker Tracker;
	const qm_friend_notify::CEnterTracker::CClient Friend{1, "Friend", "Clan", true, false};
	EXPECT_TRUE(Tracker.Update({Friend}, 0.0, false).empty());
	EXPECT_TRUE(Tracker.Update({Friend}, 100.0, false).empty());
	EXPECT_TRUE(Tracker.Update({}, 200.0, false).empty());
	EXPECT_TRUE(Tracker.Update({Friend}, 200.2, false).empty());
}

TEST(QmFriendEnterTracker, IdentityMovingSlotsWithinGraceIsSilent)
{
	qm_friend_notify::CEnterTracker Tracker;
	EXPECT_TRUE(Tracker.Update({{1, "Friend", "Clan", true, false}}, 0.0, false).empty());
	EXPECT_TRUE(Tracker.Update({}, 1.0, false).empty());
	EXPECT_TRUE(Tracker.Update({{2, "Friend", "Clan", true, false}}, 3.9, false).empty());
	EXPECT_EQ(Tracker.Update({{1, "NewFriend", "Clan", true, false}, {2, "Friend", "Clan", true, false}}, 4.0, false), std::vector<std::string>{"NewFriend"});
}

TEST(QmFriendEnterTracker, IdentityMovingSlotsAfterConfirmedAbsenceEnters)
{
	qm_friend_notify::CEnterTracker Tracker;
	EXPECT_TRUE(Tracker.Update({{1, "Friend", "Clan", true, false}}, 0.0, false).empty());
	EXPECT_TRUE(Tracker.Update({}, 1.0, false).empty());
	EXPECT_TRUE(Tracker.Update({}, 4.0, false).empty());
	EXPECT_EQ(Tracker.Update({{2, "Friend", "Clan", true, false}}, 4.2, false), std::vector<std::string>{"Friend"});
}

TEST(QmFriendEnterTracker, SlotMigrationRespectsIgnoreClan)
{
	for(const bool IgnoreClan : {false, true})
	{
		qm_friend_notify::CEnterTracker Tracker;
		EXPECT_TRUE(Tracker.Update({{1, "Friend", "OldClan", true, false}}, 0.0, IgnoreClan).empty());
		EXPECT_TRUE(Tracker.Update({}, 1.0, IgnoreClan).empty());
		const auto vNames = Tracker.Update({{2, "Friend", "NewClan", true, false}}, 1.2, IgnoreClan);
		EXPECT_EQ(vNames, IgnoreClan ? std::vector<std::string>{} : std::vector<std::string>{"Friend"});
	}
}

TEST(QmFriendEnterTracker, LocalPlayersAndNonFriendsDoNotNotify)
{
	qm_friend_notify::CEnterTracker Tracker;
	EXPECT_TRUE(Tracker.Update({}, 0.0, false).empty());
	EXPECT_TRUE(Tracker.Update({{1, "Main", "Clan", true, true}, {2, "Dummy", "Clan", true, true}, {3, "Stranger", "Clan", false, false}}, 0.2, false).empty());
	EXPECT_TRUE(Tracker.Update({{1, "Main", "Clan", true, false}, {2, "Dummy", "Clan", true, false}, {3, "Stranger", "Clan", true, false}}, 0.4, false).empty());
}

TEST(QmFriendEnterTracker, ResetBuildsANewSilentBaseline)
{
	qm_friend_notify::CEnterTracker Tracker;
	EXPECT_TRUE(Tracker.Update({}, 0.0, false).empty());
	EXPECT_EQ(Tracker.Update({{1, "Friend", "Clan", true, false}}, 0.2, false), std::vector<std::string>{"Friend"});
	Tracker.Reset();
	EXPECT_TRUE(Tracker.Update({{1, "Friend", "Clan", true, false}, {2, "Another", "Clan", true, false}}, 1.0, false).empty());
}

TEST(QmEmoticonProjectile, TransparentPixelsDoNotCollideAndRotationFollowsImage)
{
	unsigned char aPixels[4 * 4 * 4] = {};
	for(int Y = 0; Y < 4; ++Y)
		aPixels[(Y * 4 + 3) * 4 + 3] = 255;
	QmEmoticon::CAlphaMask Mask;
	Mask.Build(aPixels, 4, 4);
	const auto Wall = [](int X, int Y) { return X == 1 && Y == 0; };
	EXPECT_FALSE(Mask.Overlaps(vec2(16, 16), 32, pi, Wall));
	EXPECT_TRUE(Mask.Overlaps(vec2(20, 16), 32, 0, Wall));
	EXPECT_FALSE(Mask.Overlaps(vec2(20, 16), 32, pi, Wall));
	EXPECT_TRUE(Mask.Overlaps(vec2(16, 16), 64, 0, Wall));
}

TEST(QmEmoticonProjectile, EmptyAndHollowImagesPreserveTransparentAreas)
{
	unsigned char aPixels[3 * 3 * 4] = {};
	QmEmoticon::CAlphaMask Mask;
	const auto CenterTile = [](int X, int Y) { return X == 0 && Y == 0; };
	Mask.Build(aPixels, 3, 3);
	EXPECT_FALSE(Mask.Overlaps(vec2(16, 16), 96, 0, CenterTile));
	for(int I = 0; I < 9; ++I)
		aPixels[I * 4 + 3] = I == 4 ? 0 : 255;
	Mask.Build(aPixels, 3, 3);
	EXPECT_FALSE(Mask.Overlaps(vec2(16, 16), 96, 0, CenterTile));
	EXPECT_TRUE(Mask.Overlaps(vec2(18, 16), 96, 0, CenterTile));
}

TEST(QmEmoticonProjectile, FastProjectileCannotCrossOneTileWall)
{
	const unsigned char aPixel[] = {255, 255, 255, 255};
	QmEmoticon::CAlphaMask Mask;
	Mask.Build(aPixel, 1, 1);
	CEmoticonProjectile Projectile;
	Projectile.Init(vec2(-100, 16), vec2(1200, 0), 0);
	Projectile.m_AngVel = 0;
	const auto Wall = [](int X, int) { return X == 0; };
	Projectile.Update(0.2f, Mask, Wall);
	EXPECT_LT(Projectile.m_Pos.x, -31.9f);
	EXPECT_LT(Projectile.m_Vel.x, 0);
	EXPECT_FALSE(Mask.Overlaps(Projectile.m_Pos, Projectile.Size(), Projectile.m_Angle, Wall));
}

TEST(QmEmoticonProjectile, FramePartitionsHaveSameMotion)
{
	QmEmoticon::CAlphaMask Mask;
	CEmoticonProjectile First, Second;
	First.Init(vec2(0, 0), vec2(1200, -400), 0);
	Second = First;
	const auto Air = [](int, int) { return false; };
	for(int I = 0; I < 30; ++I)
		First.Update(1.0f / 30, Mask, Air);
	for(int I = 0; I < 144; ++I)
		Second.Update(1.0f / 144, Mask, Air);
	EXPECT_NEAR(First.m_Pos.x, Second.m_Pos.x, 0.01f);
	EXPECT_NEAR(First.m_Pos.y, Second.m_Pos.y, 0.01f);
}

TEST(QmEmoticonProjectile, PoolAlwaysAcceptsNewestLaunch)
{
	CEmoticonProjectile aProjectiles[2];
	EXPECT_EQ(QmEmoticon::ProjectileSlot(aProjectiles), &aProjectiles[0]);
	aProjectiles[0].Init(vec2(0, 0), vec2(0, 0), 0);
	EXPECT_EQ(QmEmoticon::ProjectileSlot(aProjectiles), &aProjectiles[1]);
	aProjectiles[1].Init(vec2(0, 0), vec2(0, 0), 1);
	aProjectiles[0].m_LifeTime = 1;
	EXPECT_EQ(QmEmoticon::ProjectileSlot(aProjectiles), &aProjectiles[0]);
}

TEST(QmEmoticonProjectile, LatestCursorSelectsWithoutRendering)
{
	EXPECT_EQ(QmEmoticon::SelectedSector(vec2(170, 0), 110, NUM_EMOTICONS), 0);
	EXPECT_EQ(QmEmoticon::SelectedSector(vec2(0, -170), 110, NUM_EMOTICONS), 12);
	EXPECT_EQ(QmEmoticon::SelectedSector(vec2(0, 0), 110, NUM_EMOTICONS), -1);
}

TEST(QmEmoticonProjectile, SuperProjectileStartsOutsideNearbyFloor)
{
	const unsigned char aPixel[] = {255, 255, 255, 255};
	QmEmoticon::CAlphaMask Mask;
	Mask.Build(aPixel, 1, 1);
	CEmoticonProjectile Projectile;
	Projectile.Init(vec2(0, -20), vec2(1200, -400), 0, 2.35f);
	const auto Floor = [](int, int Y) { return Y >= 0; };
	ASSERT_TRUE(Projectile.PlaceOutside(Mask, Floor));
	EXPECT_LE(Projectile.m_Pos.y + Projectile.Size() / 2, 0);
	EXPECT_FALSE(Mask.Overlaps(Projectile.m_Pos, Projectile.Size(), Projectile.m_Angle, Floor));
}

TEST(QmEmoticonProjectile, LifetimeEndsEvenAfterLongFrame)
{
	QmEmoticon::CAlphaMask Mask;
	CEmoticonProjectile Projectile;
	Projectile.Init(vec2(0, 0), vec2(1200, -400), 0);
	Projectile.Update(4, Mask, [](int, int) { return false; });
	EXPECT_FALSE(Projectile.m_Active);
}

TEST(QmEmoticonProjectile, FadeGrowthCannotForceImageThroughNarrowCorridor)
{
	const unsigned char aPixel[] = {255, 255, 255, 255};
	QmEmoticon::CAlphaMask Mask;
	Mask.Build(aPixel, 1, 1);
	CEmoticonProjectile Projectile;
	Projectile.Init(vec2(32, 32), vec2(0, 0), 0);
	Projectile.m_AngVel = 0;
	Projectile.m_LifeTime = 0.5f;
	const auto Corridor = [](int, int Y) { return Y < 0 || Y >= 2; };
	Projectile.Update(0.1f, Mask, Corridor);
	EXPECT_TRUE(Projectile.m_Active);
	EXPECT_FLOAT_EQ(Projectile.Size(), 64);
	EXPECT_FALSE(Mask.Overlaps(Projectile.m_Pos, Projectile.Size(), Projectile.m_Angle, Corridor));
}
