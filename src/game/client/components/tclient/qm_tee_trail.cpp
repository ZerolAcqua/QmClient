// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "qm_tee_trail.h"

#include "trails.h"

#include <base/math.h>

#include <algorithm>
#include <cmath>

// 5 套 Tee 拖尾样式的几何构建。每套样式的灵感来源与分层结构见
// docs/qmclient/tee_trail_styles.md；这里只做「输入采样 -> 输出四边形」的纯计算。
//
// 设计要点：五套样式**不是同一骨架换五种配色**，而是各自一套独立的几何语言与运动方式：
// - 咒焰·黑闪：横向扇开的翻卷火舌（越靠尾端张得越开）+ 头部爆裂环与放射尖刺 + 分叉电弧，
//   短促爆发后整体压暗收束；
// - 紫电·雷切：一太刀决断的斩线（横跨轨迹的薄斩击沿轨迹推进）+ 缠绕乱舞的紫电雷纹 + 落点雷环；
// - 灵光·流萤：三股正弦编织的柔软细丝 + 断续光缕 + 交叠的柔光光团 + 缓慢漂浮的光点；
// - 虚空·暗影：墨黑实体烟身（头部厚、尾部散）+ 灰白薄边 + 向外卷曲的触须 + 悬浮碎片；
// - 黄金·赐福：三道金丝编织流动 + 横向交织细弧 + 赐福脉冲闪光 + 缓缓上升的金尘。
//
// 其余约定：
// - 拖尾采样从新到旧排列，vTrail[0] 是 Tee 当前位置；
// - 所有随时间变化的形态都走 CurTime（与 m_Tick 同一时间基），因此同一输入必然同输出；
// - 随机细节全部来自确定性哈希（Seed + 序号），不使用 rand()，同屏多个 Tee 互不干扰；
// - 不依赖任何贴图，输出的是无贴图自由形变四边形，因此形状只能靠几何与逐顶点透明度堆出来。

namespace
{
	// 效果节奏按 50 tick/s 折算成秒；寿命、年龄仍以 tick 为单位与拖尾采样对齐。
	constexpr float TICK_SECONDS = 1.0f / 50.0f;

	// 采样规模上限：无论拖尾多长，单帧构建的采样点与四边形数量都有界。
	constexpr int MAX_SAMPLES = 48;
	constexpr float MIN_SAMPLE_SPACING = 1.5f;

	// 总位移小于该值视为原地不动：不生成特效（停下后尾迹自然消散）。
	constexpr float MIN_TRAIL_EXTENT = 3.0f;
	// 单段位移超过该值视为传送：整条不画，避免拉出一条穿屏长条。
	constexpr float MAX_SEGMENT_DIST = 192.0f;
	// 整体透明度低于该值视为完全不可见。
	constexpr float MIN_VISIBLE_ALPHA = 0.002f;
	// 顶点透明度低于该值就不必提交（消散末端大量四边形会落到这里）。
	constexpr float MIN_QUAD_ALPHA = 0.002f;

	// 拖尾末端软收：最后这一段弧长上透明度渐变到 0，
	// 免得采样窗口走完时整条被硬生生截断。
	constexpr float TAIL_SOFT_START = 0.70f;

	// 每套样式的存活时间与空间包络。
	// 寿命必须短于拖尾历史窗口（默认 25 tick）叠加的消散时间，
	// 这样 Tee 停下后尾迹会先淡完再随采样窗口一起消失，不会突然整条不见。
	// Reach 是「特效沿轨迹铺多远」：黑闪是短促爆发（0.55），紫电与赐福几乎贯穿全条。
	struct SStyleSpec
	{
		float m_Life;
		float m_FadePower;
		float m_Reach;
	};

	constexpr SStyleSpec STYLE_SPECS[qm_tee_trail::STYLE_COUNT] = {
		{1.0f, 1.0f, 1.0f}, // STYLE_ORIGINAL：不参与新样式渲染
		{28.0f, 1.55f, 0.55f}, // 咒焰·黑闪：短促爆发，只在 Tee 身前一小段炸开
		{20.0f, 1.35f, 1.00f}, // 紫电·雷切：一闪即散，但斩线与主雷贯穿整条
		{42.0f, 1.15f, 1.00f}, // 灵光·流萤：缓慢漂浮
		{36.0f, 1.25f, 0.90f}, // 虚空·暗影：烟絮拖尾
		{45.0f, 1.05f, 1.00f}, // 黄金·赐福：缓慢衰减
	};

	// 样式自带配色（α 统一留 1.0，实际透明度在分层时按年龄叠加）。
	struct SPalette
	{
		ColorRGBA m_Glow; // 外层辉光
		ColorRGBA m_Main; // 主体
		ColorRGBA m_Core; // 核心 / 亮线
		ColorRGBA m_Accent; // 描边 / 火花
		ColorRGBA m_Deep; // 尾端暗部
	};

	const SPalette STYLE_PALETTES[qm_tee_trail::STYLE_COUNT] = {
		// STYLE_ORIGINAL：占位，原版拖尾不使用本模块配色。
		{ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f)},
		// 咒焰·黑闪：蓝青咒力外放主导，近黑核心 + 猩红描边只做点缀。
		{ColorRGBA(0.30f, 0.72f, 1.00f, 1.0f), ColorRGBA(0.16f, 0.42f, 0.98f, 1.0f), ColorRGBA(0.02f, 0.02f, 0.04f, 1.0f), ColorRGBA(0.82f, 0.06f, 0.14f, 1.0f), ColorRGBA(0.05f, 0.13f, 0.42f, 1.0f)},
		// 紫电·雷切：紫色雷炎，全部压在紫 → 近白紫区间，不出现任何暖色。
		{ColorRGBA(0.44f, 0.20f, 0.95f, 1.0f), ColorRGBA(0.62f, 0.32f, 1.00f, 1.0f), ColorRGBA(0.95f, 0.92f, 1.00f, 1.0f), ColorRGBA(0.80f, 0.66f, 1.00f, 1.0f), ColorRGBA(0.22f, 0.06f, 0.52f, 1.0f)},
		// 灵光·流萤：蓝白流光 + 漂浮光点。
		{ColorRGBA(0.34f, 0.62f, 1.00f, 1.0f), ColorRGBA(0.70f, 0.88f, 1.00f, 1.0f), ColorRGBA(0.96f, 0.99f, 1.00f, 1.0f), ColorRGBA(0.82f, 0.94f, 1.00f, 1.0f), ColorRGBA(0.18f, 0.36f, 0.76f, 1.0f)},
		// 虚空·暗影：墨黑烟絮 + 灰白薄边。
		{ColorRGBA(0.44f, 0.48f, 0.61f, 1.0f), ColorRGBA(0.03f, 0.03f, 0.05f, 1.0f), ColorRGBA(0.76f, 0.78f, 0.82f, 1.0f), ColorRGBA(0.62f, 0.65f, 0.70f, 1.0f), ColorRGBA(0.16f, 0.16f, 0.20f, 1.0f)},
		// 黄金·赐福：金色丝线 + 金尘。
		{ColorRGBA(1.00f, 0.79f, 0.34f, 1.0f), ColorRGBA(1.00f, 0.88f, 0.55f, 1.0f), ColorRGBA(1.00f, 0.97f, 0.84f, 1.0f), ColorRGBA(1.00f, 0.90f, 0.60f, 1.0f), ColorRGBA(0.62f, 0.40f, 0.08f, 1.0f)},
	};

	// 等距重采样后的拖尾点：位置、朝向、年龄、半宽、弧长进度与输入透明度。
	struct SSample
	{
		vec2 m_Pos = vec2(0.0f, 0.0f);
		vec2 m_Tangent = vec2(1.0f, 0.0f);
		vec2 m_Normal = vec2(0.0f, 1.0f);
		float m_Age = 0.0f;
		float m_Width = 1.0f;
		float m_S = 0.0f; // 0 = 头部，1 = 尾部
		float m_RelAlpha = 1.0f; // 输入透明度 / 整体透明度
	};

	// 带状路径点：位置 + 半宽 + 颜色。
	struct SPathPoint
	{
		vec2 m_Pos;
		float m_HalfWidth;
		ColorRGBA m_Col;
	};

	// 逐点扰动：把光滑的带状路径打散成有撕裂边缘的形态。
	struct SStrandJitter
	{
		const std::vector<float> *m_pWidthScale = nullptr; // 宽度系数
		const std::vector<float> *m_pAlphaScale = nullptr; // 透明度系数
		const std::vector<vec2> *m_pDrift = nullptr; // 额外位移（世界坐标，用于上浮 / 摆动）
		int m_SampleStep = 1; // 取样步长，>1 用于外圈柔光这类不需要逐点的层
	};

	// 沿轨迹方向的空间包络：头部权重（前段淡入）与尾部权重（后段淡出）。
	// 黑闪用它把能量压成一次短促爆发，赐福用它把金光铺满整条。
	float HeadEnvelope(float S, float Reach)
	{
		return 1.0f - std::clamp(S / std::max(Reach, 0.01f), 0.0f, 1.0f);
	}

	float TailEnvelope(float S)
	{
		return 1.0f - std::pow(std::clamp((S - TAIL_SOFT_START) / (1.0f - TAIL_SOFT_START), 0.0f, 1.0f), 1.4f);
	}

	float Fract(float Value)
	{
		return Value - std::floor(Value);
	}

	unsigned Hash(unsigned Value)
	{
		Value ^= Value >> 16;
		Value *= 0x7feb352du;
		Value ^= Value >> 15;
		Value *= 0x846ca68bu;
		Value ^= Value >> 16;
		return Value;
	}

	// 确定性随机：同一 Seed 永远给出同一结果，保证画面可重放、可测试。
	float Hash01(unsigned Value)
	{
		return (float)(Hash(Value) & 0xffffffu) / (float)0xffffffu;
	}

	float HashSigned(unsigned Value)
	{
		return Hash01(Value) * 2.0f - 1.0f;
	}

	ColorRGBA MixColor(const ColorRGBA &A, const ColorRGBA &B, float Amount)
	{
		return ColorRGBA(
			mix(A.r, B.r, Amount),
			mix(A.g, B.g, Amount),
			mix(A.b, B.b, Amount),
			mix(A.a, B.a, Amount));
	}

	// 年龄 -> 透明度系数：寿命内单调递减，超出寿命即为 0（消散完成）。
	float Fade(float Age, const SStyleSpec &Spec)
	{
		const float Remaining = std::clamp(1.0f - Age / Spec.m_Life, 0.0f, 1.0f);
		return std::pow(Remaining, Spec.m_FadePower);
	}

	// 逐点噪声：让每一层都有自己的粗细节奏，观感上就是撕裂的火焰 / 烟絮边缘。
	void BuildRaggedNoise(size_t Count, unsigned Seed, float MinScale, float MaxScale, std::vector<float> &vOut)
	{
		vOut.resize(Count);
		for(size_t i = 0; i < Count; ++i)
			vOut[i] = mix(MinScale, MaxScale, Hash01(Seed + (unsigned)i * 2654435761u));
	}

	// 随年龄的世界坐标上浮：越老的采样点飘得越高并轻微摆动。
	// 水平跑动的 Tee 拖尾如果没有这一项，就只是几条平行色带。
	void BuildRiseDrift(const std::vector<SSample> &vSamples, float RisePerSec, float SwayPerSec, float Phase, std::vector<vec2> &vDrift)
	{
		vDrift.resize(vSamples.size());
		for(size_t i = 0; i < vSamples.size(); ++i)
		{
			const SSample &Sample = vSamples[i];
			const float AgeSec = Sample.m_Age * TICK_SECONDS;
			vDrift[i] = vec2(
				std::sin(AgeSec * 2.1f + Phase + Sample.m_S * 2.6f) * AgeSec * SwayPerSec,
				-AgeSec * RisePerSec);
		}
	}

	void PushQuad(std::vector<qm_tee_trail::SQuad> &vOut, const vec2 &P0, const vec2 &P1, const vec2 &P2, const vec2 &P3,
		ColorRGBA C0, ColorRGBA C1, ColorRGBA C2, ColorRGBA C3, float MaxAlpha)
	{
		if(vOut.size() >= qm_tee_trail::MAX_QUADS)
			return;
		C0.a = std::clamp(C0.a, 0.0f, MaxAlpha);
		C1.a = std::clamp(C1.a, 0.0f, MaxAlpha);
		C2.a = std::clamp(C2.a, 0.0f, MaxAlpha);
		C3.a = std::clamp(C3.a, 0.0f, MaxAlpha);
		if(C0.a <= MIN_QUAD_ALPHA && C1.a <= MIN_QUAD_ALPHA && C2.a <= MIN_QUAD_ALPHA && C3.a <= MIN_QUAD_ALPHA)
			return;

		qm_tee_trail::SQuad Quad;
		Quad.m_aPos[0] = P0;
		Quad.m_aPos[1] = P1;
		Quad.m_aPos[2] = P2;
		Quad.m_aPos[3] = P3;
		Quad.m_aColor[0] = C0;
		Quad.m_aColor[1] = C1;
		Quad.m_aColor[2] = C2;
		Quad.m_aColor[3] = C3;
		vOut.push_back(Quad);
	}

	// 单段带状四边形；相邻段各自用本段方向取法线，转角处只轻微收窄，不会出现退化四边形。
	void EmitPathSegment(std::vector<qm_tee_trail::SQuad> &vOut, const SPathPoint &A, const SPathPoint &B, float MaxAlpha)
	{
		const vec2 Segment = B.m_Pos - A.m_Pos;
		if(length(Segment) < 0.01f)
			return;
		const vec2 Direction = normalize(Segment);
		const vec2 Normal(-Direction.y, Direction.x);
		PushQuad(vOut, A.m_Pos + Normal * A.m_HalfWidth, B.m_Pos + Normal * B.m_HalfWidth,
			B.m_Pos - Normal * B.m_HalfWidth, A.m_Pos - Normal * A.m_HalfWidth,
			A.m_Col, B.m_Col, B.m_Col, A.m_Col, MaxAlpha);
	}

	void EmitPath(std::vector<qm_tee_trail::SQuad> &vOut, const std::vector<SPathPoint> &vPath, float MaxAlpha)
	{
		for(size_t i = 0; i + 1 < vPath.size(); ++i)
			EmitPathSegment(vOut, vPath[i], vPath[i + 1], MaxAlpha);
	}

	// 采样点 -> 带状路径：按弧长在首尾颜色间过渡，透明度叠加年龄衰减、输入透明度与末端软收。
	void BuildStrandPath(const std::vector<SSample> &vSamples, const std::vector<float> &vOffset,
		float HeadScale, float TailScale, float AlphaScale, const ColorRGBA &Head, const ColorRGBA &Tail,
		const SStyleSpec &Spec, float MaxAlpha, std::vector<SPathPoint> &vPath, const SStrandJitter &Jitter)
	{
		vPath.clear();
		vPath.reserve(vSamples.size() / (size_t)std::max(Jitter.m_SampleStep, 1) + 1);
		for(size_t i = 0; i < vSamples.size(); i += (size_t)std::max(Jitter.m_SampleStep, 1))
		{
			const SSample &Sample = vSamples[i];
			const float WidthScale = Jitter.m_pWidthScale != nullptr ? (*Jitter.m_pWidthScale)[i] : 1.0f;
			const float AlphaJitter = Jitter.m_pAlphaScale != nullptr ? (*Jitter.m_pAlphaScale)[i] : 1.0f;
			SPathPoint Point;
			Point.m_Pos = Sample.m_Pos + Sample.m_Normal * vOffset[i];
			if(Jitter.m_pDrift != nullptr)
				Point.m_Pos += (*Jitter.m_pDrift)[i];
			Point.m_HalfWidth = Sample.m_Width * mix(HeadScale, TailScale, Sample.m_S) * WidthScale;
			ColorRGBA Color = MixColor(Head, Tail, Sample.m_S);
			Color.a *= AlphaScale * AlphaJitter * TailEnvelope(Sample.m_S) * Fade(Sample.m_Age, Spec) * Sample.m_RelAlpha * MaxAlpha;
			Point.m_Col = Color;
			vPath.push_back(Point);
		}
	}

	void EmitStrand(std::vector<qm_tee_trail::SQuad> &vOut, const std::vector<SSample> &vSamples, const std::vector<float> &vOffset,
		float HeadScale, float TailScale, float AlphaScale, const ColorRGBA &Head, const ColorRGBA &Tail,
		const SStyleSpec &Spec, float MaxAlpha, const SStrandJitter &Jitter = {})
	{
		static std::vector<SPathPoint> s_vPath;
		BuildStrandPath(vSamples, vOffset, HeadScale, TailScale, AlphaScale, Head, Tail, Spec, MaxAlpha, s_vPath, Jitter);
		EmitPath(vOut, s_vPath, MaxAlpha);
	}

	// 断裂闪电：与 EmitStrand 同形，但按「每 2 段一组」的时间相位错开通断。
	void EmitBrokenStrand(std::vector<qm_tee_trail::SQuad> &vOut, const std::vector<SSample> &vSamples, const std::vector<float> &vOffset,
		float HeadScale, float TailScale, float AlphaScale, const ColorRGBA &Head, const ColorRGBA &Tail,
		const SStyleSpec &Spec, float MaxAlpha, float TimeSec, unsigned Seed, float Density, const SStrandJitter &Jitter = {})
	{
		static std::vector<SPathPoint> s_vPath;
		BuildStrandPath(vSamples, vOffset, HeadScale, TailScale, AlphaScale, Head, Tail, Spec, MaxAlpha, s_vPath, Jitter);
		for(size_t i = 0; i + 1 < s_vPath.size(); ++i)
		{
			const unsigned Group = (unsigned)(i / 2);
			const float Cycle = Fract(TimeSec * (1.7f + Hash01(Seed + Group * 131u) * 2.3f) + Hash01(Seed * 31u + Group));
			if(Cycle > Density)
				continue;
			EmitPathSegment(vOut, s_vPath[i], s_vPath[i + 1], MaxAlpha);
		}
	}

	// 沿指定方向的一小段亮条：电火花、碎片这类细长元素。
	void EmitStreak(std::vector<qm_tee_trail::SQuad> &vOut, const vec2 &From, const vec2 &To, float HalfWidth, const ColorRGBA &Color, float MaxAlpha)
	{
		static std::vector<SPathPoint> s_vPath;
		s_vPath.clear();
		s_vPath.push_back({From, HalfWidth, Color});
		s_vPath.push_back({To, HalfWidth * 0.55f, Color});
		EmitPath(vOut, s_vPath, MaxAlpha);
	}

	// 带旋转的小方块：光点、金尘、碎片。
	void EmitDot(std::vector<qm_tee_trail::SQuad> &vOut, const vec2 &Center, float HalfSize, float Angle, const ColorRGBA &Color, float MaxAlpha)
	{
		const vec2 AxisX(std::cos(Angle) * HalfSize, std::sin(Angle) * HalfSize);
		const vec2 AxisY(-AxisX.y, AxisX.x);
		PushQuad(vOut, Center - AxisX - AxisY, Center + AxisX - AxisY, Center + AxisX + AxisY, Center - AxisX + AxisY,
			Color, Color, Color, Color, MaxAlpha);
	}

	// 全幅横条：横跨整个轨迹宽度的一道条带，用「短边 + 渐变」在视觉上顶替贴图光晕。
	// Core 占中间 CoreSpan 的比例（端点最亮，形成细亮中轴），Beyond 是条带向外延伸的长度系数。
	// 用于等离子刀光、金丝脉冲这类「一条亮线贯穿」的元素。
	void EmitBar(std::vector<qm_tee_trail::SQuad> &vOut, const vec2 &Center, const vec2 &Tangent, float Span, float HalfWidth,
		const ColorRGBA &Color, float EdgeFalloff, float CoreSpan, float Beyond, float MaxAlpha)
	{
		if(length(Tangent) < 0.001f || Span <= 0.01f || HalfWidth <= 0.01f)
			return;
		const vec2 Along = normalize(Tangent);
		const vec2 Across(-Along.y, Along.x);
		const float Half = Span * 0.5f * Beyond;
		const float Core = std::clamp(CoreSpan, 0.0f, 1.0f);
		const float Middle = (1.0f - Core) * 0.5f;
		ColorRGBA Edge = Color;
		Edge.a *= EdgeFalloff;
		PushQuad(vOut, Center + Across * Half, Center + Across * (Half * Core),
			Center - Across * (Half * Core), Center - Across * Half,
			Edge, Color, Color, Edge, MaxAlpha);
		if(Middle > 0.0f)
		{
			ColorRGBA Tip = Color;
			Tip.a *= EdgeFalloff * EdgeFalloff;
			const float MidHalf = Half * (Core + Middle);
			PushQuad(vOut, Center + Across * MidHalf, Center + Across * (Half * Core),
				Center - Across * (Half * Core), Center - Across * MidHalf,
				Tip, Edge, Edge, Tip, MaxAlpha);
		}
		// 纵向只是一小段：靠上下两端的透明度衰减把条带两端磨圆。
		ColorRGBA FadeEnd = Color;
		FadeEnd.a *= EdgeFalloff;
		const vec2 Fore = Center + Along * HalfWidth;
		const vec2 Aft = Center - Along * HalfWidth;
		PushQuad(vOut, Aft + Across * Half, Fore + Across * Half, Fore + Across * (Half * Core), Aft + Across * (Half * Core),
			FadeEnd, FadeEnd, Color, Color, MaxAlpha);
		PushQuad(vOut, Fore - Across * Half, Aft - Across * Half, Aft - Across * (Half * Core), Fore - Across * (Half * Core),
			FadeEnd, FadeEnd, Color, Color, MaxAlpha);
	}

	// 爆裂环：以 Center 为圆心的环带，逐边透明度按「到环半径的距离」衰减。
	// 用于黑闪头部的短促爆闪——比实心圆更像冲击波。
	void EmitRing(std::vector<qm_tee_trail::SQuad> &vOut, const vec2 &Center, float Radius, float Thickness,
		const ColorRGBA &Color, float MaxAlpha, int Sides = 12)
	{
		if(Radius <= 0.05f || Thickness <= 0.01f || Color.a <= MIN_QUAD_ALPHA)
			return;
		static std::vector<SPathPoint> s_vPath;
		s_vPath.clear();
		s_vPath.reserve((size_t)Sides + 1);
		for(int i = 0; i <= Sides; ++i)
		{
			const float Angle = (float)i / (float)Sides * 2.0f * pi;
			s_vPath.push_back({Center + vec2(std::cos(Angle), std::sin(Angle)) * Radius, Thickness, Color});
		}
		EmitPath(vOut, s_vPath, MaxAlpha);
	}

	// 放射尖刺：从 Origin 向 Dir 方向甩出的细长条，用于爆闪的放射线与破碎感。
	void EmitSpike(std::vector<qm_tee_trail::SQuad> &vOut, const vec2 &Origin, const vec2 &Dir, float Length, float HalfWidth,
		const ColorRGBA &Color, float MaxAlpha)
	{
		if(length(Dir) < 0.001f || Length <= 0.05f)
			return;
		const vec2 Direction = normalize(Dir);
		const vec2 Perp(-Direction.y, Direction.x);
		ColorRGBA Base = Color;
		Base.a *= 0.15f;
		PushQuad(vOut, Origin + Perp * HalfWidth, Origin + Direction * Length + Perp * HalfWidth * 0.15f,
			Origin + Direction * Length - Perp * HalfWidth * 0.15f, Origin - Perp * HalfWidth,
			Base, Color, Color, Base, MaxAlpha);
	}

	// 分叉电弧：3 段折线，先铺一层描边色再压一层核心色，得到「黑芯 + 亮边」的观感。
	void EmitArc(std::vector<qm_tee_trail::SQuad> &vOut, const vec2 &Origin, const vec2 &Direction, float Length, float HalfWidth,
		const ColorRGBA &Core, const ColorRGBA &Rim, float AlphaScale, const SStyleSpec &Spec, float Age, float MaxAlpha, unsigned Seed)
	{
		constexpr int SEGMENTS = 3;
		const vec2 Perp(-Direction.y, Direction.x);
		static std::vector<SPathPoint> s_vPath;
		const float BaseAlpha = AlphaScale * Fade(Age, Spec) * MaxAlpha;
		vec2 Prev = Origin;
		for(int Segment = 1; Segment <= SEGMENTS; ++Segment)
		{
			const float T = (float)Segment / (float)SEGMENTS;
			const vec2 Cur = Origin + Direction * (Length * T) + Perp * (HashSigned(Seed + (unsigned)Segment * 977u) * Length * 0.30f * T);
			const float Narrow = 1.0f - T * 0.55f;
			const float FadeAlpha = BaseAlpha * (1.0f - T * 0.5f);

			s_vPath.clear();
			s_vPath.push_back({Prev, HalfWidth * 1.9f * Narrow, Rim.WithMultipliedAlpha(FadeAlpha)});
			s_vPath.push_back({Cur, HalfWidth * 1.7f * Narrow, Rim.WithMultipliedAlpha(FadeAlpha * 0.75f)});
			EmitPath(vOut, s_vPath, MaxAlpha);

			s_vPath[0].m_HalfWidth = HalfWidth * 0.8f * Narrow;
			s_vPath[1].m_HalfWidth = HalfWidth * 0.7f * Narrow;
			s_vPath[0].m_Col = Core.WithMultipliedAlpha(FadeAlpha);
			s_vPath[1].m_Col = Core.WithMultipliedAlpha(FadeAlpha * 0.75f);
			EmitPath(vOut, s_vPath, MaxAlpha);

			Prev = Cur;
		}
	}

	// 断裂折线闪电：从 Origin 向 Dir 甩出 Steps 段折线，逐段在垂直方向做硬折角偏移。
	// 与 EmitArc（3 段定长分叉）不同，这里的长度、折角幅度、每段宽度都可调，
	// 用来画雷之呼吸那种「一道主雷贯穿、乱枝四散」的雷纹。
	void EmitLightningPath(std::vector<qm_tee_trail::SQuad> &vOut, const vec2 &Origin, const vec2 &Dir,
		float Length, float HalfWidth, int Steps, float Jag, unsigned Seed, const ColorRGBA &Color, float MaxAlpha)
	{
		if(length(Dir) < 0.001f || Length <= 0.05f || Steps < 1 || Color.a <= MIN_QUAD_ALPHA)
			return;
		const vec2 Direction = normalize(Dir);
		const vec2 Perp(-Direction.y, Direction.x);
		static std::vector<SPathPoint> s_vBolt;
		s_vBolt.clear();
		s_vBolt.reserve((size_t)Steps + 1);
		for(int Step = 0; Step <= Steps; ++Step)
		{
			const float T = (float)Step / (float)Steps;
			const float Side = HashSigned(Seed + (unsigned)Step * 7919u) * Jag * Length * T;
			ColorRGBA Node = Color;
			Node.a *= 1.0f - 0.70f * T;
			s_vBolt.push_back({Origin + Direction * (Length * T) + Perp * Side, HalfWidth * (1.0f - 0.55f * T), Node});
		}
		EmitPath(vOut, s_vBolt, MaxAlpha);
	}

	// 斩击：一条横跨轨迹的细亮斩线（沿轨迹推进的「一太刀」），
	// 以及斩线扫过之后留在轨迹上的刀痕。这是雷之呼吸·火雷神最核心的一笔。
	void EmitLightningSlash(std::vector<qm_tee_trail::SQuad> &vOut, const std::vector<SSample> &vSamples,
		const SStyleSpec &Spec, float CurTime, float SeedPhase, const SPalette &Palette, float MaxAlpha)
	{
		const size_t Count = vSamples.size();
		// 采样点只带「年龄」，所以沿轨迹的亮度比例统一以头部年龄为基准归一化：
		// 头部恒为 1，越靠尾端越老、越暗。
		const float HeadFade = std::max(Fade(vSamples[0].m_Age, Spec), 0.02f);

		// 1) 主斩线：相位 0..1 沿轨迹从头部推到尾端，再各自淡出。
		const float Phase = Fract(CurTime * 0.55f + SeedPhase);
		const size_t CutIndex = std::min<size_t>(1 + (size_t)(Phase * (float)(Count - 2)), Count - 2);
		const SSample &Cut = vSamples[CutIndex];
		const float CutGain = std::sin(Phase * pi);
		ColorRGBA CutColor = MixColor(Palette.m_Core, Palette.m_Accent, 0.30f);
		CutColor.a = 0.95f * CutGain * Fade(Cut.m_Age, Spec) / HeadFade * Cut.m_RelAlpha;
		// 斩线横跨轨迹宽度，纵向只有极薄一段——视觉上是「一条线」而不是「一条带」。
		EmitBar(vOut, Cut.m_Pos, Cut.m_Tangent, Cut.m_Width * 4.4f, Cut.m_Width * 0.12f,
			CutColor, 0.06f, 0.55f, 1.0f, MaxAlpha);

		// 2) 刀痕：斩线扫过之后，在身后留下一道正在淡出的细痕。
		static std::vector<SPathPoint> s_vSlash;
		s_vSlash.clear();
		for(size_t i = CutIndex; i < Count; ++i)
		{
			const SSample &Sample = vSamples[i];
			SPathPoint Point;
			Point.m_Pos = Sample.m_Pos;
			Point.m_HalfWidth = Sample.m_Width * 0.11f;
			Point.m_Col = Palette.m_Core.WithAlpha(0.75f * Fade(Sample.m_Age, Spec) / HeadFade * Sample.m_RelAlpha);
			s_vSlash.push_back(Point);
		}
		EmitPath(vOut, s_vSlash, MaxAlpha);
	}

	// 把拖尾采样点重采样成等距网格，并算出朝向、年龄与相对透明度。
	bool BuildSamples(const std::vector<CTrailPart> &vTrail, const SStyleSpec &Spec, float CurTime, float FallbackHalfWidth,
		std::vector<SSample> &vSamples, float &MaxAlpha, ColorRGBA &AverageTint)
	{
		vSamples.clear();
		MaxAlpha = 0.0f;
		AverageTint = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
		if(vTrail.size() < 3)
			return false;

		float TintR = 0.0f;
		float TintG = 0.0f;
		float TintB = 0.0f;
		float WeightSum = 0.0f;
		for(const CTrailPart &Part : vTrail)
		{
			const float Weight = std::max(Part.m_Col.a, 0.05f);
			TintR += Part.m_Col.r * Weight;
			TintG += Part.m_Col.g * Weight;
			TintB += Part.m_Col.b * Weight;
			WeightSum += Weight;
			MaxAlpha = std::max(MaxAlpha, Part.m_Col.a);
		}
		if(MaxAlpha <= MIN_VISIBLE_ALPHA || WeightSum <= 0.0f)
			return false;
		AverageTint = ColorRGBA(TintR / WeightSum, TintG / WeightSum, TintB / WeightSum, 1.0f);

		// 新采样点都超过寿命就直接不画：停下后尾迹整体消失的收尾条件。
		if(CurTime - (float)vTrail[0].m_Tick >= Spec.m_Life)
			return false;

		float TotalLength = 0.0f;
		for(size_t i = 0; i + 1 < vTrail.size(); ++i)
		{
			const float Segment = distance(vTrail[i].m_Pos, vTrail[i + 1].m_Pos);
			// 传送：整条不画，否则会拉出一条横贯地图的长条。
			if(Segment > MAX_SEGMENT_DIST)
				return false;
			TotalLength += Segment;
		}
		// 原地不动：没有可依附的轨迹，不生成特效。
		if(TotalLength < MIN_TRAIL_EXTENT)
			return false;

		// 采样间距按整条拖尾长度自适应，只保底不封顶：
		// 封顶会让长拖尾只画出一小段，长度设置形同虚设。
		const float Spacing = std::max(TotalLength / (float)(MAX_SAMPLES - 1), MIN_SAMPLE_SPACING);
		float SegmentStart = 0.0f;
		float SegmentLength = distance(vTrail[0].m_Pos, vTrail[1].m_Pos);
		size_t Segment = 0;
		for(float Target = 0.0f; (int)vSamples.size() < MAX_SAMPLES; Target += Spacing)
		{
			while(Target > SegmentStart + SegmentLength && Segment + 2 < vTrail.size())
			{
				SegmentStart += SegmentLength;
				++Segment;
				SegmentLength = distance(vTrail[Segment].m_Pos, vTrail[Segment + 1].m_Pos);
			}
			if(SegmentLength <= 0.0f || Target > TotalLength)
				break;

			const float Amount = std::clamp((Target - SegmentStart) / SegmentLength, 0.0f, 1.0f);
			const CTrailPart &Head = vTrail[Segment];
			const CTrailPart &Tail = vTrail[Segment + 1];
			const float InterpolatedWidth = mix(Head.m_Width, Tail.m_Width, Amount);
			SSample Sample;
			Sample.m_Pos = mix(Head.m_Pos, Tail.m_Pos, Amount);
			Sample.m_Age = std::max(0.0f, CurTime - mix((float)Head.m_Tick, (float)Tail.m_Tick, Amount));
			Sample.m_Width = std::max(InterpolatedWidth > 0.05f ? InterpolatedWidth : FallbackHalfWidth, 0.05f);
			Sample.m_S = std::clamp(Target / TotalLength, 0.0f, 1.0f);
			Sample.m_RelAlpha = std::clamp(mix(Head.m_Col.a, Tail.m_Col.a, Amount) / MaxAlpha, 0.0f, 1.0f);
			vSamples.push_back(Sample);
		}

		if(vSamples.size() < 3)
			return false;

		for(size_t i = 0; i < vSamples.size(); ++i)
		{
			const vec2 Pos = vSamples[i].m_Pos;
			const vec2 PrevPos = i == 0 ? Pos + (Pos - vSamples[i + 1].m_Pos) : vSamples[i - 1].m_Pos;
			const vec2 NextPos = i + 1 == vSamples.size() ? Pos + (Pos - vSamples[i - 1].m_Pos) : vSamples[i + 1].m_Pos;
			vec2 Direction = NextPos - PrevPos;
			if(length(Direction) < 0.001f)
				Direction = vec2(1.0f, 0.0f);
			vSamples[i].m_Tangent = normalize(Direction);
			vSamples[i].m_Normal = vec2(-vSamples[i].m_Tangent.y, vSamples[i].m_Tangent.x);
		}
		return true;
	}

	// 从拖尾颜色派生一套配色：用户选择「使用现有颜色模式」时走这里。
	// 注意：只有配色被替换，形态、分层、运动完全不受影响。
	SPalette PaletteFromTint(const ColorRGBA &Tint, int Style)
	{
		const ColorRGBA Black(0.0f, 0.0f, 0.0f, 1.0f);
		const ColorRGBA White(1.0f, 1.0f, 1.0f, 1.0f);
		const ColorRGBA Base = Tint.WithAlpha(1.0f);
		SPalette Palette;
		Palette.m_Main = Base;
		Palette.m_Glow = MixColor(Base, White, 0.12f);
		// 虚空·暗影靠「暗主体 + 亮描边」成立，因此核心仍然压到近黑，亮边改由描边槽承担。
		const bool VoidLike = Style == qm_tee_trail::STYLE_VOID_SHADOW;
		Palette.m_Core = MixColor(Base, VoidLike ? Black : White, VoidLike ? 0.88f : 0.72f);
		Palette.m_Accent = MixColor(Base, White, VoidLike ? 0.60f : 0.45f);
		Palette.m_Deep = MixColor(Base, Black, 0.55f);
		return Palette;
	}

	// ---------------------------------------------------------------------------
	// 咒焰·黑闪（《咒术回战》黑闪）
	// ---------------------------------------------------------------------------
	// 形态语言：**横向扇开的翻卷火舌** + 头部爆裂 + 分叉电弧。
	// 与其他样式的关键差别：火舌沿法线扇开并随年龄上浮翻卷，越靠尾端张角越大；
	// 能量沿轨迹快速衰减，整条只在 Tee 身后一段炸开，然后碎裂消散。
	void EmitCursedFlame(std::vector<qm_tee_trail::SQuad> &vOut, const std::vector<SSample> &vSamples,
		const SPalette &Palette, const SStyleSpec &Spec, float TimeSec, unsigned Seed, float MaxAlpha)
	{
		const size_t Count = vSamples.size();
		const ColorRGBA White(1.0f, 1.0f, 1.0f, 1.0f);
		const SSample &Head = vSamples[0];
		const float HeadWidth = std::max(Head.m_Width, 0.5f);
		static std::vector<float> s_vOffset;
		static std::vector<float> s_vRagged;
		static std::vector<vec2> s_vDrift;
		static std::vector<vec2> s_vCoreDrift;
		s_vOffset.assign(Count, 0.0f);

		// 1) 咒力外放：两圈蓝青柔光铺底（外圈隔点取样），让火舌有可依附的底色。
		{
			SStrandJitter Halo;
			Halo.m_SampleStep = 2;
			EmitStrand(vOut, vSamples, s_vOffset, 2.05f, 1.25f, 1.0f,
				MixColor(Palette.m_Glow, Palette.m_Deep, 0.35f).WithAlpha(0.22f), Palette.m_Deep.WithAlpha(0.02f), Spec, MaxAlpha, Halo);
			EmitStrand(vOut, vSamples, s_vOffset, 1.45f, 0.70f, 1.0f,
				Palette.m_Glow.WithAlpha(0.38f), Palette.m_Deep.WithAlpha(0.03f), Spec, MaxAlpha);
		}

		// 2) 三股扇开的火舌：向同一侧扇出（不是对称摆动），越靠尾端张角越大并向上翻卷。
		const ColorRGBA TongueHead = MixColor(Palette.m_Glow, White, 0.45f);
		for(int Tongue = 0; Tongue < 3; ++Tongue)
		{
			const float Phase = (float)Tongue * 2.2f;
			const float FanDirection = Tongue == 2 ? -1.0f : 1.0f;
			const float FanAmount = 1.0f + 0.75f * (float)Tongue;
			BuildRiseDrift(vSamples, 20.0f + 8.0f * (float)Tongue, 12.0f, Phase, s_vDrift);
			BuildRaggedNoise(Count, Seed ^ (0x9e3779b9u + (unsigned)Tongue * 0x85ebca6bu), 0.60f, 1.35f, s_vRagged);
			for(size_t i = 0; i < Count; ++i)
			{
				const SSample &Sample = vSamples[i];
				// 张角随 S 增长 = 火焰在 Tee 身后越远越散；再叠正弦翻卷与逐点噪声。
				const float Spread = Sample.m_Width * (0.55f + 0.85f * Sample.m_S) * FanAmount;
				const float Wave = std::sin(Sample.m_S * pi * 3.1f + TimeSec * 3.6f + Phase);
				const float Ragged = (s_vRagged[i] - 1.0f) * 0.55f;
				s_vOffset[i] = FanDirection * Spread + Wave * Sample.m_Width * 0.75f + Ragged * Sample.m_Width;
			}
			SStrandJitter Jitter;
			Jitter.m_pWidthScale = &s_vRagged;
			Jitter.m_pDrift = &s_vDrift;
			EmitStrand(vOut, vSamples, s_vOffset, 0.70f + 0.10f * (float)Tongue, 0.30f, 0.95f,
				TongueHead.WithAlpha(0.55f), Palette.m_Deep.WithAlpha(0.05f), Spec, MaxAlpha, Jitter);
		}

		// 3) 猩红描边压在外缘，再由黑色核心盖住内侧，只留一条锐利暗红边。
		BuildRiseDrift(vSamples, 15.0f, 9.0f, 0.7f, s_vCoreDrift);
		BuildRaggedNoise(Count, Seed ^ 0x1b873593u, 0.80f, 1.20f, s_vRagged);
		for(int Side = -1; Side <= 1; Side += 2)
		{
			for(size_t i = 0; i < Count; ++i)
			{
				const SSample &Sample = vSamples[i];
				s_vOffset[i] = (float)Side * Sample.m_Width * (0.24f + 0.75f * Sample.m_S) * s_vRagged[i];
			}
			SStrandJitter Jitter;
			Jitter.m_pDrift = &s_vCoreDrift;
			EmitStrand(vOut, vSamples, s_vOffset, 0.14f, 0.03f, 1.0f,
				Palette.m_Accent.WithAlpha(0.75f), Palette.m_Accent.WithAlpha(0.04f), Spec, MaxAlpha, Jitter);
		}

		// 4) 黑闪核心：窄而实的近黑中轴，跟着火舌一起上浮（能量收束在轨迹中轴上）。
		std::fill(s_vOffset.begin(), s_vOffset.end(), 0.0f);
		{
			SStrandJitter Jitter;
			Jitter.m_pDrift = &s_vCoreDrift;
			EmitStrand(vOut, vSamples, s_vOffset, 0.36f, 0.06f, 1.0f,
				Palette.m_Core.WithAlpha(0.95f), Palette.m_Core.WithAlpha(0.06f), Spec, MaxAlpha, Jitter);
		}

		// 5) 头部爆裂：以当前头部位置为中心的冲击环 + 放射尖刺，随时间脉动。
		//    这是黑闪「短促爆发」的主视觉，只出现在 Tee 所在处。
		{
			const float Pulse = 0.55f + 0.45f * std::sin(TimeSec * 8.0f);
			const float RingRadius = HeadWidth * (1.15f + 0.85f * Pulse);
			EmitRing(vOut, Head.m_Pos, RingRadius, HeadWidth * 0.14f,
				MixColor(Palette.m_Glow, White, 0.55f).WithAlpha(0.80f * Pulse * Fade(Head.m_Age, Spec) * Head.m_RelAlpha), MaxAlpha, 14);
			EmitRing(vOut, Head.m_Pos, RingRadius * 1.45f, HeadWidth * 0.08f,
				Palette.m_Glow.WithAlpha(0.42f * (1.0f - Pulse) * Fade(Head.m_Age, Spec) * Head.m_RelAlpha), MaxAlpha, 14);
			for(int Spike = 0; Spike < 5; ++Spike)
			{
				const unsigned SpikeSeed = Seed * 0x9e3779b9u + (unsigned)Spike * 0x27d4eb2du + 71u;
				const float Angle = Hash01(SpikeSeed) * 2.0f * pi;
				const float Length = HeadWidth * (0.75f + 0.95f * Hash01(SpikeSeed + 3u)) * (0.75f + 0.35f * Pulse);
				const ColorRGBA Color = (Spike % 2 == 0 ? Palette.m_Accent : MixColor(Palette.m_Glow, White, 0.5f))
								.WithAlpha(0.85f * Pulse * Fade(Head.m_Age, Spec) * Head.m_RelAlpha);
				EmitSpike(vOut, Head.m_Pos, vec2(std::cos(Angle), std::sin(Angle)), Length, HeadWidth * 0.09f, Color, MaxAlpha);
			}
		}

		// 6) 分叉电弧：黑芯 + 猩红描边，从核心甩出去，年龄越大越短越淡。
		for(int Fork = 0; Fork < 3; ++Fork)
		{
			const unsigned ForkSeed = Seed * 0x9e3779b9u + (unsigned)Fork * 0x85ebca6bu + 17u;
			const size_t Index = std::min<size_t>(1 + (size_t)(Hash01(ForkSeed) * (float)(Count - 2)), Count - 2);
			const SSample &Sample = vSamples[Index];
			const float Length = Sample.m_Width * (1.8f + 1.6f * Hash01(ForkSeed + 5u));
			const vec2 Side = Hash01(ForkSeed + 9u) < 0.5f ? -Sample.m_Normal : Sample.m_Normal;
			const vec2 Direction = normalize(Side + Sample.m_Tangent * (HashSigned(ForkSeed + 13u) * 0.9f));
			EmitArc(vOut, Sample.m_Pos + s_vCoreDrift[Index], Direction, Length, Sample.m_Width * 0.13f,
				Palette.m_Core.WithAlpha(1.0f), Palette.m_Accent.WithAlpha(0.95f), 0.85f, Spec, Sample.m_Age, MaxAlpha, ForkSeed);
		}

		// 7) 飞散火星：向上飘散的蓝青与猩红余烬，越老越高越淡（碎裂消散）。
		for(int Ember = 0; Ember < 8; ++Ember)
		{
			const unsigned EmberSeed = Seed * 0x27d4eb2du + (unsigned)Ember * 0x9e3779b9u + 101u;
			const size_t Index = std::min<size_t>((size_t)(Hash01(EmberSeed) * (float)(Count - 1)), Count - 1);
			const SSample &Sample = vSamples[Index];
			const float AgeSec = Sample.m_Age * TICK_SECONDS;
			const vec2 Pos = Sample.m_Pos + Sample.m_Normal * (HashSigned(EmberSeed + 5u) * Sample.m_Width * (0.7f + AgeSec * 7.0f)) +
					 vec2(std::sin(TimeSec * 2.0f + (float)Ember) * 2.5f, -AgeSec * (20.0f + 14.0f * Hash01(EmberSeed + 3u)));
			const float Falloff = HeadEnvelope(Sample.m_S, Spec.m_Reach);
			const float Size = Sample.m_Width * (0.05f + 0.06f * Hash01(EmberSeed + 7u)) * (1.0f + AgeSec * 0.5f);
			const ColorRGBA Color = (Ember % 3 == 0 ? Palette.m_Accent : TongueHead)
							.WithAlpha(0.70f * Falloff * Fade(Sample.m_Age, Spec) * Sample.m_RelAlpha * MaxAlpha);
			EmitDot(vOut, Pos, Size, Hash01(EmberSeed + 11u) * pi, Color, MaxAlpha);
		}
	}

	// ---------------------------------------------------------------------------
	// 紫电·雷切（《鬼灭之刃》雷之呼吸·柒之型「火雷神」，紫色版）
	// ---------------------------------------------------------------------------
	// 形态语言：**一太刀决断的斩线** + 缠绕乱舞的紫电 + 落点雷环 + 溅射电火花。
	// 与其他样式的关键差别：主视觉是「斩」而不是「拖」——一道横跨轨迹的薄斩线沿轨迹推进，
	// 身后留下正在淡出的刀痕，斩线所过之处紫电缭绕；落点爆开雷环与雷棘。
	// 火雷神本是金黄的雷炎，这里按需求整体改成紫色，因此所有层都是紫 → 近白紫；
	// 不出现任何暖色，靠雷纹的硬折角与通断表现「雷」，而不是靠火舌。
	void EmitThunderBreath(std::vector<qm_tee_trail::SQuad> &vOut, const std::vector<SSample> &vSamples,
		const SPalette &Palette, const SStyleSpec &Spec, float TimeSec, unsigned Seed, float MaxAlpha)
	{
		const size_t Count = vSamples.size();
		const ColorRGBA White(1.0f, 1.0f, 1.0f, 1.0f);
		const SSample &Head = vSamples[0];
		const float HeadWidth = std::max(Head.m_Width, 0.5f);
		static std::vector<float> s_vOffset;
		static std::vector<float> s_vRagged;
		s_vOffset.assign(Count, 0.0f);
		BuildRaggedNoise(Count, Seed ^ 0x2545f491u, 0.70f, 1.28f, s_vRagged);

		// 1) 紫色柔光：两圈，外圈隔点取样。比别的样式收敛，把画面让给斩线与雷纹。
		{
			SStrandJitter Halo;
			Halo.m_SampleStep = 2;
			EmitStrand(vOut, vSamples, s_vOffset, 1.75f, 0.70f, 1.0f,
				Palette.m_Glow.WithAlpha(0.20f), Palette.m_Deep.WithAlpha(0.02f), Spec, MaxAlpha, Halo);
			EmitStrand(vOut, vSamples, s_vOffset, 1.15f, 0.40f, 1.0f,
				Palette.m_Glow.WithAlpha(0.34f), Palette.m_Deep.WithAlpha(0.03f), Spec, MaxAlpha);
		}

		// 2) 刀身细丝：沿轨迹收束的一线亮紫（刀走过的痕迹），比柔光窄得多。
		EmitStrand(vOut, vSamples, s_vOffset, 0.26f, 0.05f, 1.0f,
			MixColor(Palette.m_Core, Palette.m_Accent, 0.40f).WithAlpha(0.95f), Palette.m_Deep.WithAlpha(0.06f), Spec, MaxAlpha);

		// 3) 斩击：一道横跨轨迹的薄斩线沿轨迹推进 + 身后正在淡出的刀痕。
		EmitLightningSlash(vOut, vSamples, Spec, TimeSec, Hash01(Seed + 5u), Palette, MaxAlpha);

		// 4) 主雷：一道贯穿整条轨迹的折断闪电（这一刀劈出的那道主雷），
		//    与斩线错开相位流动，斩完之后紫电仍在轨迹上乱窜。
		{
			const float Flow = Fract(TimeSec * 0.9f + Hash01(Seed + 11u));
			const float Slope = (Flow - 0.5f) * 2.4f;
			for(size_t i = 0; i < Count; ++i)
			{
				const SSample &Sample = vSamples[i];
				const float FlowOffset = Sample.m_S * Slope + HashSigned(Seed + (unsigned)(i / 2) * 7919u) * 0.45f;
				const float TimeJitter = std::sin(TimeSec * 6.0f + (float)i * 0.9f) * 0.12f;
				s_vOffset[i] = (FlowOffset + TimeJitter) * Sample.m_Width;
			}
			SStrandJitter Jitter;
			Jitter.m_pWidthScale = &s_vRagged;
			EmitBrokenStrand(vOut, vSamples, s_vOffset, 0.14f, 0.03f, 1.0f,
				Palette.m_Main.WithAlpha(0.95f), Palette.m_Deep.WithAlpha(0.08f), Spec, MaxAlpha,
				TimeSec, Seed * 0x85ebca6bu + 3u, 0.55f, Jitter);
		}

		// 5) 乱枝紫电：从轨迹甩向两侧的折断闪电，长度、折角与闪烁各不相同。
		for(int Fork = 0; Fork < 4; ++Fork)
		{
			const unsigned ForkSeed = Seed * 0x9e3779b9u + (unsigned)Fork * 0x27d4eb2du + 37u;
			const size_t Index = std::min<size_t>(1 + (size_t)(Hash01(ForkSeed) * (float)(Count - 2)), Count - 2);
			const SSample &Sample = vSamples[Index];
			const float Side = Hash01(ForkSeed + 3u) < 0.5f ? -1.0f : 1.0f;
			const vec2 Direction = normalize(Sample.m_Normal * Side + Sample.m_Tangent * (HashSigned(ForkSeed + 7u) * 1.1f));
			const float Length = Sample.m_Width * (1.4f + 1.6f * Hash01(ForkSeed + 11u));
			const float Gain = 0.40f + 0.60f * Fract(TimeSec * (1.9f + 2.6f * Hash01(ForkSeed + 13u)) + Hash01(ForkSeed + 17u));
			const ColorRGBA Color = MixColor(Palette.m_Main, White, 0.25f)
							.WithAlpha(0.95f * Gain * Fade(Sample.m_Age, Spec) * Sample.m_RelAlpha);
			EmitLightningPath(vOut, Sample.m_Pos, Direction, Length, Sample.m_Width * 0.095f,
				3 + (int)(Hash01(ForkSeed + 19u) * 3.0f), 0.55f, ForkSeed, Color, MaxAlpha);
		}

		// 6) 落点雷环：这一刀落下处的雷环与雷棘，随时间脉动。
		EmitRing(vOut, Head.m_Pos, HeadWidth * 1.55f, HeadWidth * 0.16f,
			MixColor(Palette.m_Core, Palette.m_Accent, 0.30f).WithAlpha(0.75f * Fade(Head.m_Age, Spec) * Head.m_RelAlpha), MaxAlpha, 12);
		for(int Spike = 0; Spike < 4; ++Spike)
		{
			const unsigned SpikeSeed = Seed * 0x165667b1u + (unsigned)Spike * 0x85ebca6bu + 83u;
			const float Angle = Hash01(SpikeSeed) * 2.0f * pi;
			const float Length = HeadWidth * (0.85f + 1.05f * Hash01(SpikeSeed + 3u));
			const ColorRGBA Color = Palette.m_Accent.WithAlpha(0.80f * Fade(Head.m_Age, Spec) * Head.m_RelAlpha);
			EmitLightningPath(vOut, Head.m_Pos, vec2(std::cos(Angle), std::sin(Angle)), Length,
				HeadWidth * 0.07f, 3, 0.60f, SpikeSeed, Color, MaxAlpha);
		}

		// 7) 溅射电火花：沿法线方向甩出去的细亮条，亮度随时间闪烁。
		for(int Spark = 0; Spark < 12; ++Spark)
		{
			const unsigned SparkSeed = Seed * 0x27d4eb2du + (unsigned)Spark * 0x9e3779b9u + 29u;
			const size_t Index = std::min<size_t>((size_t)(Hash01(SparkSeed) * (float)(Count - 1)), Count - 1);
			const SSample &Sample = vSamples[Index];
			const float Flicker = 0.35f + 0.65f * Fract(TimeSec * (2.0f + Hash01(SparkSeed + 2u) * 3.0f) + Hash01(SparkSeed + 4u));
			const vec2 Offset = Sample.m_Normal * (HashSigned(SparkSeed + 6u) * Sample.m_Width * 1.6f) +
					    Sample.m_Tangent * (HashSigned(SparkSeed + 8u) * Sample.m_Width * 0.8f);
			const float HalfLength = Sample.m_Width * (0.10f + 0.14f * Hash01(SparkSeed + 10u));
			const vec2 Direction = normalize(Sample.m_Normal * HashSigned(SparkSeed + 12u) + Sample.m_Tangent * HashSigned(SparkSeed + 14u));
			const ColorRGBA Color = (Spark % 3 == 0 ? Palette.m_Core : Palette.m_Accent)
							.WithAlpha(0.95f * Flicker * Fade(Sample.m_Age, Spec) * Sample.m_RelAlpha * MaxAlpha);
			EmitStreak(vOut, Sample.m_Pos + Offset, Sample.m_Pos + Offset + Direction * HalfLength, Sample.m_Width * 0.04f, Color, MaxAlpha);
		}
	}

	// ---------------------------------------------------------------------------
	// 灵光·流萤（《奥日与萤火意志》）
	// ---------------------------------------------------------------------------
	// 形态语言：**多股正弦编织的柔软细丝** + 断续光缕 + 交叠柔光光团 + 缓慢漂浮的光点。
	// 与其他样式的关键差别：纵向流动的连续细丝，丝与丝之间会交叉（编织）；
	// 光点是这套样式的主角（做成圆形光团而不是小方块），整体最柔、最慢。
	void EmitSpiritLight(std::vector<qm_tee_trail::SQuad> &vOut, const std::vector<SSample> &vSamples,
		const SPalette &Palette, const SStyleSpec &Spec, float TimeSec, unsigned Seed, float MaxAlpha)
	{
		const size_t Count = vSamples.size();
		const ColorRGBA White(1.0f, 1.0f, 1.0f, 1.0f);
		static std::vector<float> s_vOffset;
		static std::vector<float> s_vRagged;
		static std::vector<vec2> s_vDrift;
		static std::vector<float> s_vThread[3];
		s_vOffset.assign(Count, 0.0f);
		BuildRaggedNoise(Count, Seed ^ 0x27d4eb2du, 0.72f, 1.15f, s_vRagged);

		// 1) 柔光底：一层很淡的宽光晕（不做三层，避免和黑闪的辉光铺底撞观感）。
		{
			SStrandJitter Halo;
			Halo.m_SampleStep = 2;
			EmitStrand(vOut, vSamples, s_vOffset, 2.15f, 1.00f, 1.0f,
				Palette.m_Glow.WithAlpha(0.13f), Palette.m_Deep.WithAlpha(0.02f), Spec, MaxAlpha, Halo);
		}

		// 2) 三股编织细丝：同频同幅、相位差 120°，因此会在轨迹上反复交叉（编织）。
		for(int Thread = 0; Thread < 3; ++Thread)
		{
			const float Phase = (float)Thread * (2.0f * pi / 3.0f);
			BuildRiseDrift(vSamples, 8.0f + 2.5f * (float)Thread, 6.0f, Phase, s_vDrift);
			s_vThread[Thread].assign(Count, 0.0f);
			for(size_t i = 0; i < Count; ++i)
			{
				const SSample &Sample = vSamples[i];
				s_vThread[Thread][i] = std::sin(Sample.m_S * pi * 2.6f - TimeSec * 1.1f + Phase) * Sample.m_Width * 0.78f;
			}
			s_vOffset = s_vThread[Thread];
			SStrandJitter Jitter;
			Jitter.m_pWidthScale = &s_vRagged;
			Jitter.m_pDrift = &s_vDrift;
			EmitStrand(vOut, vSamples, s_vOffset, 0.26f, 0.10f, 0.95f,
				MixColor(Palette.m_Main, White, 0.30f).WithAlpha(0.72f), Palette.m_Deep.WithAlpha(0.04f), Spec, MaxAlpha, Jitter);
		}

		// 3) 断续光缕：沿最外侧两股丝之间的中点，用「断点 + 明暗」堆出流光的颗粒感。
		{
			static std::vector<SPathPoint> s_vFilament;
			for(int Thread = 0; Thread < 3; ++Thread)
			{
				s_vFilament.clear();
				for(size_t i = 0; i < Count; ++i)
				{
					const SSample &Sample = vSamples[i];
					const float Dash = Hash01(Seed * 0x51ed270bu + (unsigned)i * 2654435761u + (unsigned)Thread * 7919u);
					if(Dash > 0.62f)
						continue;
					const float Bright = 0.55f + 0.45f * std::sin(TimeSec * 2.2f + (float)i * 0.55f + (float)Thread * 2.0f);
					s_vFilament.push_back({Sample.m_Pos + Sample.m_Normal * s_vThread[Thread][i],
						Sample.m_Width * 0.09f * (0.5f + Dash),
						Palette.m_Core.WithAlpha(0.85f * Bright * Fade(Sample.m_Age, Spec) * Sample.m_RelAlpha)});
				}
				EmitPath(vOut, s_vFilament, MaxAlpha);
			}
			// 中心一条连续亮丝，把编织的丝收成一股。
			std::fill(s_vOffset.begin(), s_vOffset.end(), 0.0f);
			EmitStrand(vOut, vSamples, s_vOffset, 0.13f, 0.04f, 1.0f,
				Palette.m_Core.WithAlpha(0.65f), Palette.m_Core.WithAlpha(0.05f), Spec, MaxAlpha);
		}

		// 4) 交叠柔光光团：沿丝线摆放的圆形光斑，半径随时间缓慢呼吸（像流萤在飘）。
		for(int Globe = 0; Globe < 7; ++Globe)
		{
			const unsigned GlobeSeed = Seed * 0x9e3779b9u + (unsigned)Globe * 0x27d4eb2du + 23u;
			const size_t Index = 1 + (size_t)(Hash01(GlobeSeed) * (float)(Count - 2));
			const SSample &Sample = vSamples[std::min<size_t>(Index, Count - 2)];
			const float Breath = 0.6f + 0.4f * std::sin(TimeSec * 1.5f + (float)Globe * 1.7f);
			const float Radius = Sample.m_Width * (0.30f + 0.26f * Hash01(GlobeSeed + 3u)) * (0.8f + 0.5f * Breath);
			const vec2 Pos = Sample.m_Pos + Sample.m_Normal * s_vThread[Globe % 3][std::min<size_t>(Index, Count - 1)];
			const ColorRGBA Color = MixColor(Palette.m_Glow, White, 0.35f)
							.WithAlpha(0.30f * Breath * Fade(Sample.m_Age, Spec) * Sample.m_RelAlpha);
			EmitRing(vOut, Pos, Radius, Radius * 0.20f, Color, MaxAlpha, 10);
			EmitDot(vOut, Pos, Radius * 0.22f, TimeSec * 0.6f + (float)Globe,
				Palette.m_Core.WithAlpha(0.55f * Breath * Fade(Sample.m_Age, Spec) * Sample.m_RelAlpha), MaxAlpha);
		}

		// 5) 漂浮光点：缓慢上升、向外漂散、逐渐变大变淡，是这套样式的消散方式。
		for(int Mote = 0; Mote < 12; ++Mote)
		{
			const unsigned MoteSeed = Seed * 0x9e3779b9u + (unsigned)Mote * 0x27d4eb2du + 23u;
			const size_t Index = std::min<size_t>((size_t)(Hash01(MoteSeed) * (float)(Count - 1)), Count - 1);
			const SSample &Sample = vSamples[Index];
			const float AgeSec = Sample.m_Age * TICK_SECONDS;
			const vec2 Drift = Sample.m_Normal * (HashSigned(MoteSeed + 3u) * Sample.m_Width * (0.8f + AgeSec * 12.0f)) +
					   vec2(std::sin(TimeSec * 1.4f + (float)Mote) * 2.0f, -AgeSec * (14.0f + 10.0f * Hash01(MoteSeed + 5u)));
			const float Radius = Sample.m_Width * (0.10f + 0.10f * Hash01(MoteSeed + 7u)) * (1.0f + AgeSec * 0.6f);
			const ColorRGBA Color = MixColor(Palette.m_Accent, White, 0.20f * Hash01(MoteSeed + 9u))
							.WithAlpha(0.80f * Fade(Sample.m_Age, Spec) * Sample.m_RelAlpha * MaxAlpha);
			// 圆形光团 + 中心亮点，低透明度下比方块更像「萤火」。
			EmitRing(vOut, Sample.m_Pos + Drift, Radius, Radius * 0.55f, Color.WithMultipliedAlpha(0.55f), MaxAlpha, 8);
			EmitDot(vOut, Sample.m_Pos + Drift, Radius * 0.26f, Hash01(MoteSeed + 11u) * pi, Color, MaxAlpha);
		}
	}

	// ---------------------------------------------------------------------------
	// 虚空·暗影（《空洞骑士》虚空）
	// ---------------------------------------------------------------------------
	// 形态语言：**墨黑实体烟身** + 灰白薄边 + 向外卷曲的触须 + 悬浮碎片。
	// 与其他样式的关键差别：这是唯一「暗主体 + 亮描边」的负空间造型，
	// 主体是实心墨黑（不靠辉光，靠灰白薄边把轮廓从背景里勾出来），
	// 头部厚、尾部散，触须先向外卷再收回，尾部崩成碎片。
	void EmitVoidShadow(std::vector<qm_tee_trail::SQuad> &vOut, const std::vector<SSample> &vSamples,
		const SPalette &Palette, const SStyleSpec &Spec, float TimeSec, unsigned Seed, float MaxAlpha)
	{
		const size_t Count = vSamples.size();
		static std::vector<float> s_vTendril;
		static std::vector<float> s_vMist;
		static std::vector<float> s_vEdge;
		static std::vector<float> s_vRagged;
		static std::vector<float> s_vOpacity;
		static std::vector<vec2> s_vDrift;
		s_vTendril.assign(Count, 0.0f);
		s_vEdge.assign(Count, 0.0f);
		BuildRaggedNoise(Count, Seed ^ 0x165667b1u, 0.70f, 1.30f, s_vRagged);
		BuildRaggedNoise(Count, Seed ^ 0x9e3779b9u, 0.75f, 1.00f, s_vOpacity);

		// 1) 扩散墨雾：向两侧越飘越开的暗色团块，用逐点噪声打散（不是一条光滑辉光带），
		//    越靠尾端越碎——这正是触须之外的「墨滴扩散」底子。
		{
			static std::vector<vec2> s_vMistDrift;
			BuildRiseDrift(vSamples, 10.0f, 7.0f, 2.4f, s_vMistDrift);
			s_vMist.assign(Count, 0.0f);
			for(size_t i = 0; i < Count; ++i)
			{
				const SSample &Sample = vSamples[i];
				const float Spread = std::sin(Sample.m_S * pi * 0.9f) * Sample.m_Width * 0.90f;
				s_vMist[i] = (Spread + (s_vRagged[i] - 1.0f) * Sample.m_Width * 0.55f) * (0.35f + 0.65f * Sample.m_S);
			}
			SStrandJitter Mist;
			Mist.m_SampleStep = 2;
			Mist.m_pAlphaScale = &s_vOpacity;
			Mist.m_pDrift = &s_vMistDrift;
			EmitStrand(vOut, vSamples, s_vMist, 1.65f, 2.30f, 1.0f,
				MixColor(Palette.m_Glow, Palette.m_Deep, 0.55f).WithAlpha(0.30f), Palette.m_Glow.WithAlpha(0.10f), Spec, MaxAlpha, Mist);
		}

		// 2) 墨黑触须：两股向两侧弯出去再往回收，并随年龄上浮成烟。
		//    这是「墨滴入水」的核心动作，也是这套样式最容易被认出来的形状。
		for(int Tendril = 0; Tendril < 2; ++Tendril)
		{
			const float Sign = Tendril == 0 ? 1.0f : -1.0f;
			BuildRiseDrift(vSamples, 18.0f, 11.0f, (float)Tendril * 1.7f, s_vDrift);
			for(size_t i = 0; i < Count; ++i)
			{
				const SSample &Sample = vSamples[i];
				// 卷曲：中段向外鼓出、尾端收回，形成触须的「弯」而不是直线偏移。
				const float Curl = std::sin(Sample.m_S * pi * 1.05f + TimeSec * 0.5f + (float)Tendril * 1.7f);
				const float Ragged = (s_vRagged[i] - 1.0f) * 0.40f;
				s_vTendril[i] = Sign * Sample.m_Width * (0.95f * Curl - 0.35f * Sample.m_S * Sample.m_S + Ragged * 0.5f);
			}
			SStrandJitter Jitter;
			Jitter.m_pWidthScale = &s_vRagged;
			Jitter.m_pAlphaScale = &s_vOpacity;
			Jitter.m_pDrift = &s_vDrift;
			// 头部厚、尾部薄：与火焰的「尾端散开」相反，墨是尾端收细。
			EmitStrand(vOut, vSamples, s_vTendril, 1.10f, 0.16f, 1.0f,
				Palette.m_Main.WithAlpha(0.92f), Palette.m_Main.WithAlpha(0.12f), Spec, MaxAlpha, Jitter);

			// 3) 灰白薄边：贴着触须上下缘勾出轮廓，把墨黑从暗背景里「描」出来。
			for(int Edge = -1; Edge <= 1; Edge += 2)
			{
				for(size_t i = 0; i < Count; ++i)
				{
					const SSample &Sample = vSamples[i];
					const float TendrilHalfWidth = Sample.m_Width * mix(1.10f, 0.16f, Sample.m_S) * s_vRagged[i];
					s_vEdge[i] = s_vTendril[i] + (float)Edge * TendrilHalfWidth * 0.94f;
				}
				EmitStrand(vOut, vSamples, s_vEdge, 0.10f, 0.02f, 0.85f,
					Palette.m_Core.WithAlpha(0.60f), Palette.m_Core.WithAlpha(0.03f), Spec, MaxAlpha, Jitter);
			}
		}

		// 4) 悬浮碎片：越靠近尾端越向中心收缩并崩成小碎片（消散靠「碎」不靠「淡」）。
		for(int Shard = 0; Shard < 7; ++Shard)
		{
			const unsigned ShardSeed = Seed * 0x85ebca6bu + (unsigned)Shard * 0x165667b1u + 13u;
			const size_t Index = std::min<size_t>((size_t)((0.55f + 0.45f * Hash01(ShardSeed)) * (float)(Count - 1)), Count - 1);
			const SSample &Sample = vSamples[Index];
			const float Contract = std::clamp(1.0f - Sample.m_Age * 0.03f, 0.25f, 1.0f);
			const vec2 Pos = Sample.m_Pos + Sample.m_Normal * (HashSigned(ShardSeed + 3u) * Sample.m_Width * 0.7f * Contract) +
					 Sample.m_Tangent * (HashSigned(ShardSeed + 5u) * Sample.m_Width * 0.4f) +
					 vec2(0.0f, -Sample.m_Age * TICK_SECONDS * 12.0f);
			// 碎片用「暗芯 + 灰白薄边」两层，和触须保持同一套语言。
			const float HalfSize = Sample.m_Width * (0.09f + 0.08f * Hash01(ShardSeed + 7u)) * Contract;
			const float Angle = Hash01(ShardSeed + 11u) * pi;
			EmitDot(vOut, Pos, HalfSize * 1.6f, Angle,
				Palette.m_Accent.WithAlpha(0.45f * Fade(Sample.m_Age, Spec) * Sample.m_RelAlpha), MaxAlpha);
			EmitDot(vOut, Pos, HalfSize, Angle,
				Palette.m_Main.WithAlpha(0.90f * Fade(Sample.m_Age, Spec) * Sample.m_RelAlpha), MaxAlpha);
		}
	}

	// ---------------------------------------------------------------------------
	// 黄金·赐福（《艾尔登法环》赐福指引）
	// ---------------------------------------------------------------------------
	// 形态语言：**编织流动的金丝** + 横向交织细弧 + 赐福脉冲 + 缓缓上升的金尘。
	// 与其他样式的关键差别：金丝是细而密的编织（三道反向流动），
	// 主体是「线」不是「带」，节奏最慢、衰减最缓，像一条被指引的金色路径。
	void EmitGoldenGrace(std::vector<qm_tee_trail::SQuad> &vOut, const std::vector<SSample> &vSamples,
		const SPalette &Palette, const SStyleSpec &Spec, float TimeSec, unsigned Seed, float MaxAlpha)
	{
		const size_t Count = vSamples.size();
		static std::vector<float> s_vThread[3];
		static std::vector<float> s_vOffset;
		static std::vector<float> s_vRagged;
		static std::vector<vec2> s_vDrift;
		s_vOffset.assign(Count, 0.0f);
		BuildRaggedNoise(Count, Seed ^ 0x51ed270bu, 0.80f, 1.18f, s_vRagged);

		// 1) 金色辉光：两圈暖金柔光，比别的样式更淡——留给金丝本身表现。
		{
			SStrandJitter Halo;
			Halo.m_SampleStep = 2;
			EmitStrand(vOut, vSamples, s_vOffset, 1.80f, 0.65f, 1.0f,
				Palette.m_Glow.WithAlpha(0.18f), Palette.m_Deep.WithAlpha(0.02f), Spec, MaxAlpha, Halo);
			EmitStrand(vOut, vSamples, s_vOffset, 1.20f, 0.38f, 1.0f,
				Palette.m_Glow.WithAlpha(0.30f), Palette.m_Deep.WithAlpha(0.03f), Spec, MaxAlpha);
		}

		// 2) 三道金丝编织：每股用不同频率与不同流动方向，交织出「编」的观感。
		for(int Thread = 0; Thread < 3; ++Thread)
		{
			const float Phase = (float)Thread * 2.1f;
			const float Flow = Thread == 1 ? -1.0f : 1.0f;
			const float Frequency = 1.9f + 0.45f * (float)Thread;
			BuildRiseDrift(vSamples, 6.0f + 2.0f * (float)Thread, 5.0f, Phase, s_vDrift);
			s_vThread[Thread].assign(Count, 0.0f);
			for(size_t i = 0; i < Count; ++i)
			{
				const SSample &Sample = vSamples[i];
				s_vThread[Thread][i] = std::sin(Sample.m_S * pi * Frequency - Flow * TimeSec * 0.9f + Phase) * Sample.m_Width * 0.52f;
			}
			SStrandJitter Jitter;
			Jitter.m_pWidthScale = &s_vRagged;
			Jitter.m_pDrift = &s_vDrift;
			EmitStrand(vOut, vSamples, s_vThread[Thread], 0.17f, 0.05f, 0.95f,
				Palette.m_Main.WithAlpha(0.80f), Palette.m_Deep.WithAlpha(0.05f), Spec, MaxAlpha, Jitter);
		}

		// 3) 交织细弧：在若干位置把最外侧两股丝线横向连起来（金丝之间的编织扣）。
		//    Count 可能只有 3~4（极短拖尾），这里必须先夹住起点上限，避免 size_t 下溢。
		const size_t MaxArcStart = Count > 5 ? Count - 5 : 0;
		for(int Arc = 0; Arc < 4; ++Arc)
		{
			const unsigned ArcSeed = Seed * 0x9e3779b9u + (unsigned)Arc * 0x27d4eb2du + 41u;
			const size_t Start = std::min<size_t>(1 + (size_t)(Hash01(ArcSeed) * (float)MaxArcStart), MaxArcStart);
			constexpr size_t SPAN = 3;
			const size_t End = std::min<size_t>(Start + SPAN, Count - 1);
			static std::vector<SPathPoint> s_vArc;
			s_vArc.clear();
			for(size_t Index = Start; Index <= End; ++Index)
			{
				const SSample &Sample = vSamples[Index];
				const float Amount = (float)(Index - Start) / (float)(End - Start);
				const float Lateral = mix(s_vThread[0][Start], s_vThread[2][End], Amount) +
						      std::sin(Amount * pi) * Sample.m_Width * 0.85f;
				s_vArc.push_back({Sample.m_Pos + Sample.m_Normal * Lateral, Sample.m_Width * 0.08f,
					Palette.m_Accent.WithAlpha(0.60f * Fade(Sample.m_Age, Spec) * Sample.m_RelAlpha)});
			}
			EmitPath(vOut, s_vArc, MaxAlpha);
		}

		// 4) 赐福脉冲：沿金丝每隔一段亮起一颗光粒（指引的「赐福」），缓慢呼吸。
		for(int Pulse = 0; Pulse < 5; ++Pulse)
		{
			const unsigned PulseSeed = Seed * 0x165667b1u + (unsigned)Pulse * 0x9e3779b9u + 67u;
			const size_t Index = 1 + (size_t)(Hash01(PulseSeed) * (float)(Count - 2));
			const SSample &Sample = vSamples[std::min<size_t>(Index, Count - 2)];
			const float Gain = 0.25f + 0.75f * std::pow(std::sin(Fract(TimeSec * 0.5f + Hash01(PulseSeed + 3u)) * pi), 2.0f);
			if(Gain <= 0.30f)
				continue;
			const float Radius = Sample.m_Width * (0.26f + 0.18f * Hash01(PulseSeed + 5u));
			EmitRing(vOut, Sample.m_Pos, Radius * 1.55f, Radius * 0.16f,
				Palette.m_Accent.WithAlpha(0.40f * Gain * Fade(Sample.m_Age, Spec) * Sample.m_RelAlpha), MaxAlpha, 10);
			EmitDot(vOut, Sample.m_Pos, Radius * 0.42f, TimeSec * 0.4f + (float)Pulse,
				Palette.m_Core.WithAlpha(0.85f * Gain * Fade(Sample.m_Age, Spec) * Sample.m_RelAlpha), MaxAlpha);
		}

		// 5) 飘散金尘：随年龄上升并缓慢变淡，越老越散（衰减最慢的一套）。
		for(int Dust = 0; Dust < 14; ++Dust)
		{
			const unsigned DustSeed = Seed * 0x27d4eb2du + (unsigned)Dust * 0x9e3779b9u + 53u;
			const size_t Index = std::min<size_t>((size_t)(Hash01(DustSeed) * (float)(Count - 1)), Count - 1);
			const SSample &Sample = vSamples[Index];
			const float AgeSec = Sample.m_Age * TICK_SECONDS;
			const vec2 Offset = Sample.m_Normal * (HashSigned(DustSeed + 3u) * Sample.m_Width * 1.2f) +
					    vec2(std::sin(TimeSec * 1.1f + (float)Dust) * 2.0f, -AgeSec * (10.0f + 12.0f * Hash01(DustSeed + 5u)));
			const ColorRGBA Color = Palette.m_Accent.WithAlpha(0.80f * Fade(Sample.m_Age, Spec) * Sample.m_RelAlpha * MaxAlpha);
			EmitDot(vOut, Sample.m_Pos + Offset, Sample.m_Width * (0.06f + 0.06f * Hash01(DustSeed + 7u)) * (1.0f + AgeSec * 0.4f),
				Hash01(DustSeed + 11u) * pi, Color, MaxAlpha);
		}
	}
} // namespace

int qm_tee_trail::ResolveStyle(int Style)
{
	if(Style <= STYLE_ORIGINAL || Style >= STYLE_COUNT)
		return STYLE_ORIGINAL;
	return Style;
}

void qm_tee_trail::BuildEffect(const std::vector<CTrailPart> &vTrail, int Style, bool UsePresetPalette, float CurTime, float Width, int Seed, std::vector<SQuad> &vOut)
{
	vOut.clear();
	const int Resolved = ResolveStyle(Style);
	if(Resolved == STYLE_ORIGINAL)
		return;

	const SStyleSpec &Spec = STYLE_SPECS[Resolved];
	static std::vector<SSample> s_vSamples;
	float MaxAlpha = 0.0f;
	ColorRGBA AverageTint;
	if(!BuildSamples(vTrail, Spec, CurTime, Width * 0.5f, s_vSamples, MaxAlpha, AverageTint))
		return;

	const SPalette &Palette = UsePresetPalette ? STYLE_PALETTES[Resolved] : PaletteFromTint(AverageTint, Resolved);
	const float TimeSec = CurTime * TICK_SECONDS;
	const unsigned SeedValue = (unsigned)Seed * 0x9e3779b9u + (unsigned)Resolved * 0x85ebca6bu + 0x51ed270bu;

	switch(Resolved)
	{
	case STYLE_CURSED_FLAME:
		EmitCursedFlame(vOut, s_vSamples, Palette, Spec, TimeSec, SeedValue, MaxAlpha);
		break;
	case STYLE_VIOLET_BOLT:
		EmitThunderBreath(vOut, s_vSamples, Palette, Spec, TimeSec, SeedValue, MaxAlpha);
		break;
	case STYLE_SPIRIT_LIGHT:
		EmitSpiritLight(vOut, s_vSamples, Palette, Spec, TimeSec, SeedValue, MaxAlpha);
		break;
	case STYLE_VOID_SHADOW:
		EmitVoidShadow(vOut, s_vSamples, Palette, Spec, TimeSec, SeedValue, MaxAlpha);
		break;
	case STYLE_GOLDEN_GRACE:
		EmitGoldenGrace(vOut, s_vSamples, Palette, Spec, TimeSec, SeedValue, MaxAlpha);
		break;
	default:
		break;
	}

	if(vOut.size() > MAX_QUADS)
		vOut.resize(MAX_QUADS);
}
