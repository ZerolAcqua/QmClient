#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_DEMO_UI_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_DEMO_UI_H

#include <game/client/ui_rect.h>

#include <algorithm>

namespace qm_demo_ui
{
	constexpr float DISPLAY_HEIGHT = 116.0f;

	inline CUIRect PlayerRect(const CUIRect &Screen, bool DisplayExpanded)
	{
		const float Width = std::min(640.0f, Screen.w - 24.0f);
		const float Height = 120.0f + (DisplayExpanded ? DISPLAY_HEIGHT + 6.0f : 0.0f);
		return {Screen.x + (Screen.w - Width) * 0.5f, Screen.y + Screen.h - Height - 12.0f, Width, Height};
	}

	constexpr float TransportWidth(float ButtonSize)
	{
		// 十个图标、时长选择、倍速文字，以及组内和组间间距。
		return 10.0f * ButtonSize + 56.0f + 36.0f + 42.0f;
	}

	inline float TransportButtonSize(float PanelWidth)
	{
		return std::min(22.0f, (PanelWidth - 16.0f - TransportWidth(0.0f)) / 10.0f);
	}

	inline float SliceContentHeight(int SegmentCount, bool DisplayExpanded)
	{
		const float SegmentsHeight = SegmentCount > 0 ? 26.0f + std::min(SegmentCount, 4) * 22.0f : 0.0f;
		return 106.0f + SegmentsHeight + (DisplayExpanded ? DISPLAY_HEIGHT + 4.0f : 0.0f);
	}

	inline CUIRect PopupRect(const CUIRect &Screen, float ContentHeight)
	{
		const float Width = std::min(560.0f, Screen.w - 24.0f);
		const float Height = std::min(ContentHeight, Screen.h - 24.0f);
		return {Screen.x + (Screen.w - Width) * 0.5f, Screen.y + (Screen.h - Height) * 0.5f, Width, Height};
	}
}

#endif // GAME_CLIENT_COMPONENTS_QMCLIENT_DEMO_UI_H
