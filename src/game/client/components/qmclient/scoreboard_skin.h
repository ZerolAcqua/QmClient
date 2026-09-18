#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_SCOREBOARD_SKIN_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_SCOREBOARD_SKIN_H

#include <base/str.h>

#include <engine/shared/config.h>

inline bool QmCopyScoreboardSkin(CConfig &Config, bool Sixup, const char *pSkinName, int UseCustomColor, int ColorBody, int ColorFeet)
{
	// 0.7 使用独立部件配置，不能把兼容皮肤名写入 0.6 配置。
	if(Sixup)
		return false;

	const bool Dummy = Config.m_ClDummy != 0;
	str_copy(Dummy ? Config.m_ClDummySkin : Config.m_ClPlayerSkin, pSkinName, sizeof(Config.m_ClPlayerSkin));
	(Dummy ? Config.m_ClDummyUseCustomColor : Config.m_ClPlayerUseCustomColor) = UseCustomColor;
	(Dummy ? Config.m_ClDummyColorBody : Config.m_ClPlayerColorBody) = ColorBody;
	(Dummy ? Config.m_ClDummyColorFeet : Config.m_ClPlayerColorFeet) = ColorFeet;
	return true;
}

#endif
