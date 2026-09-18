/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_EMOTICON_H
#define GAME_CLIENT_COMPONENTS_EMOTICON_H
#include <base/vmath.h>

#include <engine/client/enums.h>
#include <engine/console.h>

#include <generated/protocol.h>

#include <game/client/component.h>
#include <game/client/components/tclient/bindwheel.h>
#include <game/client/ui.h>

#include <string>

namespace QmEmoticon
{
	enum class EEffect
	{
		INVALID,
		NONE,
		SUPER_HEAD,
		PROJECTILE,
		SUPER_PROJECTILE,
	};

	inline EEffect ResolveEffect(int Emoticon, bool LaunchMode, bool SuperLaunch)
	{
		if(Emoticon < 0 || Emoticon >= NUM_EMOTICONS)
			return EEffect::INVALID;
		if(LaunchMode)
			return SuperLaunch ? EEffect::SUPER_PROJECTILE : EEffect::PROJECTILE;
		return SuperLaunch ? EEffect::SUPER_HEAD : EEffect::NONE;
	}

	inline EEffect ConsumeEffect(int Emoticon, bool LaunchMode, bool &SuperPending)
	{
		const bool SuperLaunch = SuperPending;
		SuperPending = false;
		return ResolveEffect(Emoticon, LaunchMode, SuperLaunch);
	}

	inline EEffect ResolveRemoteEffect(int Emoticon, bool LaunchMode, bool SuperLaunch, bool ShowEmotes, bool EmoticonIgnored, bool ShowSuper, bool ShowLaunch)
	{
		if(!ShowEmotes || EmoticonIgnored)
			return EEffect::NONE;
		const EEffect Effect = ResolveEffect(Emoticon, LaunchMode, SuperLaunch);
		if((Effect == EEffect::SUPER_HEAD && !ShowSuper) ||
			((Effect == EEffect::PROJECTILE || Effect == EEffect::SUPER_PROJECTILE) && !ShowLaunch))
			return EEffect::NONE;
		return Effect;
	}
}

class CCollision;

struct CEmoticonProjectile
{
	vec2 m_Pos;
	vec2 m_Vel;
	float m_Angle;
	float m_AngVel;
	int m_EmoticonID;
	float m_LifeTime;
	float m_SizeScale;
	bool m_Active;

	void Init(vec2 Pos, vec2 Vel, int EmoticonID, float SizeScale = 1.0f);
	void Update(float Dt, CCollision *pCollision);
};
struct SQmLocalBlinkState
{
	static constexpr int DURATION_TICKS = 4;

	void Trigger(int CurrentTick)
	{
		m_StopTick = CurrentTick + DURATION_TICKS;
	}

	void Reset()
	{
		m_StopTick = 0;
	}

	bool IsActive(int CurrentTick) const
	{
		return CurrentTick >= 0 && CurrentTick < m_StopTick;
	}

private:
	int m_StopTick = 0;
};

class CEmoticon : public CComponent
{
	bool m_WasActive;
	bool m_Active;
	bool m_PresentationInitialized;

	vec2 m_SelectorMouse;
	int m_SelectedEmote;
	int m_SelectedEyeEmote;
	SQmLocalBlinkState m_aLocalBlinkStates[NUM_DUMMIES];

	CUi::CTouchState m_TouchState;
	bool m_TouchPressedOutside;
	bool m_LaunchModeActive = false;
	enum
	{
		MAX_PROJECTILES = 64
	};
	CEmoticonProjectile m_aProjectiles[MAX_PROJECTILES];
	float m_SuperChargeSeconds = 0.0f;
	float m_SuperChargeProgress = 0.0f;
	int m_SuperChargeTrackedEmote = -1;
	int m_SuperChargeRingEmote = -1;
	float m_SuperChargeRingPhase = 0.0f;
	float m_SuperChargeRingCharge = 0.0f;
	int m_SuperChargeRingExitEmote = -1;
	float m_SuperChargeRingExitPhase = 0.0f;
	float m_SuperChargeRingExitCharge = 0.0f;
	bool m_SuperLaunchPending = false;
	int m_LocalSuperHeadEmoticon = -1;
	int m_LocalSuperHeadExpireTick = -1;
	int m_aRemoteSuperHeadEmoticons[MAX_CLIENTS] = {};
	int m_aRemoteSuperHeadExpireTicks[MAX_CLIENTS] = {};

	static void ConKeyEmoticon(IConsole::IResult *pResult, void *pUserData);
	static void ConEmote(IConsole::IResult *pResult, void *pUserData);
	static void ConLocalBlink(IConsole::IResult *pResult, void *pUserData);
	static void ConSuperEmote(IConsole::IResult *pResult, void *pUserData);
	static void ConToggleLaunchMode(IConsole::IResult *pResult, void *pUserData);
	void ToggleLaunchMode();

public:
	CEmoticon();
	int Sizeof() const override { return sizeof(*this); }

	void OnReset() override;
	void OnConsoleInit() override;
	void OnRender() override;
	void OnRelease() override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	bool OnInput(const IInput::CEvent &Event) override;

	void Emote(int Emoticon);
	void SuperEmote(int Emoticon);
	void EyeEmote(int EyeEmote);
	void TriggerLocalBlink();
	bool ShouldRenderLocalBlink(int ClientId) const;
	bool IsLocalSuperHeadEmoticon(int ClientId, int Emoticon) const;
	bool IsLaunchModeActive() const { return m_LaunchModeActive; }

	bool IsActive() const { return m_Active; }

	friend class CBindWheel;
};

#endif
