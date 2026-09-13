#ifndef GAME_CLIENT_COMPONENTS_NAMEPLATES_H
#define GAME_CLIENT_COMPONENTS_NAMEPLATES_H

#include <base/color.h>
#include <base/vmath.h>

#include <game/client/component.h>

inline bool QmNameplateUsesPhysicalPixelAlignment(const float HiDpiScale, const bool IsMacos)
{
	return IsMacos && HiDpiScale > 1.0f;
}

struct CNetObj_PlayerInfo;
class CUIRect;

class CNamePlates : public CComponent
{
private:
	class CNamePlatesData;
	CNamePlatesData *m_pData;
	void ResetChatBubbleAnimState(int ClientId, bool IsDestructing = false);
	void UpdateCoordXAlignFrameState();

public:
	void RenderNamePlateGame(vec2 Position, const CNetObj_PlayerInfo *pPlayerInfo, float Alpha, bool TrackCoordXAlign = true);
	// 预览框（设置页深色预览区域）所需高度：铭牌内容 + 与脚本体间距 + 脚本体 + 上下留白。
	// 玩家/分身取较大者，切换预览时框高不变。
	float MeasurePreviewAreaHeight() const;
	// PreviewArea 同时是自由移动的边界：铭牌与脚本体贴框底排布，拖动范围就是可见范围。
	void RenderNamePlatePreview(const CUIRect &PreviewArea, int Dummy);
	void RenderChatBubble(vec2 Position, int ClientId, float Alpha);
	void ResetNamePlates();
	int Sizeof() const override { return sizeof(*this); }
	void OnShutdown() override;
	void OnWindowResize() override;
	void OnRender() override;
	CNamePlates();
	~CNamePlates() override;
};

#endif
