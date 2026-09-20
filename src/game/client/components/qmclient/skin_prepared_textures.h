#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_SKIN_PREPARED_TEXTURES_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_SKIN_PREPARED_TEXTURES_H

#include <engine/gfx/sprite_image.h>

#include <generated/client_data.h>

#include <array>
#include <memory>

// 未上传的像素由任务持有；上传后 CImageInfo 被移空，析构只回收剩余 CPU 数据。
class CQmPreparedSkinTextures
{
	std::array<std::array<CImageInfo, 12>, 2> m_aaImages;

public:
	CQmPreparedSkinTextures() = default;
	CQmPreparedSkinTextures(const CQmPreparedSkinTextures &) = delete;
	CQmPreparedSkinTextures &operator=(const CQmPreparedSkinTextures &) = delete;
	~CQmPreparedSkinTextures()
	{
		for(auto &aImages : m_aaImages)
			for(auto &Image : aImages)
				Image.Free();
	}

	static int SpriteId(size_t Index)
	{
		constexpr int aSprites[] = {SPRITE_TEE_BODY, SPRITE_TEE_BODY_OUTLINE, SPRITE_TEE_FOOT, SPRITE_TEE_FOOT_OUTLINE,
			SPRITE_TEE_HAND, SPRITE_TEE_HAND_OUTLINE, SPRITE_TEE_EYE_NORMAL, SPRITE_TEE_EYE_ANGRY,
			SPRITE_TEE_EYE_PAIN, SPRITE_TEE_EYE_HAPPY, SPRITE_TEE_EYE_DEAD, SPRITE_TEE_EYE_SURPRISE};
		return aSprites[Index];
	}
	CImageInfo &Image(size_t Variant, size_t Index) { return m_aaImages[Variant][Index]; }
};

inline std::unique_ptr<CQmPreparedSkinTextures> QmPrepareSkinTextures(const CImageInfo &Original, const CImageInfo &Colorable, const CDataSprite *pSprites)
{
	auto pResult = std::make_unique<CQmPreparedSkinTextures>();
	for(size_t Variant = 0; Variant < 2; ++Variant)
	{
		const CImageInfo &Source = Variant == 0 ? Original : Colorable;
		for(size_t Index = 0; Index < 12; ++Index)
			if(!ExtractSpriteImage(Source, &pSprites[CQmPreparedSkinTextures::SpriteId(Index)], pResult->Image(Variant, Index)))
				return nullptr;
	}
	return pResult;
}

#endif
