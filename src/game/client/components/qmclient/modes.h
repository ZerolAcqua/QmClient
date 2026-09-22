// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_MODES_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_MODES_H

struct SQmConfigOverrideState
{
	bool m_WasActive = false;
	int m_SavedValue = 0;
	bool m_AutoChangedValue = false;
};

// 禅模式（Focus / Zen Mode）决策输入与输出：主开关 + 各子开关推导出的渲染/静音结果。
struct SQmAirJumpEffectDecision
{
	bool m_SpawnParticles = false;
	bool m_PlaySound = false;
};

struct SQmFocusModeConfig
{
	bool m_FocusActive = false;
	bool m_HideJumpEffects = false;
	bool m_HideKillEffects = false;
	bool m_HideExplosionEffects = false;
	bool m_HideFreezeEffects = false;
	bool m_HideHammerEffects = false;
	bool m_HideMuzzleEffects = false;
	bool m_MuteJumpSounds = false;
	bool m_MuteDeathSounds = false;
	bool m_MuteHammerSounds = false;
	bool m_SoundEnabled = true;
	bool m_HideMapProgress = false;
	bool m_MapProgressEnabled = false;
	int m_MapProgressStyle = 0;
	bool m_PlayerStatsHudEnabled = false;
	bool m_GoresMapProgressEnabled = false;
	bool m_HideHud = false;
	bool m_HideScoreboard = false;
	bool m_HideNames = false;
	bool m_HideNameplates = false;
	bool m_HideInfoMessages = false;
	bool m_HideDirectionIndicators = false;
	bool m_HideGuideLines = false;
	bool m_HidePlayerMessages = false;
	bool m_HideSystemInfoMessages = false;
	bool m_HideSystemPromptMessages = false;
	bool m_HideEchoMessages = false;
};

struct SQmFocusModeDecisions
{
	SQmAirJumpEffectDecision m_AirJump;
	bool m_HideKillEffects = false;
	bool m_HideExplosionEffects = false;
	bool m_HideFreezeEffects = false;
	bool m_HideHammerEffects = false;
	bool m_HideMuzzleEffects = false;
	bool m_MuteDeathSounds = false;
	bool m_MuteHammerSounds = false;
	bool m_RenderMapProgressBar = false;
	bool m_HideHud = false;
	bool m_HideScoreboard = false;
	bool m_HideNames = false;
	bool m_HideNameplates = false;
	bool m_HideInfoMessages = false;
	bool m_HideDirectionIndicators = false;
	bool m_HideGuideLines = false;
	bool m_HidePlayerMessages = false;
	bool m_HideSystemInfoMessages = false;
	bool m_HideSystemPromptMessages = false;
	bool m_HideEchoMessages = false;
};

enum EQmHookStrongWeakScope
{
	QM_HOOK_STRONG_WEAK_SCOPE_SELF = 0,
	QM_HOOK_STRONG_WEAK_SCOPE_OTHERS = 1,
	QM_HOOK_STRONG_WEAK_SCOPE_STRONG = 2,
	QM_HOOK_STRONG_WEAK_SCOPE_WEAK = 3,
	QM_HOOK_STRONG_WEAK_SCOPE_ALL = 4,
};

// 昵称显示范围：把玩家分成三类 —— 当前操控角色、本机其他角色（分身）、其他玩家，
// 六档就是这三类可见组合的枚举，档位之间互斥。不再按好友过滤（好友标记仍走 cl_nameplates_friendmark）。
enum EQmNameplateShowScope
{
	QM_NAMEPLATE_SHOW_SCOPE_OFF = 0, // 无：谁都不看
	QM_NAMEPLATE_SHOW_SCOPE_CURRENT = 1, // 当前：只看当前操控的角色
	QM_NAMEPLATE_SHOW_SCOPE_LOCAL = 2, // 当前 + 本地：看自己（当前角色与分身）
	QM_NAMEPLATE_SHOW_SCOPE_OTHERS = 3, // 他人：只看其他玩家
	QM_NAMEPLATE_SHOW_SCOPE_OTHERS_LOCAL = 4, // 本地 + 他人：只看非当前操控的角色
	QM_NAMEPLATE_SHOW_SCOPE_ALL = 5, // 全体：都看
};

// 档位个数：设置页的分段行、行高与配置范围都以它为准，避免多处各写一个 6。
enum
{
	QM_NAMEPLATE_SHOW_SCOPE_COUNT = 6,
};

enum EQmNameplateTextPlayingScope
{
	QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OFF = 0,
	QM_NAMEPLATE_TEXT_PLAYING_SCOPE_SELF = 1,
	QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OTHERS = 2,
	QM_NAMEPLATE_TEXT_PLAYING_SCOPE_FRIENDS = 3,
	QM_NAMEPLATE_TEXT_PLAYING_SCOPE_SELF_FRIENDS = 4,
	QM_NAMEPLATE_TEXT_PLAYING_SCOPE_ALL = 5,
};

enum EQmNameplateTextSpectateScope
{
	QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF = 0,
	QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_TARGET = 1,
	QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OTHERS = 2,
	QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_FRIENDS = 3,
	QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_TARGET_FRIENDS = 4,
	QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_ALL = 5,
};

enum EQmNameplateTextDemoMode
{
	QM_NAMEPLATE_TEXT_DEMO_MODE_OFF = 0,
	QM_NAMEPLATE_TEXT_DEMO_MODE_SMART = 1,
	QM_NAMEPLATE_TEXT_DEMO_MODE_MANUAL_TARGET = 2,
	QM_NAMEPLATE_TEXT_DEMO_MODE_MANUAL_SCOPE = 3,
};

int ApplyQmConfigOverride(SQmConfigOverrideState &State, bool HideActive, int CurrentValue, int HiddenValue, bool &Changed);
bool ApplyQmGoresLinkedConfig(bool GoresActive, bool AutoToggle, bool CurrentValue, bool &Changed);
int ApplyQmGoresDummyHammerConfig(bool GoresActive, int CurrentValue, bool &Changed);
int ApplyQmGoresDummyHammerOverride(SQmConfigOverrideState &State, bool GoresActive, bool Disable, int CurrentValue, bool &Changed);
bool ShouldKeepQmGoresHammerInFreeze(bool GoresCycleActive, bool InFreeze, bool HammerRequested);
bool ShouldTriggerQmGoresHammerWakeup(bool GoresCycleActive, bool HammerRequested, bool ExternalHammerWakeup);
int QmGoresHammerWakeupFireState(int CurrentFire);
bool ShouldReleaseQmGoresHammerWakeupFire(bool PendingRelease, int CurrentFire);
int QmGoresHammerWakeupReleaseFireState(int CurrentFire);
int GoresRestoreWeaponAfterHammer(int PreHammerWeapon, bool HasPreHammerWeapon);
bool ShouldPulseGoresHammerOnFire(bool GoresCycleActive, bool FireJustPressed, bool CurrentWeaponIsHammer, bool FreezeWakeupActive);
bool ShouldRestoreGoresWeaponAfterHammer(bool CurrentWeaponIsHammer, bool HasPreHammerWeapon);
bool ShouldShowQmHookStrongWeakScope(int Scope, bool Self, bool Strong, bool Weak);
bool ShouldShowQmNameplateName(int Scope, bool IsCurrentChar, bool IsLocalClient);
bool ShouldUseQmNameplateTextEffects(int PlayingScope, int SpectateScope, int DemoMode, int DemoTarget, bool DemoPlayback, bool Spectating, bool Self, bool Friend, bool SpectateTarget, int ClientId);

bool ShouldHideGoresGuide(bool GoresEnabled, bool HideGuidesEnabled, bool ManualGuideVisible);
bool ShouldRenderGoresDebugRoute(bool Online, bool DebugRouteEnabled, bool GoresMapProgressEnabled);
bool ShouldEnableQmMovingWaterTiles(const char *pGameInfoGameType, const char *pServerInfoGameType, const char *pCommunityId, const char *pCommunityName);
bool ShouldUseServerControlledLocalSkin(const char *pGameInfoGameType, const char *pServerInfoGameType, const char *pCommunityId, const char *pCommunityName);
int ResolveLocalSkinConfigIndex(bool DemoPlayback, int ClientId, int MainClientId, int DummyClientId);
bool ConsumeQmBudgetedWork(int &Cursor, int Total, int Budget);

bool ShouldRenderMapProgressBar(bool MapProgressEnabled, int MapProgressStyle, bool PlayerStatsHudEnabled, bool GoresMapProgressEnabled);

// 禅模式：渲染/静音门控纯函数（主开关开启且对应子开关开启时才隐藏/静音）。
bool ShouldHideFocusHud(bool FocusActive, bool HideHud);
bool ShouldRenderFocusSpectatorHud(bool SpectatorActive, bool SpectatorHudEnabled, bool MainHudVisible, bool FocusActive, bool HideHud);
bool ShouldHideFocusScoreboard(bool FocusActive, bool HideScoreboard);
bool ShouldHideFocusNames(bool FocusActive, bool HideNames);
bool ShouldHideFocusNameplates(bool FocusActive, bool HideNameplates);
bool ShouldHideFocusJumpEffects(bool FocusActive, bool HideJumpEffects);
bool ShouldHideFocusKillEffects(bool FocusActive, bool HideKillEffects);
bool ShouldHideFocusExplosionEffects(bool FocusActive, bool HideExplosionEffects);
bool ShouldHideFocusFreezeEffects(bool FocusActive, bool HideFreezeEffects);
bool ShouldHideFocusHammerEffects(bool FocusActive, bool HideHammerEffects);
bool ShouldHideFocusMuzzleEffects(bool FocusActive, bool HideMuzzleEffects);
bool ShouldMuteFocusJumpSounds(bool FocusActive, bool MuteJumpSounds);
bool ShouldMuteFocusDeathSounds(bool FocusActive, bool MuteDeathSounds);
bool ShouldMuteFocusHammerSounds(bool FocusActive, bool MuteHammerSounds);
bool ShouldPlayFocusJumpSound(bool FocusActive, bool MuteJumpSounds, bool SoundEnabled);
bool ShouldPlayFocusDeathOrSpawnSound(bool FocusActive, bool MuteDeathSounds, bool SoundEnabled);
SQmAirJumpEffectDecision GetQmAirJumpEffectDecision(bool FocusActive, bool HideJumpEffects, bool MuteJumpSounds, bool SoundEnabled);
bool ShouldHideFocusMapProgress(bool FocusActive, bool HideMapProgress);
bool ShouldHideFocusInfoMessages(bool FocusActive, bool HideInfoMessages);
bool ShouldHideFocusDirectionIndicators(bool FocusActive, bool HideDirectionIndicators);
bool ShouldHideFocusGuideLines(bool FocusActive, bool HideGuideLines);
bool ShouldRenderFocusFilteredChatLine(bool FocusHidePlayerMessages, bool FocusHideSystemInfoMessages, bool FocusHideSystemPromptMessages, bool FocusHideEcho, int ClientId, bool ForceVisible, bool ServerMessageIsBasicInfo);
bool ShouldRenderAnyFocusFilteredChat(bool FocusHidePlayerMessages, bool FocusHideSystemInfoMessages, bool FocusHideSystemPromptMessages, bool FocusHideEcho, bool HasForceVisibleLine);
SQmFocusModeDecisions GetQmFocusModeDecisions(const SQmFocusModeConfig &Config);

#endif
