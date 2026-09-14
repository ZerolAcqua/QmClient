// 请抬头享受阳光｜日子很好 我很我---------致咩子
#define CONF_TEST 1
#include "test.h"

#include <base/color.h>

#include <game/client/components/qmclient/qm_title_render.h>
#include <game/client/components/qmclient/qm_title_style.h>

#include <gtest/gtest.h>
#include <test/test.h>

#include <cmath>
#include <set>
#include <string>

namespace
{
	// 读取仓库源码用于回归断言。testrunner 以构建目录为工作目录运行，
	// 必须走 DDNET_TEST_SOURCE_DIR 解析出的绝对路径，不能依赖当前工作目录。
	std::string ReadQmTitleStyleSource(const char *pPath)
	{
		return ReadTestSourceFile(pPath);
	}

	// 采样结果与期望色在 1/255 内一致：颜色最终会量化到 8 位顶点色通道。
	void ExpectColorNear(const ColorRGBA &Actual, const unsigned int ExpectedHex, const float Tolerance = 1.0f / 255.0f)
	{
		const ColorRGBA Expected = QmTitleStyleColorFromHex(ExpectedHex);
		EXPECT_NEAR(Actual.r, Expected.r, Tolerance);
		EXPECT_NEAR(Actual.g, Expected.g, Tolerance);
		EXPECT_NEAR(Actual.b, Expected.b, Tolerance);
		EXPECT_NEAR(Actual.a, Expected.a, Tolerance);
	}

	const SQmTitleStyle &Style(const char *pId)
	{
		const SQmTitleStyle *pStyle = QmTitleStyleById(pId);
		EXPECT_NE(pStyle, nullptr) << pId;
		// 断言失败时回退到首个风格，避免解引用空指针导致整个用例崩溃。
		return pStyle != nullptr ? *pStyle : *QmTitleStyleByIndex(0);
	}
}

// ---------------------------------------------------------------------------
// P1：引擎顶点分色的回归断言
// ---------------------------------------------------------------------------

// 字符内横向渐变依赖左右顶点分色：左边缘顶点用色段起始色，右边缘顶点用色段结束色。
// 顶点 0/3 是左边缘，顶点 1/2 是右边缘（见 text.cpp 中 CharX / CharX + CharWidth 的取值）。
TEST(QmTitleStyle, TextRenderSplitsLeftAndRightVertexColor)
{
	const std::string Source = ReadQmTitleStyleSource("src/engine/client/text.cpp");
	ASSERT_FALSE(Source.empty()) << "无法读取 src/engine/client/text.cpp";

	for(const int Vertex : {0, 3})
	{
		for(const char Channel : {'r', 'g', 'b', 'a'})
		{
			const std::string Expected = "TextCharQuad.m_aVertices[" + std::to_string(Vertex) + "].m_Color." + Channel +
						     " = (unsigned char)(Color." + Channel + " * 255.f);";
			EXPECT_NE(Source.find(Expected), std::string::npos) << Expected;
		}
	}
	for(const int Vertex : {1, 2})
	{
		for(const char Channel : {'r', 'g', 'b', 'a'})
		{
			const std::string Expected = "TextCharQuad.m_aVertices[" + std::to_string(Vertex) + "].m_Color." + Channel +
						     " = (unsigned char)(ColorEnd." + Channel + " * 255.f);";
			EXPECT_NE(Source.find(Expected), std::string::npos) << Expected;
		}
	}
}

// 右边缘色默认与左边缘色同源，未使用渐变的调用方写入的顶点字节与旧实现完全一致。
TEST(QmTitleStyle, TextRenderDefaultsEndColorToBaseColor)
{
	const std::string Source = ReadQmTitleStyleSource("src/engine/client/text.cpp");
	ASSERT_FALSE(Source.empty()) << "无法读取 src/engine/client/text.cpp";
	EXPECT_NE(Source.find("ColorRGBA ColorEnd = m_Color;"), std::string::npos);
}

// 色段赋值必须成对更新 Color 与 ColorEnd：若 ColorEnd 落在条件之外，
// 尚未生效的色段会污染右边缘色，未启用渐变时也会出现意外的字符内渐变。
TEST(QmTitleStyle, TextRenderAssignsEndColorTogetherWithBaseColor)
{
	const std::string Source = ReadQmTitleStyleSource("src/engine/client/text.cpp");
	ASSERT_FALSE(Source.empty()) << "无法读取 src/engine/client/text.cpp";

	const std::string Indent6(6, '\t');
	const std::string Indent7(7, '\t');
	const std::string Indent8(8, '\t');
	const std::string Indent9(9, '\t');

	// 当前字符落在色段内
	const std::string CurrentSpan =
		Indent6 + "if(PrevCharCount >= Split.m_CharIndex && (Split.m_Length == -1 || PrevCharCount < Split.m_CharIndex + Split.m_Length))\n" +
		Indent6 + "{\n" +
		Indent7 + "Color = Split.m_Color;\n" +
		Indent7 + "ColorEnd = Split.m_ColorEnd;\n" +
		Indent6 + "}";
	EXPECT_NE(Source.find(CurrentSpan), std::string::npos) << "色段内取色未成对更新 Color 与 ColorEnd";

	// 推进到下一个色段
	const std::string NextSpan =
		Indent8 + "if(PrevCharCount >= Split.m_CharIndex)\n" +
		Indent8 + "{\n" +
		Indent9 + "Color = Split.m_Color;\n" +
		Indent9 + "ColorEnd = Split.m_ColorEnd;\n" +
		Indent8 + "}";
	EXPECT_NE(Source.find(NextSpan), std::string::npos) << "推进色段时未成对更新 Color 与 ColorEnd";
}

// ---------------------------------------------------------------------------
// P2：风格表与采样器
// ---------------------------------------------------------------------------

TEST(QmTitleStyle, ColorFromHexIsExact)
{
	const ColorRGBA Color = QmTitleStyleColorFromHex(0xBF2D47);
	EXPECT_FLOAT_EQ(Color.r, 191.0f / 255.0f);
	EXPECT_FLOAT_EQ(Color.g, 45.0f / 255.0f);
	EXPECT_FLOAT_EQ(Color.b, 71.0f / 255.0f);
	EXPECT_FLOAT_EQ(Color.a, 1.0f);
}

// 风格表是唯一真源，必须自洽：id 唯一、色标合法、动效模式周期为正。
TEST(QmTitleStyle, StyleTableIsWellFormed)
{
	EXPECT_EQ(QmTitleStyleCount(), 31);

	std::set<std::string> Ids;
	for(int Index = 0; Index < QmTitleStyleCount(); ++Index)
	{
		const SQmTitleStyle *pStyle = QmTitleStyleByIndex(Index);
		ASSERT_NE(pStyle, nullptr) << Index;
		EXPECT_NE(pStyle->m_pId, nullptr) << Index;
		EXPECT_NE(pStyle->m_pLabel, nullptr) << Index;
		EXPECT_STRNE(pStyle->m_pId, "") << Index;
		EXPECT_TRUE(Ids.insert(pStyle->m_pId).second) << "重复 id: " << pStyle->m_pId;
		EXPECT_NE(pStyle->m_pColors, nullptr) << pStyle->m_pId;
		EXPECT_GE(pStyle->m_ColorCount, 1) << pStyle->m_pId;

		if(pStyle->m_Mode == EQmTitleStyleMode::Static)
		{
			EXPECT_EQ(pStyle->m_ColorCount, 1) << pStyle->m_pId;
		}
		else
		{
			EXPECT_GT(pStyle->m_PeriodSec, 0.0f) << pStyle->m_pId;
			EXPECT_GE(pStyle->m_ColorCount, 2) << pStyle->m_pId;
		}
	}

	EXPECT_EQ(QmTitleStyleByIndex(-1), nullptr);
	EXPECT_EQ(QmTitleStyleByIndex(QmTitleStyleCount()), nullptr);
	EXPECT_EQ(QmTitleStyleById(""), nullptr);
	EXPECT_EQ(QmTitleStyleById("does_not_exist"), nullptr);
	EXPECT_EQ(QmTitleStyleById(nullptr), nullptr);
}

// 静态风格的采样与时间、位置无关。
TEST(QmTitleStyle, StaticStyleIsConstant)
{
	const SQmTitleStyle &Turquoise = Style("turquoise");
	ExpectColorNear(QmTitleStyleSample(Turquoise, 0.0f, 0.0f), 0x00FFC8);
	ExpectColorNear(QmTitleStyleSample(Turquoise, 123.5f, 400.0f), 0x00FFC8);

	ExpectColorNear(QmTitleStyleSample(Style("donator_item"), 7.0f, 0.0f), 0xFF799C);
	ExpectColorNear(QmTitleStyleSample(Style("hot_pink"), 7.0f, 0.0f), 0xFF00FF);
}

// CalamityUtils.ColorSwap：周期 seconds 的正弦往返，t = seconds/4 与 t = 3*seconds/4 分别抵达两端。
TEST(QmTitleStyle, SinSwapReachesBothEnds)
{
	const SQmTitleStyle &ScarletDevil = Style("scarlet_devil");
	EXPECT_FLOAT_EQ(ScarletDevil.m_PeriodSec, 4.0f);
	ExpectColorNear(QmTitleStyleSample(ScarletDevil, 1.0f, 0.0f), 0xB9BBFD);
	ExpectColorNear(QmTitleStyleSample(ScarletDevil, 3.0f, 0.0f), 0xBF2D47);
	// t = 2 落在正弦零点，是两端色的中点
	const ColorRGBA Mid = QmTitleStyleSample(ScarletDevil, 2.0f, 0.0f);
	const ColorRGBA DevFrom = QmTitleStyleColorFromHex(0xBF2D47);
	const ColorRGBA DevTo = QmTitleStyleColorFromHex(0xB9BBFD);
	EXPECT_NEAR(Mid.r, (DevFrom.r + DevTo.r) * 0.5f, 1.0f / 255.0f);
	EXPECT_NEAR(Mid.g, (DevFrom.g + DevTo.g) * 0.5f, 1.0f / 255.0f);
	EXPECT_NEAR(Mid.b, (DevFrom.b + DevTo.b) * 0.5f, 1.0f / 255.0f);

	// Shattered Community 的周期在源码里是 3 秒
	const SQmTitleStyle &Shattered = Style("shattered_community");
	EXPECT_FLOAT_EQ(Shattered.m_PeriodSec, 3.0f);
	ExpectColorNear(QmTitleStyleSample(Shattered, 0.75f, 0.0f), 0xF569F5);
	ExpectColorNear(QmTitleStyleSample(Shattered, 2.25f, 0.0f), 0x803E80);

	// Ozzathoth 复用 Shattered Community 的色定义
	const SQmTitleStyle &Ozzathoth = Style("ozzathoth");
	ExpectColorNear(QmTitleStyleSample(Ozzathoth, 0.75f, 0.0f), 0xF569F5);
}

// CalamityUtils.MulticolorLerp：m_PeriodSec 秒走完整个色环。
TEST(QmTitleStyle, MulticolorLerpWalksColorRing)
{
	const SQmTitleStyle &Angelic = Style("angelic_alliance");
	EXPECT_FLOAT_EQ(Angelic.m_PeriodSec, 2.0f);
	ExpectColorNear(QmTitleStyleSample(Angelic, 0.0f, 0.0f), 0xFFC437);

	// increment = 0.5 → 落在第 2、3 个色标之间的一半
	const ColorRGBA Mid = QmTitleStyleSample(Angelic, 1.0f, 0.0f);
	const ColorRGBA From = QmTitleStyleColorFromHex(0xFFE76B);
	const ColorRGBA To = QmTitleStyleColorFromHex(0xFFFEF3);
	EXPECT_NEAR(Mid.r, (From.r + To.r) * 0.5f, 1.0f / 255.0f);
	EXPECT_NEAR(Mid.g, (From.g + To.g) * 0.5f, 1.0f / 255.0f);
	EXPECT_NEAR(Mid.b, (From.b + To.b) * 0.5f, 1.0f / 255.0f);

	// 走满一圈回到起点附近
	ExpectColorNear(QmTitleStyleSample(Angelic, 2.0f, 0.0f), 0xFFC437, 0.02f);
}

// ExoticRainbow 模板：每 2 秒推进一个色标，第 1 秒渐变、第 2 秒保持。
TEST(QmTitleStyle, PhaseCycleMatchesExoticRainbowTemplate)
{
	const SQmTitleStyle &Rainbow = Style("exotic_rainbow");
	EXPECT_EQ(Rainbow.m_ColorCount, 3);
	EXPECT_FLOAT_EQ(Rainbow.m_PeriodSec, 2.0f);
	// 默认档位取源码原值：ExoticRainbow 的插值因子被量化，属于硬切换。
	EXPECT_EQ(QmTitleStyleSourceInterpolation(Rainbow), EQmTitleInterpolation::Hard);

	ExpectColorNear(QmTitleStyleSample(Rainbow, 0.0f, 0.0f), 0xFF6B6B);
	ExpectColorNear(QmTitleStyleSample(Rainbow, 1.5f, 0.0f), 0x7DC4E1);
	ExpectColorNear(QmTitleStyleSample(Rainbow, 2.0f, 0.0f), 0x7DC4E1);
	ExpectColorNear(QmTitleStyleSample(Rainbow, 3.5f, 0.0f), 0xD3EB6C);
	ExpectColorNear(QmTitleStyleSample(Rainbow, 6.0f, 0.0f), 0xFF6B6B);

	// 平滑档位下，t = 0.5 恰好在第一、二色标之间
	const ColorRGBA Mid = QmTitleStyleSampleWithInterpolation(Rainbow, 0.5f, 0.0f, EQmTitleInterpolation::Smooth);
	const ColorRGBA From = QmTitleStyleColorFromHex(0xFF6B6B);
	const ColorRGBA To = QmTitleStyleColorFromHex(0x7DC4E1);
	EXPECT_NEAR(Mid.r, (From.r + To.r) * 0.5f, 1.0f / 255.0f);
	EXPECT_NEAR(Mid.g, (From.g + To.g) * 0.5f, 1.0f / 255.0f);
	EXPECT_NEAR(Mid.b, (From.b + To.b) * 0.5f, 1.0f / 255.0f);
}

// 逐字符空间相位：相位量是字符左边缘的累计像素偏移，系数 0.005/px。
TEST(QmTitleStyle, PhaseCycleAddsPerPixelPhase)
{
	const SQmTitleStyle &Rainbow = Style("exotic_rainbow");
	EXPECT_FLOAT_EQ(Rainbow.m_PhasePerPx, 0.005f);

	// x = 0 时是纯起始色
	ExpectColorNear(QmTitleStyleSampleWithInterpolation(Rainbow, 0.0f, 0.0f, EQmTitleInterpolation::Smooth), 0xFF6B6B);
	// x = 100 时 rate = 0.5，正好走到第一、二色标之间
	const ColorRGBA Shifted = QmTitleStyleSampleWithInterpolation(Rainbow, 0.0f, 100.0f, EQmTitleInterpolation::Smooth);
	const ColorRGBA From = QmTitleStyleColorFromHex(0xFF6B6B);
	const ColorRGBA To = QmTitleStyleColorFromHex(0x7DC4E1);
	EXPECT_NEAR(Shifted.r, (From.r + To.r) * 0.5f, 1.0f / 255.0f);
	EXPECT_NEAR(Shifted.g, (From.g + To.g) * 0.5f, 1.0f / 255.0f);
	EXPECT_NEAR(Shifted.b, (From.b + To.b) * 0.5f, 1.0f / 255.0f);

	// 整行同步的风格不受像素位置影响
	const SQmTitleStyle &Eternity = Style("eternity");
	EXPECT_FLOAT_EQ(Eternity.m_PhasePerPx, 0.0f);
	const ColorRGBA Near = QmTitleStyleSample(Eternity, 0.4f, 0.0f);
	const ColorRGBA Far = QmTitleStyleSample(Eternity, 0.4f, 500.0f);
	EXPECT_FLOAT_EQ(Near.r, Far.r);
	EXPECT_FLOAT_EQ(Near.g, Far.g);
	EXPECT_FLOAT_EQ(Near.b, Far.b);
}

// 硬切换把插值因子量化到 0/1；平滑档位保留连续过渡。
TEST(QmTitleStyle, PhaseCycleHardSwitchQuantizes)
{
	const SQmTitleStyle &Rainbow = Style("exotic_rainbow");
	// t = 0.3 时平滑档位应偏离起始色，硬切换仍停在起始色
	const ColorRGBA Smooth = QmTitleStyleSampleWithInterpolation(Rainbow, 0.3f, 0.0f, EQmTitleInterpolation::Smooth);
	ExpectColorNear(QmTitleStyleSampleWithInterpolation(Rainbow, 0.3f, 0.0f, EQmTitleInterpolation::Hard), 0xFF6B6B);
	EXPECT_GT(Smooth.g, QmTitleStyleColorFromHex(0xFF6B6B).g);

	// t = 0.7 越过一半，硬切换到下一个色标
	ExpectColorNear(QmTitleStyleSampleWithInterpolation(Rainbow, 0.7f, 0.0f, EQmTitleInterpolation::Hard), 0x7DC4E1);
}

// FlamsteedRing：1 秒周期内的 0.6 / 0.2 / 0.2 分段时序。
TEST(QmTitleStyle, PiecewiseMatchesFlamsteedRing)
{
	const SQmTitleStyle &Flamsteed = Style("flamsteed_ring");
	EXPECT_FLOAT_EQ(Flamsteed.m_PeriodSec, 1.0f);

	ExpectColorNear(QmTitleStyleSample(Flamsteed, 0.3f, 0.0f), 0x59E5FF);
	ExpectColorNear(QmTitleStyleSample(Flamsteed, 0.6f, 0.0f), 0x59E5FF);

	// 0.7 秒处在第一段渐变的中间
	const ColorRGBA Fading = QmTitleStyleSample(Flamsteed, 0.7f, 0.0f);
	const ColorRGBA Cyan = QmTitleStyleColorFromHex(0x59E5FF);
	EXPECT_NEAR(Fading.r, (Cyan.r + 1.0f) * 0.5f, 1.0f / 255.0f);
	EXPECT_NEAR(Fading.g, (Cyan.g + 1.0f) * 0.5f, 1.0f / 255.0f);
	EXPECT_NEAR(Fading.b, (Cyan.b + 1.0f) * 0.5f, 1.0f / 255.0f);

	// 0.9 秒处在回程的中间，与去程对称
	const ColorRGBA Returning = QmTitleStyleSample(Flamsteed, 0.9f, 0.0f);
	EXPECT_NEAR(Returning.r, Fading.r, 1.0f / 255.0f);
	EXPECT_NEAR(Returning.g, Fading.g, 1.0f / 255.0f);
	EXPECT_NEAR(Returning.b, Fading.b, 1.0f / 255.0f);

	ExpectColorNear(QmTitleStyleSample(Flamsteed, 1.0f, 0.0f), 0x59E5FF);
}

// 时间与相位对负数输入也必须落在合法色标范围内，不得越界。
TEST(QmTitleStyle, NegativeTimeStaysInRange)
{
	for(int Index = 0; Index < QmTitleStyleCount(); ++Index)
	{
		const SQmTitleStyle *pStyle = QmTitleStyleByIndex(Index);
		ASSERT_NE(pStyle, nullptr);
		const ColorRGBA Color = QmTitleStyleSample(*pStyle, -3.5f, -120.0f);
		EXPECT_GE(Color.r, 0.0f) << pStyle->m_pId;
		EXPECT_LE(Color.r, 1.0f) << pStyle->m_pId;
		EXPECT_GE(Color.g, 0.0f) << pStyle->m_pId;
		EXPECT_LE(Color.g, 1.0f) << pStyle->m_pId;
		EXPECT_GE(Color.b, 0.0f) << pStyle->m_pId;
		EXPECT_LE(Color.b, 1.0f) << pStyle->m_pId;
		EXPECT_FLOAT_EQ(Color.a, 1.0f) << pStyle->m_pId;
	}
}

// ---------------------------------------------------------------------------
// 逐字符波浪浮动
// ---------------------------------------------------------------------------

// 偏移游标必须对每个字符消费一次，包括不产生顶点的字符，否则后续字符会整体错位。
TEST(QmTitleStyle, TextRenderConsumesCharOffsetForEveryChar)
{
	const std::string Source = ReadQmTitleStyleSource("src/engine/client/text.cpp");
	ASSERT_FALSE(Source.empty()) << "无法读取 src/engine/client/text.cpp";

	EXPECT_NE(Source.find("int OffsetOption = 0;"), std::string::npos);

	// 必须跳过序号落后的条目：调用方若为换行等不产生顶点的字符也建了条目，
	// 游标会永久卡住，之后所有字符的偏移恒为 0。
	EXPECT_NE(Source.find("pCursor->m_vCharOffsets.at(OffsetOption).m_CharIndex < PrevCharCount"), std::string::npos)
		<< "缺少跳过落后条目的逻辑";

	const std::size_t OffsetLookup = Source.find("pCursor->m_vCharOffsets.at(OffsetOption).m_CharIndex == PrevCharCount");
	const std::size_t VertexWrite = Source.find("TextContainer.m_StringInfo.m_vCharacterQuads.emplace_back();");
	ASSERT_NE(OffsetLookup, std::string::npos) << "未找到逐字符偏移查询";
	ASSERT_NE(VertexWrite, std::string::npos);
	EXPECT_LT(OffsetLookup, VertexWrite) << "偏移查询必须在顶点写入之前，否则不渲染的字符会漏掉游标推进";

	// 四个顶点的 X 与 Y 都必须带上偏移，否则浮动会把字符拉变形
	for(const int Vertex : {0, 1, 2, 3})
	{
		for(const char Axis : {'X', 'Y'})
		{
			const std::string Prefix = "TextCharQuad.m_aVertices[" + std::to_string(Vertex) + "].m_" + Axis + " = ";
			const std::size_t Start = Source.find(Prefix);
			ASSERT_NE(Start, std::string::npos) << Prefix;
			const std::size_t LineEnd = Source.find(';', Start);
			ASSERT_NE(LineEnd, std::string::npos) << Prefix;
			const std::string Line = Source.substr(Start, LineEnd - Start);
			EXPECT_NE(Line.find("CharOffset"), std::string::npos) << Line;
		}
	}
}

// 关闭时（幅度为 0 或波长非法）偏移必须恒为 0，保证默认档位与 Calamity 的静止文字一致。
TEST(QmTitleStyle, BobOffsetDisabled)
{
	const SQmTitleBobStyle Off{0.0f, 64.0f, 0.4f};
	for(int Step = 0; Step < 32; ++Step)
		EXPECT_FLOAT_EQ(QmTitleStyleBobOffset(Off, (float)Step * 0.31f, (float)Step * 5.0f), 0.0f);

	const SQmTitleBobStyle NoWaveLength{2.0f, 0.0f, 0.4f};
	EXPECT_FLOAT_EQ(QmTitleStyleBobOffset(NoWaveLength, 1.0f, 40.0f), 0.0f);

	const SQmTitleBobStyle NegativeWaveLength{2.0f, -8.0f, 0.4f};
	EXPECT_FLOAT_EQ(QmTitleStyleBobOffset(NegativeWaveLength, 1.0f, 40.0f), 0.0f);
}

// 相位沿像素 X 递进：波长四分之一处到峰值，四分之三处到谷值。
TEST(QmTitleStyle, BobOffsetVariesAlongPixelX)
{
	const SQmTitleBobStyle Bob{4.0f, 64.0f, 0.0f, 1.0f};
	EXPECT_FLOAT_EQ(QmTitleStyleBobOffset(Bob, 0.0f, 0.0f), 0.0f);
	EXPECT_FLOAT_EQ(QmTitleStyleBobOffset(Bob, 0.0f, 16.0f), 4.0f);
	EXPECT_FLOAT_EQ(QmTitleStyleBobOffset(Bob, 0.0f, 32.0f), 0.0f);
	EXPECT_FLOAT_EQ(QmTitleStyleBobOffset(Bob, 0.0f, 48.0f), -4.0f);

	// 相邻字符（间隔一个字符宽）的偏移不同，这才形成「波浪扫过」而不是整体升降
	EXPECT_NE(QmTitleStyleBobOffset(Bob, 0.0f, 8.0f), QmTitleStyleBobOffset(Bob, 0.0f, 16.0f));
}

// 时间推进让同一位置发生起伏。m_Speed 的单位是弧度/秒，2π 表示每秒推进一整圈。
TEST(QmTitleStyle, BobOffsetAdvancesWithTime)
{
	const SQmTitleBobStyle Moving{4.0f, 64.0f, 6.2831853f, 1.0f};
	EXPECT_NE(QmTitleStyleBobOffset(Moving, 0.0f, 16.0f), QmTitleStyleBobOffset(Moving, 0.1f, 16.0f));

	// 推进一整圈后回到原相位
	const float Start = QmTitleStyleBobOffset(Moving, 0.0f, 16.0f);
	const float Looped = QmTitleStyleBobOffset(Moving, 1.0f, 16.0f);
	EXPECT_NEAR(Start, Looped, 1.0f);
}

// Calamity Wavy 的默认参数（幅度 6、波长 320、freq 1.5）必须能直接照抄使用。
TEST(QmTitleStyle, BobOffsetMatchesCalamityWavyDefaults)
{
	const SQmTitleBobStyle Wavy{6.0f, 320.0f, 1.5f, 1.0f};
	// 时间到达 sin 峰值：1.5t = π/2
	EXPECT_FLOAT_EQ(QmTitleStyleBobOffset(Wavy, 1.0471976f, 0.0f), 6.0f);
	// 位置到达 sin 峰值：x = 四分之一波长
	EXPECT_FLOAT_EQ(QmTitleStyleBobOffset(Wavy, 0.0f, 80.0f), 6.0f);
	// 波谷
	EXPECT_FLOAT_EQ(QmTitleStyleBobOffset(Wavy, 0.0f, 240.0f), -6.0f);
	// 半波长处回到基线
	EXPECT_FLOAT_EQ(QmTitleStyleBobOffset(Wavy, 0.0f, 160.0f), 0.0f);
}

// 偏移必须有界；量化步长为 1 时吸附到整数像素。
TEST(QmTitleStyle, BobOffsetBoundedAndSnappedWhenRequested)
{
	const SQmTitleBobStyle Snapped{3.0f, 70.0f, 0.45f, 1.0f};
	for(int Step = 0; Step < 400; ++Step)
	{
		const float Offset = QmTitleStyleBobOffset(Snapped, (float)Step * 0.037f, (float)Step * 1.7f);
		EXPECT_LE(std::fabs(Offset), Snapped.m_Amplitude) << "偏移超出幅度上限";
		EXPECT_FLOAT_EQ(Offset, std::nearbyint(Offset)) << "步长为 1 时未吸附整数像素";
	}
}

// 默认步长为 0，必须输出亚像素偏移：否则小幅度下波形会长时间停在同一像素再跳变，
// 观感类似掉帧（这正是把默认值从整数像素改为亚像素的原因）。
TEST(QmTitleStyle, BobOffsetIsSubPixelByDefault)
{
	const SQmTitleBobStyle Smooth{3.0f, 70.0f, 0.45f, 0.0f};
	int FractionalSamples = 0;
	for(int Step = 0; Step < 200; ++Step)
	{
		const float Offset = QmTitleStyleBobOffset(Smooth, (float)Step * 0.037f, (float)Step * 1.7f);
		EXPECT_LE(std::fabs(Offset), Smooth.m_Amplitude) << "偏移超出幅度上限";
		if(Offset != std::nearbyint(Offset))
			++FractionalSamples;
	}
	EXPECT_GT(FractionalSamples, 100) << "亚像素模式应产生大量非整数偏移";
}

// 相位只沿像素 X 递进，与字符索引无关：这样中英文混排时波纹间距仍然均匀。
TEST(QmTitleStyle, BobOffsetUsesPixelXNotCharIndex)
{
	const SQmTitleBobStyle Wavy{6.0f, 320.0f, 1.5f, 1.0f};
	// 两个字符间隔越远，相位差越大
	const float Near = QmTitleStyleBobOffset(Wavy, 0.0f, 0.0f);
	const float Mid = QmTitleStyleBobOffset(Wavy, 0.0f, 80.0f);
	const float Far = QmTitleStyleBobOffset(Wavy, 0.0f, 160.0f);
	EXPECT_FLOAT_EQ(Near, 0.0f);
	EXPECT_FLOAT_EQ(Mid, 6.0f);
	EXPECT_FLOAT_EQ(Far, 0.0f);
}

// 相位系数覆盖：Calamity 只有 ExoticRainbow 系列自带非零系数，其余风格整行同步变色；
// 覆盖后所有模式都必须产生逐字符差异（这正是「光带扫过」的来源）。
TEST(QmTitleStyle, EffectivePhasePerPxOverride)
{
	const SQmTitleStyle &Rainbow = Style("exotic_rainbow");
	EXPECT_FLOAT_EQ(QmTitleStyleEffectivePhasePerPx(Rainbow, 0.0f), Rainbow.m_PhasePerPx);
	EXPECT_FLOAT_EQ(QmTitleStyleEffectivePhasePerPx(Rainbow, 0.02f), 0.02f);

	const SQmTitleStyle &Scarlet = Style("scarlet_devil");
	EXPECT_FLOAT_EQ(Scarlet.m_PhasePerPx, 0.0f);
	EXPECT_FLOAT_EQ(QmTitleStyleEffectivePhasePerPx(Scarlet, 0.0f), 0.0f);

	// 不带相位的正弦往返风格：覆盖后同一时刻相距 60px 的两点颜色必须明显不同
	SQmTitleStyle Shifted = Scarlet;
	Shifted.m_PhasePerPx = QmTitleStyleEffectivePhasePerPx(Scarlet, 0.02f);
	const ColorRGBA Near = QmTitleStyleSampleWithInterpolation(Shifted, 0.0f, 0.0f, EQmTitleInterpolation::Smooth);
	const ColorRGBA Far = QmTitleStyleSampleWithInterpolation(Shifted, 0.0f, 60.0f, EQmTitleInterpolation::Smooth);
	const float Difference = std::fabs(Near.r - Far.r) + std::fabs(Near.g - Far.g) + std::fabs(Near.b - Far.b);
	EXPECT_GT(Difference, 0.1f) << "覆盖相位后不同位置的颜色仍应明显不同";

	// 相位为 0 时必须与不覆盖完全一致（保持 Calamity 原版行为）
	SQmTitleStyle Unchanged = Scarlet;
	Unchanged.m_PhasePerPx = QmTitleStyleEffectivePhasePerPx(Scarlet, 0.0f);
	const ColorRGBA A = QmTitleStyleSampleWithInterpolation(Unchanged, 0.7f, 0.0f, EQmTitleInterpolation::Smooth);
	const ColorRGBA B = QmTitleStyleSampleWithInterpolation(Unchanged, 0.7f, 200.0f, EQmTitleInterpolation::Smooth);
	EXPECT_FLOAT_EQ(A.r, B.r);
	EXPECT_FLOAT_EQ(A.g, B.g);
	EXPECT_FLOAT_EQ(A.b, B.b);

	// 多色循环与分段模式同样必须响应相位。分段模式的色区不连续，
	// 因此扫描一段位置取最大差异，而不是只看单个采样点。
	for(const char *pId : {"angelic_alliance", "eternity", "flamsteed_ring"})
	{
		SQmTitleStyle Multi = Style(pId);
		Multi.m_PhasePerPx = 0.02f;
		const ColorRGBA X0 = QmTitleStyleSampleWithInterpolation(Multi, 0.0f, 0.0f, EQmTitleInterpolation::Smooth);
		float MaxDifference = 0.0f;
		for(int Step = 1; Step <= 40; ++Step)
		{
			const ColorRGBA Other = QmTitleStyleSampleWithInterpolation(Multi, 0.0f, (float)Step * 2.0f, EQmTitleInterpolation::Smooth);
			const float StepDifference = std::fabs(X0.r - Other.r) + std::fabs(X0.g - Other.g) + std::fabs(X0.b - Other.b);
			if(StepDifference > MaxDifference)
				MaxDifference = StepDifference;
		}
		EXPECT_GT(MaxDifference, 0.05f) << pId;
	}
}
TEST(QmTitleStyle, ZeroPeriodFallsBackToFirstColor)
{
	const unsigned int aColors[] = {0x112233, 0x445566};
	SQmTitleStyle Broken{};
	Broken.m_pId = "broken";
	Broken.m_pLabel = "Broken";
	Broken.m_Mode = EQmTitleStyleMode::SinSwap;
	Broken.m_Interpolation = EQmTitleInterpolation::Smooth;
	Broken.m_pColors = aColors;
	Broken.m_ColorCount = 2;
	Broken.m_PeriodSec = 0.0f;
	Broken.m_TimeScale = 1.0f;
	Broken.m_PhasePerPx = 0.0f;

	ExpectColorNear(QmTitleStyleSample(Broken, 1.0f, 0.0f), 0x112233);
	ExpectColorNear(QmTitleStyleSample(Broken, 99.0f, 500.0f), 0x112233);

	Broken.m_PeriodSec = -4.0f;
	ExpectColorNear(QmTitleStyleSample(Broken, 1.0f, 0.0f), 0x112233);
}

// P5：头衔抛光档。掠光只改顶点色，且必须在任何头衔长度下都恰好扫过一次、不随长度变慢。
TEST(QmTitleStyle, ShimmerDisabledPaths)
{
	SQmTitleShimmer Off;
	EXPECT_FLOAT_EQ(QmTitleShimmerFactor(Off, 0, 6, 0.0f), 0.0f);

	SQmTitleShimmer ZeroSpeed = Off;
	ZeroSpeed.m_Enabled = true;
	ZeroSpeed.m_Speed = 0.0f;
	EXPECT_FLOAT_EQ(QmTitleShimmerFactor(ZeroSpeed, 0, 6, 0.3f), 0.0f);

	SQmTitleShimmer ZeroAmount = Off;
	ZeroAmount.m_Enabled = true;
	ZeroAmount.m_Amount = 0.0f;
	EXPECT_FLOAT_EQ(QmTitleShimmerFactor(ZeroAmount, 0, 6, 0.3f), 0.0f);

	SQmTitleShimmer ZeroDuty = Off;
	ZeroDuty.m_Enabled = true;
	ZeroDuty.m_DutyCycle = 0.0f;
	EXPECT_FLOAT_EQ(QmTitleShimmerFactor(ZeroDuty, 0, 6, 0.3f), 0.0f);

	// 空文本或非法字符数不得产生高光，也不能让索引越界。
	EXPECT_FLOAT_EQ(QmTitleShimmerFactor(Off, 0, 0, 0.0f), 0.0f);
	EXPECT_FLOAT_EQ(QmTitleShimmerFactor(Off, 5, -1, 0.0f), 0.0f);

	// 默认配置是「抛光档 + 掠光开启」。
	const SQmTitleShimmer FromConfig = QmTitleShimmerFromConfig();
	EXPECT_TRUE(FromConfig.m_Enabled);
	EXPECT_FLOAT_EQ(FromConfig.m_Speed, 60.0f);
}

// 相位 = 字符序号 / 字符总数 + 时间 / 周期：t=0 时高光落在第 0 个字上，且扫过速度与长度无关。
TEST(QmTitleStyle, ShimmerPhaseIsLengthIndependent)
{
	SQmTitleShimmer Shimmer;
	Shimmer.m_Enabled = true;

	const float FirstChar = QmTitleShimmerFactor(Shimmer, 0, 6, 0.0f);
	EXPECT_GT(FirstChar, 0.0f);
	EXPECT_NEAR(FirstChar, Shimmer.m_Amount, 1e-6f);

	// 同样的时间，更长的头衔里高光覆盖的字符比例相同（第 0 个字都在峰值）。
	EXPECT_NEAR(QmTitleShimmerFactor(Shimmer, 0, 12, 0.0f), FirstChar, 1e-6f);

	// 周期 = 1 / (speed / 100) 秒：默认 100 时每秒扫过一整行。
	const float Period = 1.0f / (Shimmer.m_Speed / 100.0f);
	EXPECT_NEAR(QmTitleShimmerFactor(Shimmer, 0, 6, Period), FirstChar, 1e-4f);
	EXPECT_NEAR(QmTitleShimmerFactor(Shimmer, 0, 6, 2.0f * Period), FirstChar, 1e-4f);
	// 相位回绕的另一侧（接近周期末尾）也必须是峰值，否则最后几个字符会突然整片变亮。
	EXPECT_GT(QmTitleShimmerFactor(Shimmer, 0, 6, Period - 1e-4f), Shimmer.m_Amount * 0.9f);
	EXPECT_GT(FirstChar, QmTitleShimmerFactor(Shimmer, 3, 6, 0.0f));
}

// 高光强度必须落在 [0, Amount]：越界会让颜色溢出成纯白。
TEST(QmTitleStyle, ShimmerFactorStaysBounded)
{
	SQmTitleShimmer Shimmer;
	Shimmer.m_Enabled = true;
	Shimmer.m_DutyCycle = 0.5f;
	Shimmer.m_Amount = 0.5f;

	for(int CharIndex = 0; CharIndex < 6; ++CharIndex)
	{
		for(int Step = 0; Step < 200; ++Step)
		{
			const float Factor = QmTitleShimmerFactor(Shimmer, CharIndex, 6, (float)Step * 0.01f);
			EXPECT_GE(Factor, 0.0f);
			EXPECT_LE(Factor, Shimmer.m_Amount + 1e-6f);
		}
	}

	// 一个周期内每个字符都恰好被扫到一次，不会出现「永远不亮」的字符。
	for(int CharIndex = 0; CharIndex < 6; ++CharIndex)
	{
		bool Lit = false;
		for(int Step = 0; Step < 1000 && !Lit; ++Step)
			Lit = QmTitleShimmerFactor(Shimmer, CharIndex, 6, (float)Step * 0.001f) > 0.0f;
		EXPECT_TRUE(Lit) << CharIndex;
	}
}

// 掠光按字符数（不是字节数）分配相位：中文头衔用 UTF-8，一个汉字三字节，按字节计数会让光带错位。
TEST(QmTitleStyle, ShimmerCountsUtf8Chars)
{
	EXPECT_EQ(QmTitleShimmerUtf8CharCount(""), 0);
	EXPECT_EQ(QmTitleShimmerUtf8CharCount("QmClient"), 8);
	EXPECT_EQ(QmTitleShimmerUtf8CharCount("璇梦"), 2);
	EXPECT_EQ(QmTitleShimmerUtf8CharCount("开发者璇梦"), 5);
	// 非法字节序列必须停下，不能继续往后读。
	EXPECT_EQ(QmTitleShimmerUtf8CharCount("\xFF\xFE"), 0);
}

// 掠光与配色正交：它只按字符序号给出提亮量，不改变风格采样本身（静默期必须逐字节等于原色）。
TEST(QmTitleStyle, ShimmerLeavesStyleSamplingUntouched)
{
	const SQmTitleStyle *pStyle = QmTitleStyleById("turquoise");
	ASSERT_NE(pStyle, nullptr);

	SQmTitleShimmer Shimmer;
	Shimmer.m_Enabled = true;
	// 峰值落在第 0 个字，第 3 个字在同一时刻处于静默期。
	EXPECT_GT(QmTitleShimmerFactor(Shimmer, 0, 4, 0.0f), 0.0f);
	EXPECT_FLOAT_EQ(QmTitleShimmerFactor(Shimmer, 3, 4, 0.0f), 0.0f);

	// 采样函数本身不读掠光参数：静默期的颜色必须与风格原色完全一致。
	const ColorRGBA Base = QmTitleStyleSample(*pStyle, 0.0f, 0.0f);
	ExpectColorNear(Base, 0x00FFC8);
}

// 抛光档的绘制路径必须被真正接线：默认档走抛光，经典档仍保留旧路径。
TEST(QmTitleStyle, PolishedDrawPathIsWired)
{
	const std::string Header = ReadQmTitleStyleSource("src/game/client/qm_title_effect.h");
	const std::string Render = ReadQmTitleStyleSource("src/game/client/render.cpp");
	const std::string RenderHeader = ReadQmTitleStyleSource("src/game/client/render.h");
	const std::string Nameplates = ReadQmTitleStyleSource("src/game/client/components/nameplates.cpp");
	const std::string Menus = ReadQmTitleStyleSource("src/game/client/components/qmclient/menus_qmclient.cpp");
	ASSERT_FALSE(Header.empty());
	ASSERT_FALSE(Render.empty());
	ASSERT_FALSE(RenderHeader.empty());
	ASSERT_FALSE(Nameplates.empty());
	ASSERT_FALSE(Menus.empty());

	EXPECT_NE(Header.find("struct SQmTitlePolishStyle"), std::string::npos);
	EXPECT_NE(Header.find("enum EQmTitleEffect"), std::string::npos);
	EXPECT_NE(RenderHeader.find("void RenderTitleContainerWithPolishedEffects(STextContainerIndex TextContainerIndex, const SQmTitlePolishStyle &Style, float X, float Y) const;"), std::string::npos);
	EXPECT_NE(Render.find("void CRenderTools::RenderTitleContainerWithPolishedEffects"), std::string::npos);
	// 抛光档的每一层都必须用空描边绘制，否则引擎会为每层再补四个偏移描边，重新糊成一圈黑。
	EXPECT_NE(Render.find("auto DrawPass = [&](const ColorRGBA &Color, const float OffsetX, const float OffsetY)"), std::string::npos);
	EXPECT_NE(Render.find("TextRender()->RenderTextContainer(TextContainerIndex, Color, EmptyOutline, X + OffsetX, Y + OffsetY);"), std::string::npos);
	// 投影强度按文字亮度缩放：深色文字不会再被叠一层黑。
	EXPECT_NE(Render.find("Style.m_ShadowAlpha * Luminance * TextAlpha"), std::string::npos);
	// 名牌默认走抛光档，且动态风格在经典档下仍能回到旧路径。
	EXPECT_NE(Nameplates.find("if(Effect == (int)EQmTitleEffect::QM_TITLE_EFFECT_POLISHED || Effect == (int)EQmTitleEffect::QM_TITLE_EFFECT_SOLID)"), std::string::npos);
	EXPECT_NE(Nameplates.find("RenderTitleContainerWithPolishedEffects(m_TextContainerIndex, PolishStyle"), std::string::npos);
	EXPECT_NE(Nameplates.find("QmTitleRenderFillCursor(This.TextRender(), Cursor, m_aText, m_FontSize, m_TitleRenderStyle, (float)This.m_QmClient.TitleAnimationTime(), 1.0f, QmTitleShimmerFromConfig(), &m_TitleTextMetrics);"), std::string::npos);
	// 设置页提供空间效果选择，预览沿用现有绘制入口。
	EXPECT_NE(Menus.find("s_TitleEffectNames = {Localize(\"Polished\"), Localize(\"Solid\"), Localize(\"Classic\"), Localize(\"Off\")};"), std::string::npos);
	EXPECT_NE(Menus.find("pPreview->m_pRenderTools->RenderTitleContainerWithPolishedEffects(PreviewContainer, PreviewPolish"), std::string::npos);
	EXPECT_NE(Menus.find("QmTitleShimmerFromConfig()"), std::string::npos);

	// 默认值必须是抛光档（0），否则新外观不会成为现状；掠光速度默认开启。
	const std::string Config = ReadQmTitleStyleSource("src/engine/shared/config_variables_qmclient.h");
	ASSERT_FALSE(Config.empty());
	EXPECT_NE(Config.find("MACRO_CONFIG_INT(QmTitleEffect, qm_title_effect, 0, 0, 3,"), std::string::npos);
	EXPECT_NE(Config.find("MACRO_CONFIG_INT(QmTitleShimmerSpeed, qm_title_shimmer_speed, 60, 0, 400,"), std::string::npos);
}

// P4：头衔绘制走 Calamity 风格的固定描边 + 加法混合辉光。
TEST(QmTitleStyle, CalamityDrawPathIsWired)
{
	const std::string Header = ReadQmTitleStyleSource("src/game/client/render.h");
	const std::string Source = ReadQmTitleStyleSource("src/game/client/render.cpp");
	const std::string Nameplates = ReadQmTitleStyleSource("src/game/client/components/nameplates.cpp");
	ASSERT_FALSE(Header.empty());
	ASSERT_FALSE(Source.empty());
	ASSERT_FALSE(Nameplates.empty());

	// 空间效果的参数结构已迁到 qm_title_effect.h，render.h 只留绘制入口。
	const std::string EffectHeader = ReadQmTitleStyleSource("src/game/client/qm_title_effect.h");
	ASSERT_FALSE(EffectHeader.empty());
	EXPECT_NE(EffectHeader.find("struct SQmTitleEffectStyle"), std::string::npos);
	EXPECT_NE(Header.find("void RenderTitleContainerWithCalamityEffects(STextContainerIndex TextContainerIndex, const SQmTitleEffectStyle &Style, float X, float Y) const;"), std::string::npos);
	EXPECT_NE(Source.find("void CRenderTools::RenderTitleContainerWithCalamityEffects"), std::string::npos);
	// 辉光必须用加法混合，且用完恢复普通混合，否则会污染后续所有绘制
	EXPECT_NE(Source.find("Graphics()->BlendAdditive();"), std::string::npos);
	EXPECT_NE(Source.find("Graphics()->BlendNormal();"), std::string::npos);
	// 名牌在动态风格时必须走这条路径
	EXPECT_NE(Nameplates.find("RenderTitleContainerWithCalamityEffects(m_TextContainerIndex, EffectStyle"), std::string::npos);
	// 半径必须与 Calamity 的 4 + 16 * sin^5 同构
	EXPECT_NE(Nameplates.find("EffectStyle.m_BloomPulse = 16.0f * Breathe;"), std::string::npos);
	EXPECT_NE(Nameplates.find("EffectStyle.m_BloomRotation = BloomTime * 1.7f;"), std::string::npos);

	// 设置页必须提供实时预览，否则用户无法在选风格时看到实际效果。
	// 预览在卡片内展开的列表里绘制（不再用浮层下拉，层级与高度都可控）。
	const std::string Menus = ReadQmTitleStyleSource("src/game/client/components/qmclient/menus_qmclient.cpp");
	ASSERT_FALSE(Menus.empty());
	// 预览一律走 Calamity 路径；设置页提供实时预览，用户才能在选风格时看到实际效果
	EXPECT_NE(Menus.find("QmTitleRenderFillCursor(pTextRender, PreviewCursor, pPreview->m_aPreviewText"), std::string::npos);
	EXPECT_NE(Menus.find("m_pRenderTools->RenderTitleContainerWithCalamityEffects(PreviewContainer"), std::string::npos);
	// 列表画在卡片内部：展开时由卡片自己撑高，不再有浮层/弹窗坐标与裁剪
	EXPECT_EQ(Menus.find("s_TitleStyleDropDownState"), std::string::npos);
	EXPECT_EQ(Menus.find("DoSettingsDropDown(&TitleStyleControl"), std::string::npos);
	EXPECT_NE(Menus.find("RenderQmTitleStylePreviewEntry(&s_TitleStylePreviewContext, ItemCtx, ItemIndex, \"\");"), std::string::npos);
}

// 展开列表的条目与预览必须同源同序（都按 QmTitleStyleByIndex 取），一旦错位就会「选 A 显示 B」。
// 卡片高度必须跟着展开状态走，否则列表会顶出卡片、把保存/刷新挤到外面。
TEST(QmTitleStyle, StyleListEntriesAndPreviewsShareIndexOrder)
{
	const std::string Menus = ReadQmTitleStyleSource("src/game/client/components/qmclient/menus_qmclient.cpp");
	ASSERT_FALSE(Menus.empty());

	// 条目只画效果本体：第 0 项是「跟随服务器」哨兵，风格条目用 Index - 1
	EXPECT_NE(Menus.find("FollowServerEntry ? QmTitleStyleById(pPreview->m_aFollowPreviewStyleId) : QmTitleStyleByIndex(Index - 1)"), std::string::npos);
	EXPECT_NE(Menus.find("s_aTitleStylePreviewContainers[Index]"), std::string::npos);
	EXPECT_NE(Menus.find("const SQmTitleStyle *pPicked = QmTitleStyleByIndex(ItemIndex - 1);"), std::string::npos);
	// 任何条目都必须有预览：服务端已下发风格时也不能把风格条目画成空白行
	EXPECT_EQ(Menus.find("m_AllowEntryPreview"), std::string::npos);
	EXPECT_NE(Menus.find("\t\tpStyle = QmTitleStyleByIndex(0);"), std::string::npos);

	// 展开面板必须完整占位，后续控件才能排在列表下方。
	EXPECT_NE(Menus.find("Height += LineSpacing + ResolveQmTitleStylePanelHeight(VisibleStyleItems);"), std::string::npos);
	EXPECT_NE(Menus.find("Content.HSplitTop(ResolveQmTitleStylePanelHeight(VisibleStyleItems), &Panel, &Content);"), std::string::npos);
	EXPECT_EQ(Menus.find("Panel.h = ResolveQmTitleStylePanelHeight(VisibleStyleItems);"), std::string::npos);
	// 展开状态改变可见行数，必须进卡片布局版本，否则不会重新测量
	EXPECT_NE(Menus.find("CardLayoutRevision = CardLayoutRevision * 1099511628211ULL ^ (uint64_t)(s_TitleStyleExpanded ? 1u : 0u);"), std::string::npos);
	// 选择写回仍然是 qm_title_style 的 id（不是下标）
	EXPECT_NE(Menus.find("str_copy(g_Config.m_QmTitleStyle, pPicked->m_pId);"), std::string::npos);

	// 「跟随服务器」作为第 0 项的回滚口：选中它必须关掉本地风格
	EXPECT_NE(Menus.find("g_Config.m_QmTitleStyleEnabled = 0;"), std::string::npos);
	EXPECT_NE(Menus.find("g_Config.m_QmTitleStyleEnabled = 0;"), std::string::npos);

	// 预览文本固定为 [赞助者]：既不显示效果名，也不显示玩家名字
	EXPECT_NE(Menus.find("const char *pSponsorPreview = \"[赞助者]\";"), std::string::npos);
	EXPECT_EQ(Menus.find("pPreviewTitle = \"QmClient\""), std::string::npos);
	EXPECT_EQ(Menus.find("QmTitleStyleByIndex(Index)->m_pLabel"), std::string::npos);
}

// 测量行数与真实绘制调用对照，新增标签或按钮却漏改测量时必须失败。
TEST(QmTitleStyle, SponsorCardMeasuresEveryRenderedRow)
{
	const std::string Menus = ReadQmTitleStyleSource("src/game/client/components/qmclient/menus_qmclient.cpp");
	const size_t Start = Menus.find("SSettingsCardDefinition TitleCard;");
	const size_t End = Menus.find("vCards.push_back(std::move(TitleCard));", Start);
	ASSERT_NE(Start, std::string::npos);
	ASSERT_NE(End, std::string::npos);
	const std::string Card = Menus.substr(Start, End - Start);
	int RenderedRows = 0;
	for(size_t Position = 0; (Position = Card.find("Row = NextRow();", Position)) != std::string::npos; ++Position)
		++RenderedRows;
	ASSERT_EQ(RenderedRows, 22);
	EXPECT_NE(Card.find("ResolveSettingsRowsHeight(TitleAdvanced ? 22 : 12, LineHeight, LineSpacing)"), std::string::npos);
	EXPECT_NE(Card.find("Height += LineSpacing + TitlePreviewHeight;"), std::string::npos);
	EXPECT_NE(Card.find("Content.HSplitTop(TitlePreviewHeight, &Preview, &Content);"), std::string::npos);

	// 点击只改变下一帧状态，本帧测量和渲染共用已捕获的展开状态。
	EXPECT_NE(Menus.find("const bool TitleStyleExpanded = s_TitleStyleExpanded;"), std::string::npos);
	EXPECT_NE(Card.find("TitleCard.m_Measure = [LineHeight, LineSpacing, TitleStyleExpanded, TitleAdvanced, TitlePreviewHeight]"), std::string::npos);
	EXPECT_NE(Card.find("LineSpacing, ReadOnly, TitleStyleExpanded, TitleAdvanced, TitlePreviewHeight, Metrics](CUIRect Content)"), std::string::npos);
	EXPECT_EQ(Card.find("if(s_TitleStyleExpanded)"), std::string::npos);
}

// 文本容器生成后光标已在行末，不能把它当屏幕坐标再次叠加。
TEST(QmTitleStyle, StylePreviewUsesLocalOriginAndSingleScreenOffset)
{
	const std::string Menus = ReadQmTitleStyleSource("src/game/client/components/qmclient/menus_qmclient.cpp");
	const size_t Start = Menus.find("static void RenderQmTitleStylePreviewEntry(");
	const size_t End = Menus.find("static void UpdateKeywordRulesLayoutState(", Start);
	ASSERT_NE(Start, std::string::npos);
	ASSERT_NE(End, std::string::npos);
	const std::string Preview = Menus.substr(Start, End - Start);
	EXPECT_NE(Preview.find("PreviewCursor.SetPosition(vec2(0.0f, 0.0f));"), std::string::npos);
	EXPECT_EQ(Preview.find("PreviewCursor.m_X"), std::string::npos);
	EXPECT_NE(Preview.find("const float PreviewX = PreviewRect.x + std::max(0.0f, (PreviewRect.w - PreviewTextWidth) * 0.5f);"), std::string::npos);
	EXPECT_NE(Preview.find("PreviewPolish, PreviewX, PreviewY);"), std::string::npos);
	EXPECT_NE(Preview.find("PreviewEffect, PreviewX, PreviewY);"), std::string::npos);
}

// 高级模式只折叠设置；全部十个配置入口必须在父级之内，预览与保存仍在外面。
TEST(QmTitleStyle, AdvancedSettingsContainAllOptionsWithoutResettingThem)
{
	const std::string Menus = ReadQmTitleStyleSource("src/game/client/components/qmclient/menus_qmclient.cpp");
	const size_t Start = Menus.find("if(TitleAdvanced)");
	const size_t End = Menus.find("// 成品预览始终可见", Start);
	ASSERT_NE(Start, std::string::npos);
	ASSERT_NE(End, std::string::npos);
	const std::string Advanced = Menus.substr(Start, End - Start);
	for(const char *pOption : {"QmTitleColorMode", "QmTitleColor", "QmTitleOpacity", "QmTitlePhase", "QmTitleEffect", "QmTitleBloom", "QmTitleShimmerSpeed", "QmTitleBobWavelength", "QmTitleBobSpeed", "QmTitleBobPixelSnap"})
		EXPECT_NE(Advanced.find(std::string("g_Config.m_") + pOption), std::string::npos) << pOption;
	EXPECT_EQ(Advanced.find("SaveTitleProfile"), std::string::npos);
	EXPECT_NE(Menus.find("const bool TitleAdvanced = g_Config.m_QmTitleAdvanced != 0;"), std::string::npos);
	EXPECT_NE(Menus.find("(uint64_t)(g_Config.m_QmTitleAdvanced != 0)"), std::string::npos);
	EXPECT_NE(Menus.find("g_Config.m_QmTitleAdvanced ^= 1;"), std::string::npos);
	const std::string Config = ReadQmTitleStyleSource("src/engine/shared/config_variables_qmclient.h");
	EXPECT_NE(Config.find("MACRO_CONFIG_INT(QmTitleAdvanced, qm_title_advanced, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE,"), std::string::npos);
	const std::string Render = ReadQmTitleStyleSource("src/game/client/components/qmclient/qm_title_render.cpp");
	EXPECT_EQ(Render.find("m_QmTitleAdvanced"), std::string::npos);
}

// 成品预览使用输入文本和当前草稿风格，颜色、透明度、光晕及关闭档都必须反映最终配置。
TEST(QmTitleStyle, FinishedPreviewUsesDraftAndCompleteAppearance)
{
	const std::string Menus = ReadQmTitleStyleSource("src/game/client/components/qmclient/menus_qmclient.cpp");
	const size_t Start = Menus.find("static void RenderQmTitleFinishedPreview(");
	const size_t End = Menus.find("static void UpdateKeywordRulesLayoutState(", Start);
	ASSERT_NE(Start, std::string::npos);
	ASSERT_NE(End, std::string::npos);
	const std::string Preview = Menus.substr(Start, End - Start);
	EXPECT_NE(Menus.find("s_Title.GetString(), pServerStyleId, g_Config.m_QmTitleStyleEnabled != 0, pLocalStyleId"), std::string::npos);
	EXPECT_NE(Preview.find("ResolveQmTitleColorStyle(g_Config.m_QmTitleColorMode, g_Config.m_QmTitleColor, g_Config.m_QmTitleOpacity, ServerRainbow)"), std::string::npos);
	// 预览必须与实际名牌走同一条风格解析路径：本地兜底显式传入，而不是在预览里另写一套回退。
	EXPECT_NE(Preview.find("QmTitleResolveRenderStyle(pStyleId, LocalStyleEnabled, pLocalStyleId)"), std::string::npos);
	EXPECT_NE(Preview.find("QmTitleShimmerFromConfig()"), std::string::npos);
	EXPECT_NE(Preview.find("QmAddTitleRainbowSplits"), std::string::npos);
	EXPECT_NE(Preview.find("g_Config.m_QmTitleBloom >= 2 ? 16 : 6"), std::string::npos);
	EXPECT_NE(Preview.find("PreviewPolish.m_TextAlpha = Alpha;"), std::string::npos);
	EXPECT_NE(Preview.find("pTextRender->RenderTextContainer(PreviewContainer, Color, OutlineColor, PreviewX, PreviewY);"), std::string::npos);
	EXPECT_EQ(Preview.find("SaveTitleProfile"), std::string::npos);
	EXPECT_NE(Menus.find("Ui()->ClipEnable(&PreviewArea);"), std::string::npos);
}

// 配色优先级：本地配色档（单色/彩虹）必须压过服务端/本地风格自带的渐变颜色，风格只保留浮动与掠光。
// 现状曾经是「选了单色也没用，起效的还是预设渐变」，这条用例锁住优先级，防止再次反转。
TEST(QmTitleStyle, LocalColorModeOverridesStyleColors)
{
	const std::string Source = ReadQmTitleStyleSource("src/game/client/components/qmclient/qm_title_render.cpp");
	const std::string Header = ReadQmTitleStyleSource("src/game/client/components/qmclient/qm_title_render.h");
	const std::string Chat = ReadQmTitleStyleSource("src/game/client/components/chat.cpp");
	const std::string Nameplates = ReadQmTitleStyleSource("src/game/client/components/nameplates.cpp");
	const std::string Menus = ReadQmTitleStyleSource("src/game/client/components/qmclient/menus_qmclient.cpp");
	ASSERT_FALSE(Source.empty());
	ASSERT_FALSE(Header.empty());
	ASSERT_FALSE(Chat.empty());
	ASSERT_FALSE(Nameplates.empty());
	ASSERT_FALSE(Menus.empty());

	// 只有单色与彩虹档覆盖风格颜色：「跟随服务器」档保持既有表现。
	EXPECT_NE(Header.find("bool m_ColorOverride = false;"), std::string::npos);
	EXPECT_NE(Source.find("Style.m_ColorOverride = g_Config.m_QmTitleColorMode == (int)EQmTitleColorMode::SINGLE ||"), std::string::npos);
	EXPECT_NE(Source.find("g_Config.m_QmTitleColorMode == (int)EQmTitleColorMode::RAINBOW;"), std::string::npos);
	// 被覆盖时不能再采样风格颜色，否则渐变会盖在本地配色上。
	EXPECT_NE(Source.find("if(Style.m_ColorOverride)"), std::string::npos);
	EXPECT_NE(Source.find("LeftColor = Color.WithAlpha(Alpha);"), std::string::npos);
	EXPECT_NE(Source.find("RightColor = ColorEnd.WithAlpha(Alpha);"), std::string::npos);
	// 覆盖档也必须逐帧重建：色段由本函数生成，静态判断不能把它当成不随时间变化的文本。
	EXPECT_NE(Source.find("return UseBob || Shimmer.m_Enabled || Style.m_ColorOverride || TitleStyle.m_Mode != EQmTitleStyleMode::Static;"), std::string::npos);
	// 浮动与掠光不能因为颜色被接管就一起丢掉。
	EXPECT_NE(Source.find("void QmTitleRenderFillMotionOffsets"), std::string::npos);
	EXPECT_NE(Header.find("void QmTitleRenderFillMotionOffsets"), std::string::npos);

	// 三条绘制路径（聊天、名牌、设置页成品预览）都必须处理覆盖档，漏一条就会出现「某处颜色不跟随」。
	EXPECT_NE(Chat.find("if(TitleRenderStyle.m_ColorOverride)"), std::string::npos);
	EXPECT_NE(Nameplates.find("if(m_TitleRenderStyle.m_ColorOverride)"), std::string::npos);
	EXPECT_NE(Menus.find("if(DynamicStyle && RenderStyle.m_ColorOverride)"), std::string::npos);
	// 名牌在覆盖档下彩虹要能在「只写浮动」之后补上色段。
	EXPECT_NE(Nameplates.find("QmAddTitleRainbowSplits(Cursor, m_aText, 1.0f);"), std::string::npos);
}

// 服务端 presence 只能在「确实拿到本服务器的有效名单」时丢弃：否则请求失败会让头衔与风格瞬间消失，
// 表现为颜色在本地配色档与服务端风格之间来回闪。
TEST(QmTitleStyle, TitlePresenceSurvivesFailedRefresh)
{
	const std::string Client = ReadQmTitleStyleSource("src/game/client/components/qmclient/qmclient.cpp");
	ASSERT_FALSE(Client.empty());

	const size_t ListStart = Client.find("if(m_pTitleList && m_pTitleList->Done())");
	ASSERT_NE(ListStart, std::string::npos);
	const size_t ListEnd = Client.find("if(Client()->State() != IClient::STATE_ONLINE", ListStart);
	ASSERT_NE(ListEnd, std::string::npos);
	const std::string List = Client.substr(ListStart, ListEnd - ListStart);

	const size_t Success = List.find("m_pTitleList->StatusCode() == 200 && str_comp(aServer, m_aTitlePendingServer) == 0");
	ASSERT_NE(Success, std::string::npos);
	const size_t Clear = List.find("mem_zero(m_aTitleExpires, sizeof(m_aTitleExpires));");
	ASSERT_NE(Clear, std::string::npos);
	// 清空必须发生在成功分支内部、且在校验之后。
	EXPECT_GT(Clear, Success) << "清空旧 presence 的位置必须落在拿名单成功的分支里";
}

// 相位基准：对齐服务端时间并取模。直接用 Unix 时间戳量级会让 float 相位失效（1e9 时 ULP≈64 秒）。
TEST(QmTitleStyle, AnimationTimeAlignsToServerAndWraps)
{
	// 没有服务端时间时退回本地时间
	EXPECT_DOUBLE_EQ(QmTitleAnimationTime(1234.5, 0.0, false), 1234.5);

	// 对齐后取模到一小时
	const double ServerNow = 1789223928.0;
	EXPECT_DOUBLE_EQ(QmTitleAnimationTime(100.0, ServerNow - 100.0, true), std::fmod(ServerNow, 3600.0));

	// 两个客户端本地时间不同，但在同一服务端时刻必须得到相同相位
	EXPECT_DOUBLE_EQ(QmTitleAnimationTime(500.0, ServerNow - 500.0, true), QmTitleAnimationTime(500000.0, ServerNow - 500000.0, true));

	// 结果始终落在 [0, 3600)，即 float 精度充足的量级
	for(const double LocalTime : {-5.0, 0.0, 3599.9, 3600.0, 7200.5})
	{
		const double Wrapped = QmTitleAnimationTime(LocalTime, 0.0, false);
		EXPECT_GE(Wrapped, 0.0) << LocalTime;
		EXPECT_LT(Wrapped, 3600.0) << LocalTime;
	}

	// 取模周期必须能整除所有风格周期，否则边界处会出现相位跳变
	for(int Index = 0; Index < QmTitleStyleCount(); ++Index)
	{
		const SQmTitleStyle *pStyle = QmTitleStyleByIndex(Index);
		ASSERT_NE(pStyle, nullptr);
		if(pStyle->m_Mode == EQmTitleStyleMode::Static)
			continue;
		const double Ratio = 3600.0 / (double)pStyle->m_PeriodSec;
		EXPECT_DOUBLE_EQ(Ratio, (double)(int)Ratio) << pStyle->m_pId << " 的周期不能整除取模周期";
	}
}

// 服务端时间偏移做指数平滑，首次估算直接采纳。
TEST(QmTitleStyle, ServerTimeOffsetSmoothing)
{
	EXPECT_DOUBLE_EQ(QmTitleUpdateServerTimeOffset(0.0, false, 1000.0), 1000.0);
	EXPECT_DOUBLE_EQ(QmTitleUpdateServerTimeOffset(1000.0, true, 1100.0), 1020.0);

	// 单次抖动经过平滑后只引起很小的偏移变化
	const double Steady = QmTitleUpdateServerTimeOffset(1020.0, true, 1030.0);
	const double Noisy = QmTitleUpdateServerTimeOffset(Steady, true, 1000.0);
	EXPECT_LT(std::fabs(Noisy - Steady), 30.0);
}

// 聊天行必须容纳整段波浪的上下边界，不能随当前动画相位伸缩。
TEST(QmTitleStyle, ChatBobPaddingContainsWrappedRows)
{
	for(const float Amplitude : {0.0f, 4.0f, 12.0f})
	{
		for(const float PixelSize : {0.25f, 0.75f, 1.25f})
		{
			for(const float QuantizeStep : {0.0f, 1.0f})
			{
				const SQmTitleBobStyle Bob{Amplitude, 64.0f, 0.4f, QuantizeStep};
				const float Padding = QmTitleStyleBobPadding(Bob, PixelSize);
				EXPECT_GE(Padding, Amplitude);
				EXPECT_NEAR(Padding / PixelSize, std::round(Padding / PixelSize), 0.0001f);
				const float FontSize = 6.0f;
				const float RowHeight = FontSize + 2.0f * Padding;
				for(int Step = 0; Step < 80; ++Step)
				{
					const float Offset = QmTitleStyleBobOffset(Bob, Step * 0.17f, Step * 3.0f);
					// 第一行上浮不越顶，最后一行下沉不越底。
					EXPECT_GE(Padding + Offset, 0.0f);
					EXPECT_LE(2.0f * RowHeight + Padding + FontSize + Offset, 3.0f * RowHeight);
					// 相邻两行即使分别到达波峰、波谷，也有足够间距。
					EXPECT_LE(Padding + FontSize + std::fabs(Offset), RowHeight + Padding - std::fabs(Offset));
				}
			}
		}
	}
	EXPECT_FLOAT_EQ(QmTitleStyleBobPadding({0.0f, 64.0f, 0.4f, 0.0f}, 0.75f), 0.0f);
	EXPECT_FLOAT_EQ(QmTitleStyleBobPadding({4.0f, 0.0f, 0.4f, 0.0f}, 0.75f), 0.0f);
}

// 聊天必须使用完整风格与名牌的掠光配置，偏移仅作用于头衔。
TEST(QmTitleStyle, ChatPreservesBobAndShimmerForEveryAuthor)
{
	const std::string Chat = ReadQmTitleStyleSource("src/game/client/components/chat.cpp");
	const size_t Start = Chat.find("const auto AppendQmTitle =");
	const size_t End = Chat.find("\n\t\t};", Start);
	ASSERT_NE(Start, std::string::npos);
	ASSERT_NE(End, std::string::npos);
	const std::string Append = Chat.substr(Start, End - Start);
	EXPECT_EQ(Append.find("m_Bob.m_Amplitude = 0.0f"), std::string::npos);
	EXPECT_NE(Append.find("TitleRenderStyle, TitleAnimationTime, 1.0f, TitleShimmer"), std::string::npos);
	EXPECT_NE(Append.find("LineCursor.m_vCharOffsets.clear();"), std::string::npos);
	EXPECT_NE(Chat.find("const SQmTitleShimmer TitleShimmer = QmTitleShimmerFromConfig();"), std::string::npos);
	EXPECT_NE(Chat.find("AppendQmTitle(Line.m_aQmTitle, NameColor, Line.m_ClientId)"), std::string::npos);
	EXPECT_NE(Chat.find("AppendQmTitle(Line.m_vMergedAuthors[i].m_aQmTitle, Line.m_vMergedAuthors[i].m_NameColor, Line.m_vMergedAuthors[i].m_ClientId)"), std::string::npos);
}

// 测量、换行、图片表情和实际绘制共用留白，幅度改变时必须重算两种聊天宽度的高度。
TEST(QmTitleStyle, ChatMeasuresAndRendersTheSameBobSpace)
{
	const std::string Chat = ReadQmTitleStyleSource("src/game/client/components/chat.cpp");
	EXPECT_NE(Chat.find("MeasureCursor.m_LineSpacing = 2.0f * TitleBobPadding;"), std::string::npos);
	EXPECT_NE(Chat.find("LineCursor.m_LineSpacing = 2.0f * TitleBobPadding;"), std::string::npos);
	EXPECT_NE(Chat.find("Line.m_TextYOffset + TitleBobPadding"), std::string::npos);
	EXPECT_NE(Chat.find("EmojiLayout.m_RequiredHeight + 2.0f * TitleBobPadding"), std::string::npos);
	const size_t Start = Chat.find("if(TitleHidden || TitleBobPadding != Line.m_QmTitleBobPadding)");
	const size_t End = Chat.find("const bool LinePrepared", Start);
	ASSERT_NE(Start, std::string::npos);
	ASSERT_NE(End, std::string::npos);
	const std::string Invalidation = Chat.substr(Start, End - Start);
	EXPECT_NE(Invalidation.find("Line.m_aYOffset[0] = -1.0f;"), std::string::npos);
	EXPECT_NE(Invalidation.find("Line.m_aYOffset[1] = -1.0f;"), std::string::npos);
}

// 相同标题跨帧复用前缀测量；UTF-8 的字节边界和整段字距保持原样。
TEST(QmTitleMetrics, ReusesWholePrefixWidthsAcrossAnimationFrames)
{
	CQmTitleTextMetrics Metrics;
	CQmTitleTextMetrics::SContext Context;
	Context.m_FontSize = 12.0f;
	int Measurements = 0;
	const auto Measure = [&](const char *pPrefix) {
		++Measurements;
		const std::string Prefix(pPrefix);
		if(Prefix == "A")
			return 10.0f;
		if(Prefix == "AV")
			return 17.0f; // 整段测量保留 AV 字距，不能把单字宽度相加。
		return 29.0f;
	};
	for(int Frame = 0; Frame < 240; ++Frame)
	{
		Metrics.Update("AV中", Context, Measure);
		EXPECT_FLOAT_EQ(Metrics.PrefixWidth(1, 0.0f), 10.0f);
		EXPECT_FLOAT_EQ(Metrics.PrefixWidth(2, 10.0f), 17.0f);
		EXPECT_FLOAT_EQ(Metrics.PrefixWidth(5, 17.0f), 29.0f);
	}
	EXPECT_EQ(Measurements, 3);
}

TEST(QmTitleMetrics, TextFontScaleAndFlagsInvalidateCachedMeasurements)
{
	CQmTitleTextMetrics Metrics;
	CQmTitleTextMetrics::SContext Context;
	int Measurements = 0;
	const auto Measure = [&](const char *) { return float(++Measurements); };
	Metrics.Update("中", Context, Measure);
	Metrics.Update("文", Context, Measure);
	Context.m_FontSize = 14.0f;
	Metrics.Update("文", Context, Measure);
	Context.m_ScreenScale.x = 2.0f;
	Metrics.Update("文", Context, Measure);
	Context.m_ScreenScale.y = 2.0f;
	Metrics.Update("文", Context, Measure);
	Context.m_RenderFlags = 1;
	Metrics.Update("文", Context, Measure);
	Context.m_FontPreset = 1;
	Metrics.Update("文", Context, Measure);
	EXPECT_EQ(Measurements, 7);
	// 字体语言切换和资源重建沿用名牌文本容器的失效通知。
	Metrics.Reset();
	Metrics.Update("文", Context, Measure);
	EXPECT_EQ(Measurements, 8);
	EXPECT_FLOAT_EQ(Metrics.PrefixWidth(3, 0.0f), 8.0f);
}

TEST(QmTitleMetrics, PreservesPrefixBufferBoundaryAndEmptyTitles)
{
	CQmTitleTextMetrics Metrics;
	CQmTitleTextMetrics::SContext Context;
	int Measurements = 0;
	const auto Measure = [&](const char *pPrefix) {
		++Measurements;
		return float(str_length(pPrefix));
	};
	const std::string Text(64, 'A');
	Metrics.Update(Text.c_str(), Context, Measure);
	EXPECT_EQ(Measurements, 63);
	EXPECT_FLOAT_EQ(Metrics.PrefixWidth(63, 62.0f), 63.0f);
	EXPECT_FLOAT_EQ(Metrics.PrefixWidth(64, 63.0f), 63.0f);
	Metrics.Update("", Context, Measure);
	EXPECT_EQ(Measurements, 63);
	Metrics.Update("A", Context, Measure);
	EXPECT_EQ(Measurements, 64);
	EXPECT_FLOAT_EQ(Metrics.PrefixWidth(1, 0.0f), 1.0f);
}
