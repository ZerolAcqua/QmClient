/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "emoticon.h"

#include "chat.h"

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/client/QmUi/QmAnimResolve.h>
#include <game/client/QmUi/UiTokens.h>
#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/collision.h>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace
{
	constexpr float s_SuperChargeSecondsRequired = 1.5f;
	constexpr float s_SuperProjectileScale = 2.35f;
	constexpr float s_SuperChargeRingThickness = 4.0f;
	constexpr int s_SuperChargeRingSegments = 72;
	constexpr float s_SuperChargeRingAnimationSeconds = 0.18f;

	void RenderChargeRing(IGraphics *pGraphics, vec2 Center, float OuterRadius, float Thickness, float Progress, ColorRGBA FilledColor, ColorRGBA EmptyColor, int Segments)
	{
		if(OuterRadius <= 0.0f || Thickness <= 0.0f || Segments <= 0)
			return;
		const float ClampedProgress = std::clamp(Progress, 0.0f, 1.0f);
		const float InnerRadius = std::max(0.0f, OuterRadius - Thickness);
		const float SegmentAngle = 2.0f * pi / (float)Segments;
		const float AngleOffset = -0.5f * pi;

		pGraphics->TextureClear();
		pGraphics->QuadsBegin();
		pGraphics->SetColor(EmptyColor);
		for(int i = 0; i < Segments; ++i)
		{
			const vec2 Dir1 = direction(AngleOffset + i * SegmentAngle);
			const vec2 Dir2 = direction(AngleOffset + (i + 1) * SegmentAngle);
			const IGraphics::CFreeformItem Item(
				Center + Dir1 * InnerRadius, Center + Dir2 * InnerRadius,
				Center + Dir1 * OuterRadius, Center + Dir2 * OuterRadius);
			pGraphics->QuadsDrawFreeform(&Item, 1);
		}
		pGraphics->SetColor(FilledColor);
		const float FilledSegments = ClampedProgress * Segments;
		const int WholeSegments = std::clamp((int)std::floor(FilledSegments), 0, Segments);
		for(int i = 0; i < WholeSegments; ++i)
		{
			const vec2 Dir1 = direction(AngleOffset + i * SegmentAngle);
			const vec2 Dir2 = direction(AngleOffset + (i + 1) * SegmentAngle);
			const IGraphics::CFreeformItem Item(
				Center + Dir1 * InnerRadius, Center + Dir2 * InnerRadius,
				Center + Dir1 * OuterRadius, Center + Dir2 * OuterRadius);
			pGraphics->QuadsDrawFreeform(&Item, 1);
		}
		const float PartialSegment = FilledSegments - WholeSegments;
		if(PartialSegment > 0.0f && WholeSegments < Segments)
		{
			const float Angle1 = AngleOffset + WholeSegments * SegmentAngle;
			const float Angle2 = Angle1 + SegmentAngle * PartialSegment;
			const IGraphics::CFreeformItem Item(
				Center + direction(Angle1) * InnerRadius, Center + direction(Angle2) * InnerRadius,
				Center + direction(Angle1) * OuterRadius, Center + direction(Angle2) * OuterRadius);
			pGraphics->QuadsDrawFreeform(&Item, 1);
		}
		pGraphics->QuadsEnd();
	}
}

void CEmoticonProjectile::Init(vec2 Pos, vec2 Vel, int EmoticonID, float SizeScale)
{
	m_Pos = Pos;
	m_Vel = Vel;
	m_EmoticonID = EmoticonID;
	m_LifeTime = 3.0f;
	m_SizeScale = std::max(SizeScale, 0.1f);
	m_Active = true;
	m_Angle = 0.0f;
	m_AngVel = (float)((rand() % 100) - 50) / 10.0f;
}

void CEmoticonProjectile::Update(float Dt, CCollision *pCollision)
{
	if(!m_Active)
		return;
	m_LifeTime -= Dt;
	if(m_LifeTime < 0.0f)
	{
		m_Active = false;
		return;
	}
	m_Vel.y += 1500.0f * Dt;
	vec2 Move = m_Vel * Dt;
	int Bounces = 0;
	pCollision->MovePoint(&m_Pos, &Move, 0.6f, &Bounces);
	if(Dt > 0.0001f)
		m_Vel = Move / Dt;
	m_Angle += m_AngVel * Dt;
}

static uint64_t EmoticonPresentationNodeKey(const char *pScope)
{
	static const uint64_t s_BaseKey = static_cast<uint64_t>(str_quickhash("qm_extra_emoticon_presentation"));
	return BuildUiAnimNodeKey(s_BaseKey, static_cast<uint64_t>(str_quickhash(pScope)));
}

static SUiSpringConfig EmoticonPresentationSpring()
{
	SUiSpringConfig Spring;
	Spring.m_Stiffness = 470.0f;
	Spring.m_Damping = 40.0f;
	Spring.m_RestEpsilon = 0.006f;
	Spring.m_RestVelocity = 0.08f;
	return Spring;
}

static int EmoticonClockwiseOrderFromTop(int Index, int Count)
{
	const float Angle = (2.0f * pi * Index) / Count;
	const float ClockwiseFromTop = std::fmod(Angle + pi / 2.0f + 2.0f * pi, 2.0f * pi);
	return std::clamp(static_cast<int>(std::round(ClockwiseFromTop / (2.0f * pi) * Count)), 0, Count - 1);
}

static float EmoticonStaggerReveal(int Index, int Count, float PresentationAlpha)
{
	if(Count <= 1)
		return PresentationAlpha;

	constexpr float MaxDelay = 0.42f;
	const int Order = EmoticonClockwiseOrderFromTop(Index, Count);
	const float Delay = MaxDelay * Order / (Count - 1);
	const float Denominator = std::max(0.001f, 1.0f - Delay);
	const float LocalT = std::clamp((PresentationAlpha - Delay) / Denominator, 0.0f, 1.0f);
	const float Inv = 1.0f - LocalT;
	return 1.0f - Inv * Inv * Inv * Inv;
}

CEmoticon::CEmoticon()
{
	OnReset();
}

void CEmoticon::ConKeyEmoticon(IConsole::IResult *pResult, void *pUserData)
{
	CEmoticon *pSelf = (CEmoticon *)pUserData;

	if(pSelf->GameClient()->m_Scoreboard.IsActive())
		return;

	if(!pSelf->GameClient()->m_Snap.m_SpecInfo.m_Active && pSelf->Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(pSelf->GameClient()->m_BindWheel.IsActive())
			pSelf->m_Active = false;
		else
			pSelf->m_Active = pResult->GetInteger(0) != 0;
	}
}

void CEmoticon::ConEmote(IConsole::IResult *pResult, void *pUserData)
{
	((CEmoticon *)pUserData)->Emote(pResult->GetInteger(0));
}

void CEmoticon::ConSuperEmote(IConsole::IResult *pResult, void *pUserData)
{
	((CEmoticon *)pUserData)->SuperEmote(pResult->GetInteger(0));
}

void CEmoticon::ConToggleLaunchMode(IConsole::IResult *, void *pUserData)
{
	((CEmoticon *)pUserData)->ToggleLaunchMode();
}

void CEmoticon::ConLocalBlink(IConsole::IResult *, void *pUserData)
{
	((CEmoticon *)pUserData)->TriggerLocalBlink();
}

void CEmoticon::OnConsoleInit()
{
	Console()->Register("+emote", "", CFGFLAG_CLIENT, ConKeyEmoticon, this, "Open emote selector");
	Console()->Register("emote", "i[emote-id]", CFGFLAG_CLIENT, ConEmote, this, "Use emote");
	Console()->Register("super_emote", "i[emote-id]", CFGFLAG_CLIENT, ConSuperEmote, this, "Use large emote");
	Console()->Register("qm_blink", "", CFGFLAG_CLIENT, ConLocalBlink, this, "Blink the active local tee");
	Console()->Register("toggle_emote_launcher", "", CFGFLAG_CLIENT, ConToggleLaunchMode, this, "Toggle emote launcher");
}

void CEmoticon::OnReset()
{
	m_WasActive = false;
	m_Active = false;
	m_PresentationInitialized = false;
	m_SelectedEmote = -1;
	m_SelectedEyeEmote = -1;
	m_LaunchModeActive = false;
	m_SuperChargeSeconds = 0.0f;
	m_SuperChargeProgress = 0.0f;
	m_SuperChargeTrackedEmote = -1;
	m_SuperChargeRingEmote = -1;
	m_SuperChargeRingPhase = 0.0f;
	m_SuperChargeRingCharge = 0.0f;
	m_SuperChargeRingExitEmote = -1;
	m_SuperChargeRingExitPhase = 0.0f;
	m_SuperChargeRingExitCharge = 0.0f;
	m_SuperLaunchPending = false;
	m_LocalSuperHeadEmoticon = -1;
	m_LocalSuperHeadExpireTick = -1;
	std::fill(std::begin(m_aRemoteSuperHeadEmoticons), std::end(m_aRemoteSuperHeadEmoticons), -1);
	std::fill(std::begin(m_aRemoteSuperHeadExpireTicks), std::end(m_aRemoteSuperHeadExpireTicks), -1);
	for(auto &Projectile : m_aProjectiles)
		Projectile.m_Active = false;
	for(auto &LocalBlinkState : m_aLocalBlinkStates)
		LocalBlinkState.Reset();
	m_TouchPressedOutside = false;
}

void CEmoticon::OnRelease()
{
	m_Active = false;
}

void CEmoticon::ToggleLaunchMode()
{
	if(!m_Active)
		return;
	m_LaunchModeActive = !m_LaunchModeActive;
	GameClient()->Echo(m_LaunchModeActive ? "表情发射：开启" : "表情发射：关闭");
}

bool CEmoticon::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!m_Active)
		return false;

	Ui()->ConvertMouseMove(&x, &y, CursorType);
	m_SelectorMouse += vec2(x, y);
	return true;
}

bool CEmoticon::OnInput(const IInput::CEvent &Event)
{
	if(IsActive() && Event.m_Flags & IInput::FLAG_PRESS)
	{
		if(Event.m_Key == KEY_ESCAPE)
		{
			OnRelease();
			return true;
		}
		if(Event.m_Key == KEY_TAB)
		{
			ToggleLaunchMode();
			return true;
		}
	}
	return false;
}

void CEmoticon::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	static const auto PositiveMod = [](float x, float y) -> float {
		return std::fmod(x + y, y);
	};

	static const float s_InnerMouseLimitRadius = 40.0f;
	static const float s_InnerOuterMouseBoundaryRadius = 110.0f;
	static const float s_OuterMouseLimitRadius = 170.0f;
	static const float s_InnerItemRadius = 70.0f;
	static const float s_OuterItemRadius = 150.0f;
	static const float s_InnerCircleRadius = 100.0f;
	static const float s_OuterCircleRadius = 190.0f;

	CQmClient::SQmRemoteEmoticonEvent RemoteEvent;
	while(GameClient()->m_QmClient.PollQmRemoteEmoticonEvent(RemoteEvent))
	{
		if(RemoteEvent.m_ClientId == GameClient()->m_QmClient.QmClientId())
			continue;
		if(RemoteEvent.m_PlayerId < 0 || RemoteEvent.m_PlayerId >= MAX_CLIENTS || RemoteEvent.m_Emoticon < 0 || RemoteEvent.m_Emoticon >= NUM_EMOTICONS)
			continue;
		if(!GameClient()->m_aClients[RemoteEvent.m_PlayerId].m_Active ||
			str_comp(GameClient()->m_aClients[RemoteEvent.m_PlayerId].m_aName, RemoteEvent.m_PlayerName.c_str()) != 0)
			continue;
		const auto Effect = QmEmoticon::ResolveRemoteEffect(RemoteEvent.m_Emoticon, RemoteEvent.m_LaunchMode, RemoteEvent.m_SuperLaunch,
			g_Config.m_ClShowEmotes, GameClient()->m_aClients[RemoteEvent.m_PlayerId].m_EmoticonIgnore,
			g_Config.m_QmShowOtherSuperEmotes, g_Config.m_QmShowOtherLaunchEmotes);
		m_aRemoteSuperHeadEmoticons[RemoteEvent.m_PlayerId] = -1;
		m_aRemoteSuperHeadExpireTicks[RemoteEvent.m_PlayerId] = -1;
		if(Effect == QmEmoticon::EEffect::SUPER_HEAD)
		{
			m_aRemoteSuperHeadEmoticons[RemoteEvent.m_PlayerId] = RemoteEvent.m_Emoticon;
			m_aRemoteSuperHeadExpireTicks[RemoteEvent.m_PlayerId] = Client()->GameTick(g_Config.m_ClDummy) + 2 * Client()->GameTickSpeed();
		}
		if(Effect != QmEmoticon::EEffect::PROJECTILE && Effect != QmEmoticon::EEffect::SUPER_PROJECTILE)
			continue;
		const bool SuperLaunch = Effect == QmEmoticon::EEffect::SUPER_PROJECTILE;

		vec2 LaunchPos = GameClient()->m_aClients[RemoteEvent.m_PlayerId].m_RenderPos;
		LaunchPos.y -= 20.0f;
		vec2 Dir = direction(GameClient()->m_aClients[RemoteEvent.m_PlayerId].m_RenderCur.m_Angle / 256.0f);
		if(length(Dir) <= 0.0001f)
			Dir = vec2(1.0f, 0.0f);
		const vec2 Vel = Dir * 1200.0f + vec2(0.0f, -400.0f);
		for(auto &Projectile : m_aProjectiles)
		{
			if(Projectile.m_Active)
				continue;
			Projectile.Init(LaunchPos, Vel, RemoteEvent.m_Emoticon, SuperLaunch ? s_SuperProjectileScale : 1.0f);
			if(SuperLaunch)
				GameClient()->m_Effects.Explosion(LaunchPos, 0.9f);
			else
				GameClient()->m_Effects.HammerHit(LaunchPos, 0.65f, 0.0f);
			break;
		}
	}

	for(auto &Projectile : m_aProjectiles)
	{
		if(!Projectile.m_Active)
			continue;
		float Width, Height;
		Graphics()->CalcScreenParams(Graphics()->ScreenAspect(), GameClient()->m_Camera.m_Zoom, &Width, &Height);
		const vec2 Center = GameClient()->m_Camera.m_Center;
		Graphics()->MapScreen(Center.x - Width / 2.0f, Center.y - Height / 2.0f, Center.x + Width / 2.0f, Center.y + Height / 2.0f);
		Projectile.Update(Client()->RenderFrameTime(), Collision());
		Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_aSpriteEmoticons[Projectile.m_EmoticonID]);
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetRotation(Projectile.m_Angle);
		float Scale = std::max(Projectile.m_SizeScale, 0.1f);
		float Alpha = 1.0f;
		if(Projectile.m_LifeTime < 0.5f)
		{
			Scale *= 1.0f + (0.5f - Projectile.m_LifeTime) * 2.0f;
			Alpha = Projectile.m_LifeTime * 2.0f;
		}
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
		IGraphics::CQuadItem QuadItem(Projectile.m_Pos.x, Projectile.m_Pos.y, 64.0f * Scale, 64.0f * Scale);
		Graphics()->QuadsDraw(&QuadItem, 1);
		Graphics()->QuadsEnd();
		Ui()->MapScreen();
	}

	if(!m_Active)
	{
		if(m_TouchPressedOutside)
		{
			m_SelectedEmote = -1;
			m_SelectedEyeEmote = -1;
			m_TouchPressedOutside = false;
		}

		if(m_WasActive && m_SelectedEmote != -1)
		{
			m_SuperLaunchPending = m_SuperChargeTrackedEmote == m_SelectedEmote && m_SuperChargeProgress >= 1.0f;
			Emote(m_SelectedEmote);
		}
		if(m_WasActive && m_SuperChargeRingEmote != -1)
		{
			m_SuperChargeRingExitEmote = m_SuperChargeRingEmote;
			m_SuperChargeRingExitPhase = m_SuperChargeRingPhase;
			m_SuperChargeRingExitCharge = m_SuperChargeRingCharge;
			m_SuperChargeRingEmote = -1;
			m_SuperChargeRingPhase = 0.0f;
			m_SuperChargeRingCharge = 0.0f;
		}
		if(m_WasActive && m_SelectedEyeEmote != -1)
			EyeEmote(m_SelectedEyeEmote);
		m_WasActive = false;
		m_SuperChargeTrackedEmote = -1;
		m_SuperChargeSeconds = 0.0f;
		m_SuperChargeProgress = 0.0f;
	}
	else
	{
		m_WasActive = true;
	}

	if(GameClient()->m_Snap.m_SpecInfo.m_Active || !GameClient()->m_Snap.m_pLocalCharacter)
	{
		m_Active = false;
		m_WasActive = false;
		return;
	}

	const CUIRect Screen = *Ui()->Screen();

	if(m_Active)
	{
		const bool WasTouchPressed = m_TouchState.m_AnyPressed;
		Ui()->UpdateTouchState(m_TouchState);
		if(m_TouchState.m_AnyPressed)
		{
			const vec2 TouchPos = (m_TouchState.m_PrimaryPosition - vec2(0.5f, 0.5f)) * Screen.Size();
			const float TouchCenterDistance = length(TouchPos);
			if(TouchCenterDistance <= s_OuterMouseLimitRadius)
			{
				m_SelectorMouse = TouchPos;
			}
			else if(TouchCenterDistance > s_OuterCircleRadius)
			{
				m_TouchPressedOutside = true;
			}
		}
		else if(WasTouchPressed)
		{
			m_Active = false;
		}
	}

	const bool ExtraAnimations = g_Config.m_QmExtraAnimations != 0 && GameClient()->UiRuntimeV2()->Enabled();
	float PresentationAlpha = m_Active ? 1.0f : 0.0f;
	float PresentationScale = m_Active ? 1.0f : 0.88f;
	if(ExtraAnimations)
	{
		CUiV2AnimationRuntime &AnimRuntime = GameClient()->UiRuntimeV2()->AnimRuntime();
		const SUiSpringConfig Spring = EmoticonPresentationSpring();
		const uint64_t PanelNode = EmoticonPresentationNodeKey("panel");
		if(!m_PresentationInitialized)
		{
			SetUiPresentationStateValue(AnimRuntime, PanelNode, EUiAnimProperty::ALPHA, 0.0f);
			SetUiPresentationStateValue(AnimRuntime, PanelNode, EUiAnimProperty::SCALE, 0.88f);
			m_PresentationInitialized = true;
		}
		PresentationAlpha = std::clamp(ResolveUiPresentationStateValue(AnimRuntime, PanelNode, EUiAnimProperty::ALPHA, m_Active ? 1.0f : 0.0f, Spring, 3, 0.004f), 0.0f, 1.0f);
		PresentationScale = std::max(0.01f, ResolveUiPresentationStateValue(AnimRuntime, PanelNode, EUiAnimProperty::SCALE, m_Active ? 1.0f : 0.88f, Spring, 3, 0.004f));
	}

	if(!m_Active && (!ExtraAnimations || PresentationAlpha <= 0.01f))
		return;

	if(m_Active)
	{
		if(length(m_SelectorMouse) > s_OuterMouseLimitRadius)
			m_SelectorMouse = normalize(m_SelectorMouse) * s_OuterMouseLimitRadius;

		const float SelectorAngle = angle(m_SelectorMouse);

		m_SelectedEmote = -1;
		m_SelectedEyeEmote = -1;
		if(length(m_SelectorMouse) > s_InnerOuterMouseBoundaryRadius)
			m_SelectedEmote = PositiveMod(std::round(SelectorAngle / (2.0f * pi) * NUM_EMOTICONS), NUM_EMOTICONS);
		else if(length(m_SelectorMouse) > s_InnerMouseLimitRadius)
			m_SelectedEyeEmote = PositiveMod(std::round(SelectorAngle / (2.0f * pi) * NUM_EMOTES), NUM_EMOTES);
	}

	if(m_Active && m_SelectedEmote != -1)
	{
		if(m_SuperChargeTrackedEmote != m_SelectedEmote)
		{
			m_SuperChargeTrackedEmote = m_SelectedEmote;
			m_SuperChargeSeconds = 0.0f;
		}
		else
			m_SuperChargeSeconds = std::min(s_SuperChargeSecondsRequired, m_SuperChargeSeconds + Client()->RenderFrameTime());
		m_SuperChargeProgress = std::clamp(m_SuperChargeSeconds / s_SuperChargeSecondsRequired, 0.0f, 1.0f);
	}
	else if(m_Active)
	{
		m_SuperChargeTrackedEmote = -1;
		m_SuperChargeSeconds = 0.0f;
		m_SuperChargeProgress = 0.0f;
	}

	if(m_Active && m_SelectedEmote != -1)
	{
		if(m_SuperChargeRingEmote != m_SelectedEmote)
		{
			if(m_SuperChargeRingEmote != -1)
			{
				m_SuperChargeRingExitEmote = m_SuperChargeRingEmote;
				m_SuperChargeRingExitPhase = m_SuperChargeRingPhase;
				m_SuperChargeRingExitCharge = m_SuperChargeRingCharge;
			}
			m_SuperChargeRingEmote = m_SelectedEmote;
			m_SuperChargeRingPhase = 0.0f;
			m_SuperChargeRingCharge = 0.0f;
		}
		m_SuperChargeRingCharge = m_SuperChargeProgress;
	}
	else if(m_Active && m_SuperChargeRingEmote != -1)
	{
		m_SuperChargeRingExitEmote = m_SuperChargeRingEmote;
		m_SuperChargeRingExitPhase = m_SuperChargeRingPhase;
		m_SuperChargeRingExitCharge = m_SuperChargeRingCharge;
		m_SuperChargeRingEmote = -1;
		m_SuperChargeRingPhase = 0.0f;
		m_SuperChargeRingCharge = 0.0f;
	}

	const float RingAnimationStep = Client()->RenderFrameTime() / s_SuperChargeRingAnimationSeconds;
	if(m_SuperChargeRingEmote != -1)
		m_SuperChargeRingPhase = std::min(1.0f, m_SuperChargeRingPhase + RingAnimationStep);
	if(m_SuperChargeRingExitEmote != -1)
	{
		m_SuperChargeRingExitPhase = std::max(0.0f, m_SuperChargeRingExitPhase - RingAnimationStep);
		if(m_SuperChargeRingExitPhase <= 0.0f)
		{
			m_SuperChargeRingExitEmote = -1;
			m_SuperChargeRingExitCharge = 0.0f;
		}
	}

	const vec2 ScreenCenter = Screen.Center();
	const float EmoticonSelectorShadowOpacity = 0.24f * PresentationAlpha;
	const float EmoticonSelectorShadowOffsetX = 2.0f * PresentationScale;
	const float EmoticonSelectorShadowOffsetY = 3.0f * PresentationScale;

	Ui()->MapScreen();

	Graphics()->BlendNormal();

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(ui_token::color::SURFACE_OVERLAY.WithMultipliedAlpha(0.95f * PresentationAlpha));
	Graphics()->DrawCircle(ScreenCenter.x, ScreenCenter.y, s_OuterCircleRadius * PresentationScale, 64);
	Graphics()->SetColor(ui_token::color::ACCENT_PRIMARY_DIM.WithMultipliedAlpha(0.95f * PresentationAlpha));
	Graphics()->DrawCircle(ScreenCenter.x, ScreenCenter.y, s_InnerOuterMouseBoundaryRadius * PresentationScale, 64);
	Graphics()->QuadsEnd();

	Graphics()->WrapClamp();
	for(int Emote = 0; Emote < NUM_EMOTICONS; Emote++)
	{
		float Angle = 2.0f * pi * Emote / NUM_EMOTICONS;
		if(Angle > pi)
			Angle -= 2.0f * pi;

		Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_aSpriteEmoticons[Emote]);
		Graphics()->QuadsSetSubset(0, 0, 1, 1);
		const float Reveal = EmoticonStaggerReveal(Emote, NUM_EMOTICONS, PresentationAlpha);
		const float ItemAlpha = PresentationAlpha * Reveal;
		const float ItemScale = PresentationScale * (0.70f + 0.30f * Reveal);
		const vec2 Nudge = direction(Angle) * s_OuterItemRadius * ItemScale;
		const float HoverPhase = Emote == m_SelectedEmote ? 1.0f : 0.0f;
		const float Size = (50.0f + HoverPhase * 30.0f) * ItemScale;
		if(g_Config.m_QmEmoticonShadow)
		{
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(0.0f, 0.0f, 0.0f, EmoticonSelectorShadowOpacity);
			IGraphics::CQuadItem ShadowQuad(ScreenCenter.x + Nudge.x + EmoticonSelectorShadowOffsetX, ScreenCenter.y + Nudge.y + EmoticonSelectorShadowOffsetY, Size, Size);
			Graphics()->QuadsDraw(&ShadowQuad, 1);
			Graphics()->QuadsEnd();
			Graphics()->TextureSet(GameClient()->m_EmoticonsSkin.m_aSpriteEmoticons[Emote]);
			Graphics()->QuadsSetSubset(0, 0, 1, 1);
		}
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, ItemAlpha);
		IGraphics::CQuadItem QuadItem(ScreenCenter.x + Nudge.x, ScreenCenter.y + Nudge.y, Size, Size);
		Graphics()->QuadsDraw(&QuadItem, 1);
		Graphics()->QuadsEnd();
	}
	Graphics()->WrapNormal();

	auto RenderSuperChargeRing = [&](int Emote, float Phase, float Charge) {
		if(Emote < 0 || Phase <= 0.0f || Charge <= 0.0f)
			return;
		const float Angle = 2.0f * pi * Emote / (float)NUM_EMOTICONS;
		const vec2 Nudge = direction(Angle) * s_OuterItemRadius * PresentationScale;
		const float RingOuterRadius = 47.0f * PresentationScale * Phase;
		const float RingThickness = s_SuperChargeRingThickness * PresentationScale * Phase;
		const ColorRGBA FilledColor = Charge >= 1.0f ?
						      ColorRGBA(1.0f, 0.82f, 0.35f, 0.95f * PresentationAlpha) :
						      ColorRGBA(1.0f, 1.0f, 1.0f, 0.90f * PresentationAlpha);
		RenderChargeRing(Graphics(), ScreenCenter + Nudge, RingOuterRadius, RingThickness, Charge, FilledColor, FilledColor.WithMultipliedAlpha(0.28f), s_SuperChargeRingSegments);
	};
	RenderSuperChargeRing(m_SuperChargeRingExitEmote, m_SuperChargeRingExitPhase, m_SuperChargeRingExitCharge);
	RenderSuperChargeRing(m_SuperChargeRingEmote, m_SuperChargeRingPhase, m_SuperChargeRingCharge);

	if(GameClient()->m_GameInfo.m_AllowEyeWheel && g_Config.m_ClEyeWheel && GameClient()->m_aLocalIds[g_Config.m_ClDummy] >= 0)
	{
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(ui_token::color::SURFACE_HIGHLIGHT.WithMultipliedAlpha(2.0f * PresentationAlpha));
		Graphics()->DrawCircle(ScreenCenter.x, ScreenCenter.y, s_InnerCircleRadius * PresentationScale, 64);
		Graphics()->QuadsEnd();

		CTeeRenderInfo TeeInfo = GameClient()->m_aClients[GameClient()->m_aLocalIds[g_Config.m_ClDummy]].m_RenderInfo;

		for(int Emote = 0; Emote < NUM_EMOTES; Emote++)
		{
			float Angle = 2.0f * pi * Emote / NUM_EMOTES;
			if(Angle > pi)
				Angle -= 2.0f * pi;

			const float Reveal = EmoticonStaggerReveal(Emote, NUM_EMOTES, PresentationAlpha);
			const float ItemAlpha = PresentationAlpha * Reveal;
			const float ItemScale = PresentationScale * (0.76f + 0.24f * Reveal);
			const vec2 Nudge = direction(Angle) * s_InnerItemRadius * ItemScale;
			const float HoverPhase = Emote == m_SelectedEyeEmote ? 1.0f : 0.0f;
			TeeInfo.m_Size = (48.0f + HoverPhase * 18.0f) * ItemScale;
			RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, Emote, vec2(-1.0f, 0.0f), ScreenCenter + Nudge, ItemAlpha);
		}

		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		Graphics()->SetColor(ui_token::color::SURFACE_ELEVATED.WithMultipliedAlpha(PresentationAlpha));
		Graphics()->DrawCircle(ScreenCenter.x, ScreenCenter.y, 30.0f * PresentationScale, 64);
		Graphics()->QuadsEnd();
	}
	else
	{
		m_SelectedEyeEmote = -1;
	}

	RenderTools()->RenderCursor(ScreenCenter + m_SelectorMouse * PresentationScale, 24.0f * PresentationScale, PresentationAlpha);
}

void CEmoticon::Emote(int Emoticon)
{
	const auto Effect = QmEmoticon::ConsumeEffect(Emoticon, m_LaunchModeActive, m_SuperLaunchPending);
	if(Effect == QmEmoticon::EEffect::INVALID)
		return;
	const bool UseSuperLaunch = Effect == QmEmoticon::EEffect::SUPER_HEAD || Effect == QmEmoticon::EEffect::SUPER_PROJECTILE;

	if(Effect == QmEmoticon::EEffect::SUPER_HEAD)
	{
		m_LocalSuperHeadEmoticon = Emoticon;
		m_LocalSuperHeadExpireTick = Client()->GameTick(g_Config.m_ClDummy) + 2 * Client()->GameTickSpeed();
	}
	else
	{
		m_LocalSuperHeadEmoticon = -1;
		m_LocalSuperHeadExpireTick = -1;
	}

	if(Effect == QmEmoticon::EEffect::PROJECTILE || Effect == QmEmoticon::EEffect::SUPER_PROJECTILE)
	{
		vec2 LaunchPos = GameClient()->m_LocalCharacterPos;
		LaunchPos.y -= 20.0f;
		vec2 Dir = normalize(GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy]);
		if(length(Dir) <= 0.0001f)
			Dir = vec2(1.0f, 0.0f);
		const vec2 Vel = Dir * 1200.0f + vec2(0.0f, -400.0f);
		for(auto &Projectile : m_aProjectiles)
		{
			if(Projectile.m_Active)
				continue;
			Projectile.Init(LaunchPos, Vel, Emoticon, UseSuperLaunch ? s_SuperProjectileScale : 1.0f);
			if(UseSuperLaunch)
				GameClient()->m_Effects.Explosion(LaunchPos, 0.9f);
			else
				GameClient()->m_Effects.HammerHit(LaunchPos, 0.65f, 0.0f);
			break;
		}
	}

	CNetMsg_Cl_Emoticon Msg;
	Msg.m_Emoticon = Emoticon;
	Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL);

	if(g_Config.m_ClDummyCopyMoves)
	{
		CMsgPacker MsgDummy(NETMSGTYPE_CL_EMOTICON, false);
		MsgDummy.AddInt(Emoticon);
		Client()->SendMsg(!g_Config.m_ClDummy, &MsgDummy, MSGFLAG_VITAL);
	}
	const int LocalClientId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
	if(LocalClientId >= 0)
		GameClient()->m_QmClient.SendQmRealtimeEmoticon(Emoticon, LocalClientId, m_LaunchModeActive, UseSuperLaunch);
}

void CEmoticon::SuperEmote(int Emoticon)
{
	m_SuperLaunchPending = true;
	Emote(Emoticon);
}

bool CEmoticon::IsLocalSuperHeadEmoticon(int ClientId, int Emoticon) const
{
	const int LocalClientId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
	if(LocalClientId >= 0 && ClientId == LocalClientId && Emoticon == m_LocalSuperHeadEmoticon &&
		m_LocalSuperHeadExpireTick >= 0 && Client()->GameTick(g_Config.m_ClDummy) <= m_LocalSuperHeadExpireTick)
		return true;
	if(!g_Config.m_QmShowOtherSuperEmotes || ClientId < 0 || ClientId >= MAX_CLIENTS)
		return false;
	return Emoticon == m_aRemoteSuperHeadEmoticons[ClientId] &&
	       m_aRemoteSuperHeadExpireTicks[ClientId] >= 0 &&
	       Client()->GameTick(g_Config.m_ClDummy) <= m_aRemoteSuperHeadExpireTicks[ClientId];
}

void CEmoticon::EyeEmote(int Emote)
{
	char aBuf[32];
	switch(Emote)
	{
	case EMOTE_NORMAL:
		str_format(aBuf, sizeof(aBuf), "/emote normal %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_PAIN:
		str_format(aBuf, sizeof(aBuf), "/emote pain %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_HAPPY:
		str_format(aBuf, sizeof(aBuf), "/emote happy %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_SURPRISE:
		str_format(aBuf, sizeof(aBuf), "/emote surprise %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_ANGRY:
		str_format(aBuf, sizeof(aBuf), "/emote angry %d", g_Config.m_ClEyeDuration);
		break;
	case EMOTE_BLINK:
		str_format(aBuf, sizeof(aBuf), "/emote blink %d", g_Config.m_ClEyeDuration);
		break;
	}
	GameClient()->m_Chat.SendChat(0, aBuf);
}

void CEmoticon::TriggerLocalBlink()
{
	const int Dummy = g_Config.m_ClDummy;
	const int ClientId = GameClient()->m_aLocalIds[Dummy];
	if(Client()->State() != IClient::STATE_ONLINE || ClientId < 0 || !GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
		return;

	m_aLocalBlinkStates[Dummy].Trigger(Client()->GameTick(Dummy));
}

bool CEmoticon::ShouldRenderLocalBlink(int ClientId) const
{
	for(int Dummy = 0; Dummy < NUM_DUMMIES; ++Dummy)
	{
		if(GameClient()->m_aLocalIds[Dummy] == ClientId && m_aLocalBlinkStates[Dummy].IsActive(Client()->GameTick(Dummy)))
			return true;
	}
	return false;
}
