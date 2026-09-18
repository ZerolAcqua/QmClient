// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "modes.h"

#include <base/str.h>

#include <generated/protocol.h>

#include <algorithm>

static bool QmTextContainsNoCase(const char *pText, const char *pNeedle)
{
	return pText && pText[0] != '\0' && pNeedle && pNeedle[0] != '\0' && str_find_nocase(pText, pNeedle) != nullptr;
}

static bool QmTextEqualsNoCase(const char *pText, const char *pExpected)
{
	return pText && pExpected && str_comp_nocase(pText, pExpected) == 0;
}

int ApplyQmConfigOverride(SQmConfigOverrideState &State, bool HideActive, int CurrentValue, int HiddenValue, bool &Changed)
{
	Changed = false;
	if(HideActive)
	{
		if(!State.m_WasActive)
		{
			State.m_WasActive = true;
			State.m_SavedValue = CurrentValue;
			State.m_AutoChangedValue = false;
			if(CurrentValue != HiddenValue)
			{
				Changed = true;
				State.m_AutoChangedValue = true;
				return HiddenValue;
			}
		}
		return CurrentValue;
	}
	if(State.m_WasActive)
	{
		State.m_WasActive = false;
		if(State.m_AutoChangedValue && CurrentValue == HiddenValue)
		{
			Changed = true;
			State.m_AutoChangedValue = false;
			return State.m_SavedValue;
		}
		State.m_AutoChangedValue = false;
	}
	else
	{
		State.m_SavedValue = CurrentValue;
	}
	return CurrentValue;
}

bool ApplyQmGoresLinkedConfig(bool GoresActive, bool AutoToggle, bool CurrentValue, bool &Changed)
{
	Changed = false;
	if(!AutoToggle)
		return CurrentValue;
	Changed = CurrentValue != GoresActive;
	return GoresActive;
}

int ApplyQmGoresDummyHammerConfig(bool GoresActive, int CurrentValue, bool &Changed)
{
	Changed = false;
	if(!GoresActive || CurrentValue == 0)
		return CurrentValue;
	Changed = true;
	return 0;
}

int ApplyQmGoresDummyHammerOverride(SQmConfigOverrideState &State, bool GoresActive, bool Disable, int CurrentValue, bool &Changed)
{
	return ApplyQmConfigOverride(State, GoresActive && Disable, CurrentValue, 0, Changed);
}

bool ShouldKeepQmGoresHammerInFreeze(bool GoresCycleActive, bool InFreeze, bool HammerRequested)
{
	return GoresCycleActive && InFreeze && HammerRequested;
}

bool ShouldTriggerQmGoresHammerWakeup(bool GoresCycleActive, bool HammerRequested, bool ExternalHammerWakeup)
{
	return GoresCycleActive && HammerRequested && ExternalHammerWakeup;
}

int QmGoresHammerWakeupFireState(int CurrentFire)
{
	return ((CurrentFire + 1) | 1) & INPUT_STATE_MASK;
}

bool ShouldReleaseQmGoresHammerWakeupFire(bool PendingRelease, int CurrentFire)
{
	return PendingRelease && (CurrentFire & 1) != 0;
}

int QmGoresHammerWakeupReleaseFireState(int CurrentFire)
{
	return ((CurrentFire + 1) & ~1) & INPUT_STATE_MASK;
}

int GoresRestoreWeaponAfterHammer(int PreHammerWeapon, bool HasPreHammerWeapon)
{
	return HasPreHammerWeapon ? PreHammerWeapon : WEAPON_GUN;
}

bool ShouldPulseGoresHammerOnFire(bool GoresCycleActive, bool FireJustPressed, bool CurrentWeaponIsHammer, bool FreezeWakeupActive)
{
	return GoresCycleActive && FireJustPressed && !CurrentWeaponIsHammer && !FreezeWakeupActive;
}

bool ShouldRestoreGoresWeaponAfterHammer(bool CurrentWeaponIsHammer, bool HasPreHammerWeapon)
{
	return CurrentWeaponIsHammer && HasPreHammerWeapon;
}

bool ShouldShowQmHookStrongWeakScope(int Scope, bool Self, bool Strong, bool Weak)
{
	switch(Scope)
	{
	case QM_HOOK_STRONG_WEAK_SCOPE_SELF:
		return Self;
	case QM_HOOK_STRONG_WEAK_SCOPE_OTHERS:
		return !Self;
	case QM_HOOK_STRONG_WEAK_SCOPE_STRONG:
		return Strong;
	case QM_HOOK_STRONG_WEAK_SCOPE_WEAK:
		return Weak;
	case QM_HOOK_STRONG_WEAK_SCOPE_ALL:
		return true;
	default:
		return false;
	}
}

bool ShouldShowQmNameplateName(int Scope, bool IsCurrentChar, bool IsLocalClient)
{
	// 当前操控角色：只有 IsCurrentChar 那一个。
	if(IsCurrentChar)
		return Scope == QM_NAMEPLATE_SHOW_SCOPE_CURRENT || Scope == QM_NAMEPLATE_SHOW_SCOPE_LOCAL || Scope == QM_NAMEPLATE_SHOW_SCOPE_ALL;
	// 本机其他角色（分身）：本机但不是当前操控角色。
	if(IsLocalClient)
		return Scope == QM_NAMEPLATE_SHOW_SCOPE_LOCAL || Scope == QM_NAMEPLATE_SHOW_SCOPE_OTHERS_LOCAL || Scope == QM_NAMEPLATE_SHOW_SCOPE_ALL;
	// 其他玩家：既不是本机、也不是当前操控角色。
	return Scope == QM_NAMEPLATE_SHOW_SCOPE_OTHERS || Scope == QM_NAMEPLATE_SHOW_SCOPE_OTHERS_LOCAL || Scope == QM_NAMEPLATE_SHOW_SCOPE_ALL;
}

static bool ShouldUseQmNameplateTextPlayingScope(int Scope, bool Self, bool Friend)
{
	switch(Scope)
	{
	case QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OFF:
		return false;
	case QM_NAMEPLATE_TEXT_PLAYING_SCOPE_SELF:
		return Self;
	case QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OTHERS:
		return !Self;
	case QM_NAMEPLATE_TEXT_PLAYING_SCOPE_FRIENDS:
		return Friend;
	case QM_NAMEPLATE_TEXT_PLAYING_SCOPE_SELF_FRIENDS:
		return Self || Friend;
	case QM_NAMEPLATE_TEXT_PLAYING_SCOPE_ALL:
		return true;
	default:
		return false;
	}
}

static bool ShouldUseQmNameplateTextSpectateScope(int Scope, bool Friend, bool SpectateTarget)
{
	switch(Scope)
	{
	case QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF:
		return false;
	case QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_TARGET:
		return SpectateTarget;
	case QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OTHERS:
		return !SpectateTarget;
	case QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_FRIENDS:
		return Friend;
	case QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_TARGET_FRIENDS:
		return SpectateTarget || Friend;
	case QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_ALL:
		return true;
	default:
		return false;
	}
}

bool ShouldUseQmNameplateTextEffects(int PlayingScope, int SpectateScope, int DemoMode, int DemoTarget, bool DemoPlayback, bool Spectating, bool Self, bool Friend, bool SpectateTarget, int ClientId)
{
	if(DemoPlayback)
	{
		switch(DemoMode)
		{
		case QM_NAMEPLATE_TEXT_DEMO_MODE_OFF:
			return false;
		case QM_NAMEPLATE_TEXT_DEMO_MODE_SMART:
			return SpectateTarget;
		case QM_NAMEPLATE_TEXT_DEMO_MODE_MANUAL_TARGET:
			return DemoTarget >= 0 && ClientId == DemoTarget;
		case QM_NAMEPLATE_TEXT_DEMO_MODE_MANUAL_SCOPE:
			return ShouldUseQmNameplateTextPlayingScope(PlayingScope, Self, Friend);
		default:
			return false;
		}
	}
	if(Spectating)
		return ShouldUseQmNameplateTextSpectateScope(SpectateScope, Friend, SpectateTarget);
	return ShouldUseQmNameplateTextPlayingScope(PlayingScope, Self, Friend);
}

bool ShouldHideGoresGuide(bool GoresEnabled, bool HideGuidesEnabled, bool ManualGuideVisible)
{
	return GoresEnabled && HideGuidesEnabled && !ManualGuideVisible;
}

bool ShouldRenderGoresDebugRoute(bool Online, bool DebugRouteEnabled, bool GoresMapProgressEnabled)
{
	return Online && DebugRouteEnabled && GoresMapProgressEnabled;
}

bool ShouldEnableQmMovingWaterTiles(const char *pGameInfoGameType, const char *pServerInfoGameType, const char *pCommunityId, const char *pCommunityName)
{
	return QmTextContainsNoCase(pGameInfoGameType, "gores") ||
	       QmTextContainsNoCase(pServerInfoGameType, "gores") ||
	       QmTextContainsNoCase(pCommunityId, "axiom") ||
	       QmTextContainsNoCase(pCommunityName, "axiom");
}

bool ShouldUseServerControlledLocalSkin(const char *pGameInfoGameType, const char *pServerInfoGameType, const char *pCommunityId, const char *pCommunityName)
{
	const bool UseTeeMenuSkin =
		QmTextEqualsNoCase(pGameInfoGameType, "DDRaceNetwork") ||
		QmTextEqualsNoCase(pGameInfoGameType, "DDNet") ||
		QmTextEqualsNoCase(pServerInfoGameType, "DDRaceNetwork") ||
		QmTextEqualsNoCase(pServerInfoGameType, "DDNet") ||
		QmTextContainsNoCase(pCommunityId, "axiom") ||
		QmTextContainsNoCase(pCommunityName, "axiom");
	return !UseTeeMenuSkin;
}

int ResolveLocalSkinConfigIndex(bool DemoPlayback, int ClientId, int MainClientId, int DummyClientId)
{
	// Demo snapshots already contain the recorded appearance and must not use current local skin settings.
	if(DemoPlayback || ClientId < 0)
		return -1;
	if(ClientId == MainClientId)
		return 0;
	if(ClientId == DummyClientId)
		return 1;
	return -1;
}

bool ConsumeQmBudgetedWork(int &Cursor, int Total, int Budget)
{
	if(Cursor >= Total)
		return false;
	if(Budget <= 0)
		return true;
	Cursor = std::min(Total, Cursor + Budget);
	return Cursor < Total;
}

bool ShouldRenderMapProgressBar(bool MapProgressEnabled, int MapProgressStyle, bool PlayerStatsHudEnabled, bool GoresMapProgressEnabled)
{
	return MapProgressEnabled && !(MapProgressStyle != 0 && PlayerStatsHudEnabled) && GoresMapProgressEnabled;
}
