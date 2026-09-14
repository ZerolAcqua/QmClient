#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_MUSIC_HOOK_REGISTRY_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_MUSIC_HOOK_REGISTRY_H

#include <engine/shared/config.h>

#include <cstddef>
#include <cstdint>

// 音乐 Hook 注册表:一个 Hook 一行。未来新增音乐客户端 Hook 时在此追加一行,
// 设置页的互斥开关与「跟随启动应用」的自动切换都会自动覆盖新条目。
// 注意:设置页文案需要像现有条目一样在 translations/i18n 源文件中登记
// (extract_strings 只扫描字面量 Localize 调用)。
struct SQmMusicHookEntry
{
	// 启用开关配置项指针(0=关闭,非 0=启用)。
	int *m_pEnableConfig;
	// 设置页按钮 id 与显示文案(显示文案为英文源串,运行时经 Localize 翻译)。
	const char *m_pSettingsTextId;
	const char *m_pSettingsText;
	// Windows 下目标音乐应用主进程名(如 cloudmusic.exe);非 Windows 平台不使用。
	const wchar_t *m_pProcessName;
};

// 用一条快照记录匹配所有 Hook；结果可按位合并，重复子进程不会重复计数。
inline uint64_t QmMusicHookMaskForProcess(const wchar_t *pProcessName, const SQmMusicHookEntry *pHooks, size_t Count)
{
	// 进程名大小写不敏感比较。
	const auto ProcessNameEquals = [](const wchar_t *pLeft, const wchar_t *pRight) {
		for(;;)
		{
			const wchar_t A = *pLeft++;
			const wchar_t B = *pRight++;
			const wchar_t LowerA = (A >= L'A' && A <= L'Z') ? (wchar_t)(A - L'A' + L'a') : A;
			const wchar_t LowerB = (B >= L'A' && B <= L'Z') ? (wchar_t)(B - L'A' + L'a') : B;
			if(LowerA != LowerB)
				return false;
			if(LowerA == L'\0')
				return true;
		}
	};
	uint64_t Mask = 0;
	for(size_t i = 0; i < Count; ++i)
	{
		if(pHooks[i].m_pProcessName != nullptr && ProcessNameEquals(pProcessName, pHooks[i].m_pProcessName))
			Mask |= uint64_t(1) << i;
	}
	return Mask;
}

inline const SQmMusicHookEntry *QmMusicHookRegistry(size_t *pCount)
{
	static const SQmMusicHookEntry aEntries[] = {
		{&g_Config.m_QmNeteaseHookEnable, "Enable Netease music Hook", "Enable Netease music Hook", L"cloudmusic.exe"},
		{&g_Config.m_QmSodaHookEnable, "Enable SodaMusic Hook", "Enable SodaMusic Hook", L"SodaMusic.exe"},
		{&g_Config.m_QmSpotifyEnable, "Enable Spotify lyrics", "Enable Spotify lyrics", L"Spotify.exe"},
	};
	if(pCount != nullptr)
		*pCount = sizeof(aEntries) / sizeof(aEntries[0]);
	return aEntries;
}

#endif
