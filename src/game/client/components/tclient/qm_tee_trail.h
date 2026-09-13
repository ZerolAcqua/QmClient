// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_QM_TEE_TRAIL_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_QM_TEE_TRAIL_H

#include <base/color.h>
#include <base/vmath.h>

#include <cstddef>
#include <vector>

class CTrailPart;

// Tee 拖尾特效样式：原版拖尾之外另有 5 套独立形态 / 运动 / 消散效果。
//
// 设计约定：
// - 本模块只做纯几何构建，不碰 Graphics()，输出一串四边形交给渲染端；
// - 输入是拖尾采样点（CTrailPart：位置、颜色、半宽、tick）与当前时间，
//   因此同一组输入必然得到同一组输出，可测试、可重放；
// - 特效只在移动时生成，停下后随采样点年龄自然消散（寿命见 qm_tee_trail.cpp）。
namespace qm_tee_trail
{
	enum
	{
		STYLE_ORIGINAL = 0, // 原版拖尾（仍由 trails.cpp 既有路径渲染）
		STYLE_CURSED_FLAME = 1, // 咒焰·黑闪：蓝青咒力火舌 + 黑色核心 + 猩红分叉电弧
		STYLE_VIOLET_BOLT = 2, // 紫电·雷切：雷之呼吸·火雷神（紫色版），斩线 + 紫电雷纹 + 落点雷环
		STYLE_SPIRIT_LIGHT = 3, // 灵光·流萤：蓝白流光细丝 + 缓慢漂浮的光点
		STYLE_VOID_SHADOW = 4, // 虚空·暗影：墨黑烟絮触须 + 灰白薄边 + 尾部碎片
		STYLE_GOLDEN_GRACE = 5, // 黄金·赐福：金色丝线 + 交织细弧 + 飘散金尘
		STYLE_COUNT = 6,
	};

	// 单个输出四边形：4 个顶点位置与 4 个顶点颜色（无贴图自由形变四边形）。
	struct SQuad
	{
		vec2 m_aPos[4];
		ColorRGBA m_aColor[4];
	};

	// 单次构建的输出上限，避免超长拖尾把顶点数量打爆。
	// 目前最重的一套（咒焰·黑闪）在满采样下约 380 个四边形。
	constexpr size_t MAX_QUADS = 448;

	// 非法或未启用样式统一回落到原版拖尾。
	int ResolveStyle(int Style);

	// 构建一套样式的全部四边形。
	// vTrail 从新到旧排列（vTrail[0] 为 Tee 当前位置）。
	// UsePresetPalette 为 true 时使用样式自带标志性配色，否则从拖尾采样颜色派生。
	// CurTime 与 CTrailPart::m_Tick 同一时间基（游戏 tick，可带小数）。
	// Width 为拖尾半宽（像素），仅在采样点自带宽度为 0 时作为回退。
	// Seed 让同屏多个 Tee 的随机细节互不相同，同一 (Seed, 输入) 结果稳定。
	void BuildEffect(const std::vector<CTrailPart> &vTrail, int Style, bool UsePresetPalette, float CurTime, float Width, int Seed, std::vector<SQuad> &vOut);
} // namespace qm_tee_trail

#endif
