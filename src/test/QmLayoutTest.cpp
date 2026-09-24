// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "test.h"

#include <game/client/QmUi/QmLayout.h>
#include <game/client/components/qmclient/afk_presentation.h>
#include <game/client/components/qmclient/input_overlay.h>
#include <game/client/components/qmclient/score_hud_layout.h>
#include <game/client/components/qmclient/scoreboard_footer.h>
#include <game/client/components/qmclient/scoreboard_skin.h>
#include <game/client/components/qmclient/scoreboard_team_modes.h>
#include <game/client/components/qmclient/tee_skin_apply.h>
#include <game/client/components/scoreboard.h>
#include <game/map/render_map.h>
#include <game/mapitems.h>

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

TEST(QmTuneColorMapper, NonArrayBackendsKeepTheOriginalTuneTileIndex)
{
	CTuneColorMapper Mapper;
	EXPECT_EQ(Mapper.TileTextureIndex(TILE_TUNE, 7, false), TILE_TUNE);
	EXPECT_EQ(Mapper.TileTextureIndex(TILE_TUNE, 0, true), TILE_TUNE);
	EXPECT_EQ(Mapper.TileTextureIndex(TILE_TUNE, 7, true), 1);
}

TEST(QmInputOverlayLayout, MouseClassificationRequiresMouseOnlyInputs)
{
	EXPECT_TRUE(QmInputOverlay::IsMouseOnlyLayout(false, true));
	EXPECT_FALSE(QmInputOverlay::IsMouseOnlyLayout(true, false));
	EXPECT_FALSE(QmInputOverlay::IsMouseOnlyLayout(true, true));
	EXPECT_FALSE(QmInputOverlay::IsMouseOnlyLayout(false, false));
}

TEST(QmInputOverlayLayout, MouseSizeDoesNotMoveKeyboardOrMouseAnchor)
{
	constexpr float KeyboardScale = 0.5f;
	const auto Keyboard = QmInputOverlay::ScaledLayoutBounds(0.0f, 0.0f, 432.0f, 300.0f, KeyboardScale, KeyboardScale);
	const auto SmallMouse = QmInputOverlay::ScaledLayoutBounds(467.0f, 0.0f, 285.0f, 421.0f, KeyboardScale, 0.1f);
	const auto LargeMouse = QmInputOverlay::ScaledLayoutBounds(467.0f, 0.0f, 285.0f, 421.0f, KeyboardScale, 0.5f);

	EXPECT_FLOAT_EQ(Keyboard.m_MinX, 0.0f);
	EXPECT_FLOAT_EQ(Keyboard.m_MaxX, 216.0f);
	EXPECT_FLOAT_EQ(SmallMouse.m_MinX, LargeMouse.m_MinX);
	EXPECT_FLOAT_EQ(SmallMouse.m_MinX - Keyboard.m_MaxX, 17.5f);
	EXPECT_FLOAT_EQ(SmallMouse.m_MaxX - SmallMouse.m_MinX, 28.5f);
	EXPECT_FLOAT_EQ(LargeMouse.m_MaxX - LargeMouse.m_MinX, 142.5f);
}

TEST(QmInputOverlayLayout, VisibleBoundsUseIndependentContentScales)
{
	constexpr float KeyboardScale = 0.5f;
	const auto Keyboard = QmInputOverlay::ScaledLayoutBounds(0.0f, 0.0f, 432.0f, 300.0f, KeyboardScale, KeyboardScale);
	const auto SmallMouse = QmInputOverlay::ScaledLayoutBounds(467.0f, 0.0f, 285.0f, 421.0f, KeyboardScale, 0.25f);
	const auto LargeMouse = QmInputOverlay::ScaledLayoutBounds(467.0f, 0.0f, 285.0f, 421.0f, KeyboardScale, 0.5f);

	const auto SmallBounds = QmInputOverlay::UnionBounds(Keyboard, SmallMouse);
	EXPECT_FLOAT_EQ(SmallBounds.m_MinX, 0.0f);
	EXPECT_FLOAT_EQ(SmallBounds.m_MinY, 0.0f);
	EXPECT_FLOAT_EQ(SmallBounds.m_MaxX, 304.75f);
	EXPECT_FLOAT_EQ(SmallBounds.m_MaxY, 150.0f);

	const auto LargeBounds = QmInputOverlay::UnionBounds(Keyboard, LargeMouse);
	EXPECT_FLOAT_EQ(LargeBounds.m_MaxX, 376.0f);
	EXPECT_FLOAT_EQ(LargeBounds.m_MaxY, 210.5f);
}

TEST(QmAfkPresentation, ServerAndEscMenuStatesRemainAvailableForNonOpacityIndicators)
{
	EXPECT_TRUE(IsQmAfkForPresentation(true, false, false, 7, 3));
	EXPECT_TRUE(IsQmAfkForPresentation(false, true, true, 3, 3));

	EXPECT_FALSE(IsQmAfkForPresentation(false, true, false, 3, 3));
	EXPECT_FALSE(IsQmAfkForPresentation(false, true, true, 4, 3));
	EXPECT_FALSE(IsQmAfkForPresentation(false, false, true, 3, 3));
	EXPECT_FALSE(IsQmAfkForPresentation(false, true, true, -1, -1));
}

TEST(QmAfkPresentation, AfkStateDoesNotChangeTeeHookOrNameplateOpacity)
{
	const std::string Header = ReadTestSourceFile("src/game/client/components/qmclient/afk_presentation.h");
	const std::string Players = ReadTestSourceFile("src/game/client/components/players.cpp");
	const std::string Nameplates = ReadTestSourceFile("src/game/client/components/nameplates.cpp");

	EXPECT_EQ(Header.find("QM_AFK_PRESENTATION_ALPHA"), std::string::npos);
	EXPECT_EQ(Header.find("ApplyQmAfkPresentationAlpha"), std::string::npos);
	EXPECT_EQ(Players.find("ApplyQmAfkPresentationAlpha"), std::string::npos);
	EXPECT_EQ(Players.find("Afk ? Alpha : 1.0f"), std::string::npos);
	EXPECT_EQ(Nameplates.find("ApplyQmAfkPresentationAlpha"), std::string::npos);
}

TEST(QmScoreboardTeamModes, AggregationRequiresDisplayInfoAndCombinesKnownMembers)
{
	SQmScoreboardTeamModeState State;
	AccumulateQmScoreboardTeamModeState(State, false, CHARACTERFLAG_PRACTICE_MODE | CHARACTERFLAG_LOCK_MODE);
	EXPECT_FALSE(State.m_Known);
	EXPECT_EQ(State.m_Flags, 0);
	SQmScoreboardTeamModeState KnownEmptyState;
	AccumulateQmScoreboardTeamModeState(KnownEmptyState, true, 0);
	EXPECT_TRUE(KnownEmptyState.m_Known);
	EXPECT_EQ(KnownEmptyState.m_Flags, 0);

	AccumulateQmScoreboardTeamModeState(State, true, CHARACTERFLAG_PRACTICE_MODE | CHARACTERFLAG_SOLO);
	EXPECT_TRUE(State.m_Known);
	EXPECT_TRUE(State.Practice());
	EXPECT_FALSE(State.Team0Mode());
	EXPECT_FALSE(State.Locked());
	EXPECT_EQ(State.m_Flags & CHARACTERFLAG_SOLO, 0);

	AccumulateQmScoreboardTeamModeState(State, true, CHARACTERFLAG_TEAM0_MODE | CHARACTERFLAG_LOCK_MODE);
	EXPECT_TRUE(State.Practice());
	EXPECT_TRUE(State.Team0Mode());
	EXPECT_TRUE(State.Locked());
}

TEST(QmScoreboardTeamModes, SpecPlayersKeepTheirScoreboardTeamAndLastKnownModeState)
{
	EXPECT_EQ(QmScoreboardEffectivePlayerTeam(TEAM_GAME, false, false), TEAM_GAME);
	EXPECT_EQ(QmScoreboardEffectivePlayerTeam(TEAM_SPECTATORS, true, false), TEAM_GAME);
	EXPECT_EQ(QmScoreboardEffectivePlayerTeam(TEAM_SPECTATORS, false, false), TEAM_SPECTATORS);
	EXPECT_EQ(QmScoreboardEffectivePlayerTeam(TEAM_SPECTATORS, true, true), TEAM_SPECTATORS);

	constexpr int DdTeam = 3;
	std::array<SQmScoreboardTeamModeState, NUM_DDRACE_TEAMS> aTeamModes{};
	std::array<SQmScoreboardTeamModeState, NUM_DDRACE_TEAMS> aCachedTeamModes{};
	std::array<bool, NUM_DDRACE_TEAMS> aTeamHasSpecPlayer{};

	aTeamModes[DdTeam].m_Known = true;
	aTeamModes[DdTeam].m_Flags = CHARACTERFLAG_PRACTICE_MODE | CHARACTERFLAG_LOCK_MODE;
	CacheAndRestoreQmScoreboardTeamModes(aTeamModes, aTeamHasSpecPlayer, aCachedTeamModes);
	EXPECT_TRUE(aCachedTeamModes[DdTeam].Practice());
	EXPECT_TRUE(aCachedTeamModes[DdTeam].Locked());

	aTeamModes = {};
	aTeamHasSpecPlayer[DdTeam] = true;
	CacheAndRestoreQmScoreboardTeamModes(aTeamModes, aTeamHasSpecPlayer, aCachedTeamModes);
	EXPECT_TRUE(aTeamModes[DdTeam].m_Known);
	EXPECT_TRUE(aTeamModes[DdTeam].Practice());
	EXPECT_TRUE(aTeamModes[DdTeam].Locked());

	aTeamModes = {};
	aTeamHasSpecPlayer = {};
	CacheAndRestoreQmScoreboardTeamModes(aTeamModes, aTeamHasSpecPlayer, aCachedTeamModes);
	EXPECT_FALSE(aTeamModes[DdTeam].m_Known);
}

TEST(UiV2Layout, RowPaddingGapAndPosition)
{
	CUiV2LayoutEngine Engine;
	SUiStyle ContainerStyle;
	ContainerStyle.m_Axis = EUiAxis::ROW;
	ContainerStyle.m_Gap = 5.0f;
	ContainerStyle.m_Padding = {10.0f, 10.0f, 10.0f, 10.0f};
	ContainerStyle.m_AlignItems = EUiAlign::START;

	SUiLayoutBox ContainerBox{0.0f, 0.0f, 200.0f, 100.0f};

	std::vector<SUiLayoutChild> vChildren(2);
	vChildren[0].m_Style.m_Width = SUiLength::Px(50.0f);
	vChildren[0].m_Style.m_Height = SUiLength::Px(20.0f);
	vChildren[1].m_Style.m_Width = SUiLength::Px(50.0f);
	vChildren[1].m_Style.m_Height = SUiLength::Px(20.0f);

	Engine.ComputeChildren(ContainerStyle, ContainerBox, vChildren);

	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_X, 10.0f);
	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_Y, 10.0f);
	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_W, 50.0f);
	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_H, 20.0f);

	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_X, 65.0f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_Y, 10.0f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_W, 50.0f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_H, 20.0f);
}

TEST(UiV2Layout, RowFlexDistribution)
{
	CUiV2LayoutEngine Engine;
	SUiStyle ContainerStyle;
	ContainerStyle.m_Axis = EUiAxis::ROW;
	ContainerStyle.m_Gap = 10.0f;

	SUiLayoutBox ContainerBox{0.0f, 0.0f, 230.0f, 40.0f};

	std::vector<SUiLayoutChild> vChildren(3);
	vChildren[0].m_Style.m_Width = SUiLength::Flex(1.0f);
	vChildren[0].m_Style.m_Height = SUiLength::Px(20.0f);
	vChildren[1].m_Style.m_Width = SUiLength::Flex(2.0f);
	vChildren[1].m_Style.m_Height = SUiLength::Px(20.0f);
	vChildren[2].m_Style.m_Width = SUiLength::Px(30.0f);
	vChildren[2].m_Style.m_Height = SUiLength::Px(20.0f);

	Engine.ComputeChildren(ContainerStyle, ContainerBox, vChildren);

	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_W, 60.0f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_W, 120.0f);
	EXPECT_FLOAT_EQ(vChildren[2].m_Box.m_W, 30.0f);
	EXPECT_FLOAT_EQ(vChildren[2].m_Box.m_X, 200.0f);
}

TEST(UiV2Layout, ColumnJustifyCenter)
{
	CUiV2LayoutEngine Engine;
	SUiStyle ContainerStyle;
	ContainerStyle.m_Axis = EUiAxis::COLUMN;
	ContainerStyle.m_Gap = 10.0f;
	ContainerStyle.m_JustifyContent = EUiAlign::CENTER;

	SUiLayoutBox ContainerBox{0.0f, 0.0f, 80.0f, 100.0f};

	std::vector<SUiLayoutChild> vChildren(2);
	vChildren[0].m_Style.m_Width = SUiLength::Px(20.0f);
	vChildren[0].m_Style.m_Height = SUiLength::Px(20.0f);
	vChildren[1].m_Style.m_Width = SUiLength::Px(20.0f);
	vChildren[1].m_Style.m_Height = SUiLength::Px(20.0f);

	Engine.ComputeChildren(ContainerStyle, ContainerBox, vChildren);

	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_Y, 25.0f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_Y, 55.0f);
}

TEST(UiV2Layout, AlignStretchExpandsCrossAxis)
{
	CUiV2LayoutEngine Engine;
	SUiStyle ContainerStyle;
	ContainerStyle.m_Axis = EUiAxis::ROW;
	ContainerStyle.m_Padding = {0.0f, 10.0f, 0.0f, 10.0f};
	ContainerStyle.m_AlignItems = EUiAlign::STRETCH;

	SUiLayoutBox ContainerBox{0.0f, 0.0f, 100.0f, 100.0f};

	std::vector<SUiLayoutChild> vChildren(1);
	vChildren[0].m_Style.m_Width = SUiLength::Px(20.0f);
	vChildren[0].m_Style.m_Height = SUiLength::Auto();

	Engine.ComputeChildren(ContainerStyle, ContainerBox, vChildren);

	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_H, 80.0f);
	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_Y, 10.0f);
}

TEST(UiV2Layout, ApplyConstraintsMinMaxPercent)
{
	CUiV2LayoutEngine Engine;
	SUiStyle Style;
	Style.m_Width = SUiLength::Percent(0.5f);
	Style.m_Height = SUiLength::Px(100.0f);
	Style.m_MinWidth = SUiLength::Px(120.0f);
	Style.m_MaxWidth = SUiLength::Px(180.0f);
	Style.m_MaxHeight = SUiLength::Px(70.0f);

	SUiLayoutBox Parent{0.0f, 0.0f, 300.0f, 300.0f};
	const SUiLayoutBox Box = Engine.ApplyConstraints(Style, Parent);

	EXPECT_FLOAT_EQ(Box.m_W, 150.0f);
	EXPECT_FLOAT_EQ(Box.m_H, 70.0f);
}

TEST(UiV2Layout, ScoreboardTeamColumnsWithGap)
{
	CUiV2LayoutEngine Engine;
	SUiStyle ContainerStyle;
	ContainerStyle.m_Axis = EUiAxis::ROW;
	ContainerStyle.m_Gap = 7.5f;
	ContainerStyle.m_AlignItems = EUiAlign::STRETCH;

	SUiLayoutBox ContainerBox{0.0f, 0.0f, 850.0f, 385.0f};

	std::vector<SUiLayoutChild> vChildren(2);
	vChildren[0].m_Style.m_Width = SUiLength::Flex(1.0f);
	vChildren[1].m_Style.m_Width = SUiLength::Flex(1.0f);

	Engine.ComputeChildren(ContainerStyle, ContainerBox, vChildren);

	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_X, 0.0f);
	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_W, 421.25f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_X, 428.75f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_W, 421.25f);
}

TEST(UiV2Layout, ScoreboardThreeColumnsEqualWidth)
{
	CUiV2LayoutEngine Engine;
	SUiStyle ContainerStyle;
	ContainerStyle.m_Axis = EUiAxis::ROW;
	ContainerStyle.m_AlignItems = EUiAlign::STRETCH;

	SUiLayoutBox ContainerBox{0.0f, 0.0f, 900.0f, 320.0f};

	std::vector<SUiLayoutChild> vChildren(3);
	vChildren[0].m_Style.m_Width = SUiLength::Flex(1.0f);
	vChildren[1].m_Style.m_Width = SUiLength::Flex(1.0f);
	vChildren[2].m_Style.m_Width = SUiLength::Flex(1.0f);

	Engine.ComputeChildren(ContainerStyle, ContainerBox, vChildren);

	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_W, 300.0f);
	EXPECT_FLOAT_EQ(vChildren[1].m_Box.m_X, 300.0f);
	EXPECT_FLOAT_EQ(vChildren[2].m_Box.m_X, 600.0f);
}

TEST(UiV2Layout, ScoreboardSoundMuteVerticalButtons)
{
	CUiV2LayoutEngine Engine;
	SUiStyle ContainerStyle;
	ContainerStyle.m_Axis = EUiAxis::COLUMN;
	ContainerStyle.m_Gap = 4.0f;
	ContainerStyle.m_AlignItems = EUiAlign::STRETCH;

	SUiLayoutBox ContainerBox{0.0f, 0.0f, 22.0f, 230.0f};

	std::vector<SUiLayoutChild> vChildren(9);
	for(SUiLayoutChild &Child : vChildren)
	{
		Child.m_Style.m_Height = SUiLength::Px(22.0f);
	}

	Engine.ComputeChildren(ContainerStyle, ContainerBox, vChildren);

	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_X, 0.0f);
	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_W, 22.0f);
	EXPECT_FLOAT_EQ(vChildren[0].m_Box.m_Y, 0.0f);
	EXPECT_FLOAT_EQ(vChildren[8].m_Box.m_Y, 208.0f);
}

TEST(QmGaussianBlurRender, TargetUsesQuarterResolutionAndRoundsUp)
{
	EXPECT_EQ(UiGaussianBlurTargetDimension(1920), 480);
	EXPECT_EQ(UiGaussianBlurTargetDimension(1080), 270);
	EXPECT_EQ(UiGaussianBlurTargetDimension(1081), 271);
	EXPECT_EQ(UiGaussianBlurTargetDimension(1), 1);
	EXPECT_EQ(UiGaussianBlurTargetDimension(0), 0);
}

TEST(QmScoreboardRender, PlayerRowsAlwaysUseFullDetail)
{
	const SScoreboardRowRenderDetail Detail = ResolveScoreboardRowRenderDetail();
	EXPECT_TRUE(Detail.m_FullTee);
	EXPECT_TRUE(Detail.m_ShowClientBrand);
	EXPECT_TRUE(Detail.m_ShowClan);
	EXPECT_TRUE(Detail.m_ShowCountry);
}

TEST(QmScoreboardRender, DdTeamLabelUsesBelowRowLayoutRegardlessOfColumnCount)
{
	const SScoreboardTeamLabelLayout SingleColumn = ResolveScoreboardTeamLabelLayout(20.0f, 40.0f, 30.0f, 8.0f, 8.0f, 0.0f, true);
	const SScoreboardTeamLabelLayout MultiColumn = ResolveScoreboardTeamLabelLayout(220.0f, 40.0f, 30.0f, 2.5f, 8.0f, 0.0f, true);

	EXPECT_FLOAT_EQ(SingleColumn.m_X, 25.0f);
	EXPECT_FLOAT_EQ(SingleColumn.m_Y, 70.0f);
	EXPECT_FLOAT_EQ(MultiColumn.m_X, 225.0f);
	EXPECT_FLOAT_EQ(MultiColumn.m_Y, 70.0f);
	EXPECT_FLOAT_EQ(SingleColumn.m_RowSpacing, 8.0f);
	EXPECT_FLOAT_EQ(MultiColumn.m_RowSpacing, 8.0f);

	// A DDTeam that continues in the next column must not reserve or render a duplicate label.
	const SScoreboardTeamLabelLayout ContinuedTeam = ResolveScoreboardTeamLabelLayout(20.0f, 40.0f, 30.0f, 2.5f, 8.0f, SCOREBOARD_TEAM_MODE_ICON_SIZE, false);
	EXPECT_FLOAT_EQ(ContinuedTeam.m_RowSpacing, 2.5f);
}

TEST(QmScoreboardRender, DdTeamModeIconsUseNativeHudSizeAndCenteredLabelLayout)
{
	const SScoreboardTeamLabelLayout Layout = ResolveScoreboardTeamLabelLayout(
		220.0f,
		40.0f,
		25.0f,
		2.5f,
		8.0f,
		SCOREBOARD_TEAM_MODE_ICON_SIZE,
		true);

	EXPECT_FLOAT_EQ(SCOREBOARD_TEAM_MODE_ICON_SIZE, 12.0f);
	EXPECT_FLOAT_EQ(Layout.m_RowSpacing, 12.0f);
	EXPECT_FLOAT_EQ(Layout.m_Y, 67.0f);
	EXPECT_FLOAT_EQ(Layout.m_IconY, 65.0f);
}

TEST(QmScoreboardRender, DdTeamLabelSpacingFitsDenseColumnsWithoutOverlap)
{
	constexpr float AvailableRowsHeight = 333.0f;
	constexpr int RowsPerColumn = 12;
	constexpr float PreferredLineHeight = 25.0f;
	constexpr float PreferredSpacing = 2.5f;
	constexpr float PreferredTeamFontSize = 8.0f;
	const float ScaleWithoutTeams = ScoreboardRowsVerticalScale(AvailableRowsHeight, RowsPerColumn, 0, 0, PreferredLineHeight, PreferredSpacing, PreferredTeamFontSize, SCOREBOARD_TEAM_MODE_ICON_SIZE);
	const float Scale = ScoreboardRowsVerticalScale(AvailableRowsHeight, RowsPerColumn, RowsPerColumn, RowsPerColumn, PreferredLineHeight, PreferredSpacing, PreferredTeamFontSize, SCOREBOARD_TEAM_MODE_ICON_SIZE);
	const SScoreboardTeamLabelLayout TeamEnd = ResolveScoreboardTeamLabelLayout(
		0.0f,
		0.0f,
		PreferredLineHeight * Scale,
		PreferredSpacing * Scale,
		PreferredTeamFontSize * Scale,
		SCOREBOARD_TEAM_MODE_ICON_SIZE,
		true);

	EXPECT_FLOAT_EQ(ScaleWithoutTeams, 1.0f);
	EXPECT_LT(Scale, 1.0f);
	EXPECT_FLOAT_EQ(TeamEnd.m_RowSpacing, SCOREBOARD_TEAM_MODE_ICON_SIZE);
	EXPECT_LE(RowsPerColumn * (PreferredLineHeight * Scale + TeamEnd.m_RowSpacing), AvailableRowsHeight + 0.001f);
}

TEST(QmInputOverlayFiles, PendingCheckDoesNotWaitOrPublishPartialTime)
{
	CSemaphore Started;
	CSemaphore Finish;
	CJobPool Pool;
	Pool.Init(1);
	auto pCheck = std::make_shared<CQmInputOverlayFileTimeJob>([&]() -> std::optional<time_t> {
		Started.Signal();
		Finish.Wait();
		return 123;
	});
	std::optional<time_t> Modified = 99;
	Pool.Add(pCheck);
	Started.Wait();
	EXPECT_FALSE(pCheck->TryGetResult(Modified));
	EXPECT_EQ(Modified, 99);
	Finish.Signal();
	Pool.Shutdown();
	EXPECT_TRUE(pCheck->TryGetResult(Modified));
	EXPECT_EQ(Modified, 123);
}

TEST(QmInputOverlayFiles, MissingFileIsACompletedResult)
{
	CJobPool Pool;
	Pool.Init(1);
	auto pCheck = std::make_shared<CQmInputOverlayFileTimeJob>([] { return std::optional<time_t>(); });
	Pool.Add(pCheck);
	Pool.Shutdown();
	std::optional<time_t> Modified = 99;
	EXPECT_TRUE(pCheck->TryGetResult(Modified));
	EXPECT_FALSE(Modified.has_value());
}

TEST(QmScoreboardSkin, CopiesSkinAndColorsOnlyToControlledRole)
{
	for(const int Dummy : {0, 1})
	{
		for(const int UseCustomColor : {0, 1})
		{
			auto pConfig = std::make_unique<CConfig>();
			pConfig->m_ClDummy = Dummy;
			str_copy(pConfig->m_ClPlayerSkin, "main");
			str_copy(pConfig->m_ClDummySkin, "dummy");
			pConfig->m_ClPlayerUseCustomColor = pConfig->m_ClDummyUseCustomColor = 1;
			pConfig->m_ClPlayerColorBody = pConfig->m_ClDummyColorBody = 11;
			pConfig->m_ClPlayerColorFeet = pConfig->m_ClDummyColorFeet = 22;
			str_copy(pConfig->m_PlayerName, "main name");
			str_copy(pConfig->m_ClDummyName, "dummy name");

			ASSERT_TRUE(QmCopyScoreboardSkin(*pConfig, false, "kitty", UseCustomColor, 12345, 67890));

			EXPECT_STREQ(pConfig->m_ClPlayerSkin, Dummy ? "main" : "kitty");
			EXPECT_STREQ(pConfig->m_ClDummySkin, Dummy ? "kitty" : "dummy");
			EXPECT_EQ(pConfig->m_ClPlayerUseCustomColor, Dummy ? 1 : UseCustomColor);
			EXPECT_EQ(pConfig->m_ClDummyUseCustomColor, Dummy ? UseCustomColor : 1);
			EXPECT_EQ(pConfig->m_ClPlayerColorBody, Dummy ? 11u : 12345u);
			EXPECT_EQ(pConfig->m_ClDummyColorBody, Dummy ? 12345u : 11u);
			EXPECT_EQ(pConfig->m_ClPlayerColorFeet, Dummy ? 22u : 67890u);
			EXPECT_EQ(pConfig->m_ClDummyColorFeet, Dummy ? 67890u : 22u);
			EXPECT_STREQ(pConfig->m_PlayerName, "main name");
			EXPECT_STREQ(pConfig->m_ClDummyName, "dummy name");
		}
	}
}

TEST(QmScoreboardSkin, SixupDoesNotChangeEitherRole)
{
	for(const int Dummy : {0, 1})
	{
		auto pConfig = std::make_unique<CConfig>();
		pConfig->m_ClDummy = Dummy;
		str_copy(pConfig->m_ClPlayerSkin, "main");
		str_copy(pConfig->m_ClDummySkin, "dummy");
		pConfig->m_ClPlayerUseCustomColor = 0;
		pConfig->m_ClDummyUseCustomColor = 1;
		pConfig->m_ClPlayerColorBody = 11;
		pConfig->m_ClDummyColorBody = 22;
		pConfig->m_ClPlayerColorFeet = 33;
		pConfig->m_ClDummyColorFeet = 44;

		EXPECT_FALSE(QmCopyScoreboardSkin(*pConfig, true, "kitty", 1, 12345, 67890));

		EXPECT_STREQ(pConfig->m_ClPlayerSkin, "main");
		EXPECT_STREQ(pConfig->m_ClDummySkin, "dummy");
		EXPECT_EQ(pConfig->m_ClPlayerUseCustomColor, 0);
		EXPECT_EQ(pConfig->m_ClDummyUseCustomColor, 1);
		EXPECT_EQ(pConfig->m_ClPlayerColorBody, 11u);
		EXPECT_EQ(pConfig->m_ClDummyColorBody, 22u);
		EXPECT_EQ(pConfig->m_ClPlayerColorFeet, 33u);
		EXPECT_EQ(pConfig->m_ClDummyColorFeet, 44u);
	}
}

// 意图：双击的目标角色与「分身」判定的映射保持 本体=0 / 分身=1。
TEST(QmTeeSkinApply, TargetRoleMappingKeepsMainAndDummyDistinct)
{
	EXPECT_EQ(QmTeeSkinApplyTargetDummy(ETeeSkinApplyTarget::MAIN), 0);
	EXPECT_EQ(QmTeeSkinApplyTargetDummy(ETeeSkinApplyTarget::DUMMY), 1);
}

TEST(QmTeeSkinApply, WritesOnlyTheRequestedRoleAndKeepsTheOtherSideUntouched)
{
	for(const bool TargetDummy : {false, true})
	{
		const ETeeSkinApplyTarget Target = TargetDummy ? ETeeSkinApplyTarget::DUMMY : ETeeSkinApplyTarget::MAIN;
		auto pConfig = std::make_unique<CConfig>();
		pConfig->m_ClDummy = TargetDummy ? 0 : 1; // 当前子标签刻意与双击目标相反
		str_copy(pConfig->m_ClPlayerSkin, "main");
		str_copy(pConfig->m_ClDummySkin, "dummy");
		pConfig->m_ClPlayerUseCustomColor = 1;
		pConfig->m_ClDummyUseCustomColor = 1;
		pConfig->m_ClPlayerColorBody = 11;
		pConfig->m_ClDummyColorBody = 22;
		pConfig->m_ClPlayerColorFeet = 33;
		pConfig->m_ClDummyColorFeet = 44;

		QmApplyTeeSkinToTarget(*pConfig, Target, "kitty", true, true, 12345, 67890);

		EXPECT_STREQ(pConfig->m_ClPlayerSkin, TargetDummy ? "main" : "kitty");
		EXPECT_STREQ(pConfig->m_ClDummySkin, TargetDummy ? "kitty" : "dummy");
		EXPECT_EQ(pConfig->m_ClPlayerColorBody, TargetDummy ? 11u : 12345u);
		EXPECT_EQ(pConfig->m_ClDummyColorBody, TargetDummy ? 12345u : 22u);
		EXPECT_EQ(pConfig->m_ClPlayerColorFeet, TargetDummy ? 33u : 67890u);
		EXPECT_EQ(pConfig->m_ClDummyColorFeet, TargetDummy ? 67890u : 44u);
		EXPECT_EQ(pConfig->m_ClPlayerUseCustomColor, 1);
		EXPECT_EQ(pConfig->m_ClDummyUseCustomColor, 1);
	}
}

TEST(QmTeeSkinApply, EntryWithoutColorKeyKeepsExistingColorsAndTogglesOff)
{
	auto pConfig = std::make_unique<CConfig>();
	str_copy(pConfig->m_ClPlayerSkin, "main");
	str_copy(pConfig->m_ClDummySkin, "dummy");
	pConfig->m_ClPlayerUseCustomColor = 1;
	pConfig->m_ClDummyUseCustomColor = 1;
	pConfig->m_ClPlayerColorBody = 11;
	pConfig->m_ClDummyColorBody = 22;
	pConfig->m_ClPlayerColorFeet = 33;
	pConfig->m_ClDummyColorFeet = 44;

	// 条目没有颜色键：只换皮肤名，颜色与开关全部保持原样。
	QmApplyTeeSkinToTarget(*pConfig, ETeeSkinApplyTarget::MAIN, "kitty", false, false, 0, 0);
	EXPECT_STREQ(pConfig->m_ClPlayerSkin, "kitty");
	EXPECT_EQ(pConfig->m_ClPlayerUseCustomColor, 1);
	EXPECT_EQ(pConfig->m_ClPlayerColorBody, 11u);
	EXPECT_EQ(pConfig->m_ClPlayerColorFeet, 33u);

	// 颜色键显式关闭自定义颜色：关开关但不写入颜色值。
	QmApplyTeeSkinToTarget(*pConfig, ETeeSkinApplyTarget::DUMMY, "santa", true, false, 12345, 67890);
	EXPECT_STREQ(pConfig->m_ClDummySkin, "santa");
	EXPECT_EQ(pConfig->m_ClDummyUseCustomColor, 0);
	EXPECT_EQ(pConfig->m_ClDummyColorBody, 22u);
	EXPECT_EQ(pConfig->m_ClDummyColorFeet, 44u);
}

TEST(QmScoreboardFooter, EmptyFooterDoesNotReservePanels)
{
	const auto Layout = QmScoreboardFooterLayout({20.0f, 465.0f, 850.0f, 100.0f}, false, 0);
	EXPECT_FLOAT_EQ(Layout.m_Media.h, 0.0f);
	EXPECT_FLOAT_EQ(Layout.m_Spectators.h, 0.0f);
}

TEST(QmScoreboardFooter, MediaUsesOneFullWidthBar)
{
	const auto Layout = QmScoreboardFooterLayout({20.0f, 465.0f, 850.0f, 100.0f}, true, 0);
	EXPECT_FLOAT_EQ(Layout.m_Media.x, 20.0f);
	EXPECT_FLOAT_EQ(Layout.m_Media.y, 465.0f);
	EXPECT_FLOAT_EQ(Layout.m_Media.w, 850.0f);
	EXPECT_FLOAT_EQ(Layout.m_Media.h, 25.0f);
	EXPECT_FLOAT_EQ(Layout.m_Spectators.h, 0.0f);
}

TEST(QmScoreboardFooter, SpectatorsFollowMediaWithinAvailableHeight)
{
	for(const float Width : {450.0f, 850.0f})
	{
		for(const float Height : {70.0f, 100.0f})
		{
			const auto Layout = QmScoreboardFooterLayout({20.0f, 465.0f, Width, Height}, true, 128);
			EXPECT_FLOAT_EQ(Layout.m_Spectators.x, Layout.m_Media.x);
			EXPECT_FLOAT_EQ(Layout.m_Spectators.w, Width);
			EXPECT_FLOAT_EQ(Layout.m_Spectators.y, Layout.m_Media.y + Layout.m_Media.h + 5.0f);
			EXPECT_FLOAT_EQ(Layout.m_Spectators.y + Layout.m_Spectators.h, 465.0f + Height);
		}
	}
}

TEST(QmScoreboardFooter, SpectatorsStartImmediatelyWhenMediaIsHidden)
{
	const auto Layout = QmScoreboardFooterLayout({20.0f, 465.0f, 450.0f, 70.0f}, false, 1);
	EXPECT_FLOAT_EQ(Layout.m_Media.h, 0.0f);
	EXPECT_FLOAT_EQ(Layout.m_Spectators.y, 465.0f);
	EXPECT_FLOAT_EQ(Layout.m_Spectators.w, 450.0f);
	EXPECT_FLOAT_EQ(Layout.m_Spectators.h, 70.0f);
}

TEST(QmScoreHudLayout, ShortRankKeepsOriginalFootprint)
{
	const auto Layout = QmScoreHudLayout(300.0f, 14.0f, 7.0f, 18.0f);
	EXPECT_FLOAT_EQ(Layout.m_BoxLeft, 248.0f);
	EXPECT_FLOAT_EQ(Layout.m_BoxWidth, 52.0f);
	EXPECT_FLOAT_EQ(Layout.m_RankX, 251.0f);
	EXPECT_FLOAT_EQ(Layout.m_TeeX, 274.0f);
}

TEST(QmScoreHudLayout, MeasuredRanksLeaveSpaceBeforeTee)
{
	// 宽度由渲染器测量，包含名次末尾的句点，覆盖短名次到三位数名次。
	for(const float RankTextWidth : {7.0f, 12.0f, 14.0f, 19.0f, 21.0f, 24.0f})
	{
		for(const float ScoreWidth : {14.0f, 70.0f})
		{
			const auto Layout = QmScoreHudLayout(300.0f, ScoreWidth, RankTextWidth, 18.0f);
			const float RankRight = Layout.m_RankX + RankTextWidth;
			const float TeeLeft = Layout.m_TeeX - 9.0f;
			EXPECT_GE(TeeLeft - RankRight, 3.0f);
			EXPECT_FLOAT_EQ(Layout.m_RankX - Layout.m_BoxLeft, 3.0f);
			EXPECT_FLOAT_EQ(Layout.m_BoxLeft + Layout.m_BoxWidth, 300.0f);
		}
	}
}

TEST(QmScoreHudLayout, WiderRankExpandsOnlyToTheLeft)
{
	const auto TwoDigits = QmScoreHudLayout(300.0f, 14.0f, 14.0f, 18.0f);
	const auto ThreeDigits = QmScoreHudLayout(300.0f, 14.0f, 21.0f, 18.0f);
	EXPECT_FLOAT_EQ(ThreeDigits.m_TeeX, TwoDigits.m_TeeX);
	EXPECT_FLOAT_EQ(ThreeDigits.m_BoxLeft, TwoDigits.m_BoxLeft - 7.0f);
	EXPECT_FLOAT_EQ(ThreeDigits.m_BoxWidth, TwoDigits.m_BoxWidth + 7.0f);
	EXPECT_FLOAT_EQ(ThreeDigits.m_RankX, TwoDigits.m_RankX - 7.0f);
}
