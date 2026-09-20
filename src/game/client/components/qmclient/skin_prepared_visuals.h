#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_SKIN_PREPARED_VISUALS_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_SKIN_PREPARED_VISUALS_H

#include "qm_chat_avatar.h"
#include "qm_skin_outline.h"

#include <generated/client_data.h>

#include <game/client/skin.h>

// 解码任务只准备 CPU 素材，发布后由主线程接管；此处不创建或释放 GPU 纹理。
struct SQmPreparedSkinVisuals
{
	std::shared_ptr<const QmChatAvatar::SSource> m_pOriginalAvatar;
	std::shared_ptr<const QmChatAvatar::SSource> m_pColorableAvatar;
	std::shared_ptr<CQmSkinOutline> m_pBodyOutline;
	std::shared_ptr<CQmSkinOutline> m_pFeetOutline;

	void Apply(CSkin &Skin) const
	{
		Skin.m_OriginalSkin.m_pChatAvatar = m_pOriginalAvatar;
		Skin.m_ColorableSkin.m_pChatAvatar = m_pColorableAvatar;
		Skin.m_OriginalSkin.m_pBodyOutline = m_pBodyOutline;
		Skin.m_OriginalSkin.m_pFeetOutline = m_pFeetOutline;
	}
};

inline SQmPreparedSkinVisuals QmPrepareSkinVisuals(const CImageInfo &Original, const CImageInfo &Colorable, const CDataSprite *pSprites)
{
	const auto PrepareAvatar = [pSprites](const CImageInfo &Image) {
		auto pSource = std::make_shared<QmChatAvatar::SSource>();
		constexpr int aSprites[] = {SPRITE_TEE_BODY, SPRITE_TEE_BODY_OUTLINE, SPRITE_TEE_FOOT, SPRITE_TEE_FOOT_OUTLINE, SPRITE_TEE_EYE_NORMAL};
		for(size_t Index = 0; Index < std::size(aSprites); ++Index)
			pSource->m_aSprites[Index] = QmChatAvatar::CopySprite(Image, pSprites[aSprites[Index]]);
		return pSource;
	};
	return {
		PrepareAvatar(Original),
		PrepareAvatar(Colorable),
		QmCreateSkinOutline(Original, pSprites[SPRITE_TEE_BODY], pSprites[SPRITE_TEE_BODY_OUTLINE], vec2(64, 64)),
		QmCreateSkinOutline(Original, pSprites[SPRITE_TEE_FOOT], pSprites[SPRITE_TEE_FOOT_OUTLINE], vec2(64, 32)),
	};
}

#endif
