// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_TITLE_RENDER_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_TITLE_RENDER_H

#include <game/client/components/qmclient/qm_title_style.h>

class CTextCursor;
class ITextRender;

// 头衔动态渲染参数：颜色风格 + 可选的逐字符波浪浮动。
struct SQmTitleRenderStyle
{
	const SQmTitleStyle *m_pStyle = nullptr; // nullptr 表示不启用动态风格
	SQmTitleBobStyle m_Bob = {}; // 幅度为 0 表示不浮动
	EQmTitleInterpolation m_Interpolation = EQmTitleInterpolation::Smooth;
	// 逐字符相位系数覆盖（每像素）。0 表示沿用风格自带值（多数风格为 0，即整行同步变色）。
	float m_PhasePerPxOverride = 0.0f;
};

// 逐字符掠光（镜面扫过）。与逐字符浮动的区别在于：浮动改顶点位置，掠光只改顶点颜色，
// 因此不需要额外预留边界，也不会让字形离开基线。
//
// 相位 = 字符序号 / 字符总数 + 时间 / 周期，因此无论头衔是两个字符还是十二个字符，
// 都是一道从左掠到右的光带，且扫过整行的耗时固定（不随长度变慢）。
struct SQmTitleShimmer
{
	bool m_Enabled = false;
	float m_Speed = 100.0f; // 1/100 行每秒：100 表示每秒扫过一整行
	float m_DutyCycle = 0.35f; // 掠光高光占行宽的比例
	float m_Amount = 0.32f; // 高光峰值：向白色的插值比例
};

// 解析某个玩家头衔应当使用的动态风格。pServerStyleId 由服务端下发，优先于本地配置；
// 为空或未知 id 时回退到本地配置，本地也未启用则返回 m_pStyle == nullptr。
SQmTitleRenderStyle QmTitleResolveRenderStyle(const char *pServerStyleId);

// 把动态风格写入文本光标：逐字符色段（含字符内渐变所需的右边缘色）与逐字符浮动偏移。
//
// pText 必须是头衔本体（不含外层方括号），FontSize 与该次绘制一致。
// 逐字符相位按「字符左边缘相对行首的累计像素宽度」计算，与 Calamity 的 pos.X 语义一致，
// 因此中英文混排时波纹与颜色带的间距仍然均匀。
//
// Shimmer 为可选参数：启用时把掠光的高光叠加到逐字符色段上（只改颜色，不改布局）。
//
// 返回 true 表示内容随时间变化，调用方需要按帧重建文本容器；false 表示静态，只需建一次。
// 与 QmAddTitleRainbowSplits 一样，色段使用字节偏移作为字符序号（与引擎的 m_CharCount 语义一致）。
bool QmTitleRenderFillCursor(ITextRender *pTextRender, CTextCursor &Cursor, const char *pText, float FontSize, const SQmTitleRenderStyle &Style, float TimeSec, float Alpha, const SQmTitleShimmer &Shimmer = {});

// 求某个字符在掠光周期里的高光强度，取值 [0, 1]。CharIndex 与 CharCount 为该次绘制内的字符序号与总数。
// 抽成独立函数是为了让「一个周期内恰好扫过一次、且整行亮度守恒」这类性质可以直接测试。
float QmTitleShimmerFactor(const SQmTitleShimmer &Shimmer, int CharIndex, int CharCount, float TimeSec);

// 掠光条带按 UTF-8 字符计数分配相位：长头衔不能因为字节数多就扫得更慢。
int QmTitleShimmerUtf8CharCount(const char *pText);

// 按当前配置构造掠光参数。名牌与设置页预览共用同一份规则，避免「预览有掠光、实际没有」。
SQmTitleShimmer QmTitleShimmerFromConfig();

#endif
