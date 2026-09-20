#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_SCOREBOARD_FOOTER_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_SCOREBOARD_FOOTER_H

#include <game/client/ui_rect.h>

struct SQmScoreboardFooterLayout
{
	CUIRect m_Media{};
	CUIRect m_Spectators{};
};

inline SQmScoreboardFooterLayout QmScoreboardFooterLayout(CUIRect Area, bool HasMedia, int NumSpectators)
{
	SQmScoreboardFooterLayout Layout;
	if(HasMedia)
	{
		Area.HSplitTop(25.0f, &Layout.m_Media, &Area);
		if(NumSpectators > 0)
			Area.HSplitTop(5.0f, nullptr, &Area);
	}
	// 旁观者区域只保留可用上限，背景在文字测量后按实际行数收缩。
	if(NumSpectators > 0)
		Layout.m_Spectators = Area;
	return Layout;
}

#endif
