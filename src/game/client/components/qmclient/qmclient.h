// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QMCLIENT_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QMCLIENT_H

#include "qm_realtime.h"
#include "qmclient_utils.h"

#include <base/hash.h>

#include <engine/shared/http.h>
#include <engine/shared/protocol.h>
#include <engine/shared/websocket_client.h>

#include <game/client/component.h>

#include <memory>
#include <mutex>
#include <deque>
#include <string>
#include <vector>

class IJob;
typedef struct _json_value json_value;

class CQmClient : public CComponent
{
public:
	struct SQmRemoteEmoticonEvent
	{
		int m_PlayerId = -1;
		int m_Emoticon = -1;
		bool m_LaunchMode = false;
		bool m_SuperLaunch = false;
		uint64_t m_Sequence = 0;
		std::string m_ClientId;
		std::string m_PlayerName;
		std::string m_ServerAddress;
	};
	// 「新功能」弹窗状态：内容来自中心服广播，本地只保留最近一次成功结果。
	enum class EQmNewsStatus
	{
		IDLE = 0,
		LOADING,
		READY,
		EMPTY,
		FAILED,
		PUBLISHING,
		PUBLISHED,
		PUBLISH_DENIED,
		PUBLISH_TOO_LARGE,
		PUBLISH_FAILED,
	};

private:
	std::shared_ptr<CHttpRequest> m_pTitleOperation;
	char m_aTitleToken[65] = "";
	char m_aTitleText[64] = "";
	char m_aTitleBoundName[64] = "";
	// 账号上已保存的自选头衔风格 id（空表示未自选，由服务端每局派生）。
	char m_aTitleProfileStyle[64] = "";
	char m_aaPlayerTitles[MAX_CLIENTS][64] = {};
	// 服务端分配的头衔动态风格 id，与 m_aaPlayerTitles 同时刷新。
	char m_aaPlayerStyles[MAX_CLIENTS][64] = {};
	// 服务端时间与本地 GlobalTime 的偏移（秒），用于让所有客户端的动画相位对齐。
	double m_ServerTimeOffset = 0.0;
	bool m_ServerTimeOffsetValid = false;
	char m_aaTitleNames[MAX_CLIENTS][MAX_NAME_LENGTH] = {};
	int64_t m_aTitleExpires[MAX_CLIENTS] = {};
	int64_t m_TitleLastSync = 0;
	bool m_TitleAuthenticated = false;
	int m_TitleRevision = 0;
	const char *m_pTitleStatus = "Enter your sponsor code";
	void InitTitleAuthentication();
	void UpdateTitleAuthentication();
	void ResetTitlePresences();
	void StartTitleRequest(const char *pPath, const char *pBody, std::shared_ptr<CHttpRequest> &pTask);

	std::shared_ptr<IJob> m_pQmClientUsersParseJob = nullptr;
	std::shared_ptr<IJob> m_pQmClientLifecycleMarkerWriteJob = nullptr;
	std::shared_ptr<std::mutex> m_pQmClientLifecycleMarkerMutex = std::make_shared<std::mutex>();
	std::shared_ptr<CHttpRequest> m_pQmDdnetPlayerTask = nullptr;
	std::shared_ptr<IJob> m_pQmDdnetPlayerParseJob = nullptr;
	std::shared_ptr<CHttpRequest> m_pQmNewsPublishTask = nullptr;

	char m_aQmClientMachineHash[SHA256_MAXSTRSIZE] = "";
	char m_aQmClientLifecycleSessionId[64] = "";
	char m_aQmClientPlaytimeClientId[65] = "";
	char m_aQmDdnetPlayerName[MAX_NAME_LENGTH] = "";
	char m_aQmDdnetFavoritePartner[MAX_NAME_LENGTH] = "";
	char m_aQmDeveloperToken[65] = "";
	char m_aQmDeveloperSessionId[33] = "";

	// 「新功能」广播：远端 Markdown + 本地缓存 + 开发者草稿。
	std::string m_QmNewsMarkdown;
	std::string m_QmNewsDraft;
	EQmNewsStatus m_QmNewsStatus = EQmNewsStatus::IDLE;
	int m_QmNewsVersion = 0;
	int m_QmNewsRevision = 0;
	bool m_QmNewsPublishing = false;
	void InitQmNews();
	void LoadQmNewsCache();
	void SaveQmNewsCache();
	void ApplyQmNewsPayload(const char *pBody, size_t BodySize);
	void FinishQmNewsPublish();

	// 自有服务专用 WS 通道；断线只重连，不回退 HTTP。
	std::unique_ptr<IQmWebSocketClient> m_pQmRealtime;
	char m_aQmRealtimeUrl[256] = "";
	bool m_QmRealtimeFailureLogged = false;
	// 已向服务端同步过的头衔资料版本，用于触发一次订阅刷新。
	int m_QmRealtimeTitleRevision = 0;
	void StartQmRealtime();
	void StopQmRealtime();
	void UpdateQmRealtime();
	void EnsureQmRealtimeConnection();
	void SendQmRealtimeHello();
	void SendQmRealtimeStop();
	std::string BuildQmRealtimePresence(bool Hello) const;
	void ApplyQmRealtimeServices(const SQmRealtimeMessage &Message);
	void ApplyQmRealtimeTitleProfile(const json_value *pPayload);
	int64_t m_QmRealtimeConnectedTick = 0;
	int64_t m_QmRealtimeLastPresence = 0;
	int64_t m_QmRealtimeNextPresenceCheck = 0;
	std::string m_QmRealtimePresenceBody;
	std::shared_ptr<const json_value> m_pQmRealtimeUsersPayload;
	char m_aQmRealtimeUsersServer[NETADDR_MAXSTRSIZE] = "";
	int64_t m_QmRealtimeUsersExpireTick = 0;
	std::deque<SQmRemoteEmoticonEvent> m_QmRemoteEmoticonEvents;
	void HandleQmRealtimeMessage(const SQmRealtimeMessage &Message);
	void ApplyQmRealtimeState(const SQmRealtimeMessage &Message);
	void ApplyQmRealtimeBroadcast(const SQmRealtimeMessage &Message);
	void ApplyQmRealtimeTitles(const SQmRealtimeMessage &Message);
	// 头衔名单的唯一落地入口，复用已有身份校验与租约规则。
	void ApplyQmTitlePresences(const json_value *pRoot, const char *pServerAddress);
	void LogQmRealtimeEvent(const char *pStage, const char *pDetail) const;

	int64_t m_QmClientServerNow = 0;
	int64_t m_QmClientServerSessionStart = 0;
	int64_t m_QmClientServerTimeLastSync = 0;
	int64_t m_QmClientServerPlaytimeSeconds = -1;
	int64_t m_QmClientPlaytimeLastSync = 0;
	int64_t m_QmClientRecoveryStopAt = 0;
	int64_t m_QmClientMarkerStartedAt = 0;
	int64_t m_QmClientMarkerLastSeenAt = 0;
	int64_t m_QmClientMarkerLastFlushTick = 0;
	int64_t m_QmDdnetPlayerLastSync = 0;
	int64_t m_QmDdnetPlayerNextRetry = 0;
	int m_QmClientOnlineUserCount = 0;
	int m_QmClientOnlineDummyCount = 0;
	int m_QmDdnetTotalFinishes = -1;
	bool m_QmClientDistributionSuccessLatched = false;
	bool m_QmClientShutdownReported = false;
	bool m_QmClientAwaitingRecoveryStop = false;
	bool m_QmClientStartupSent = false;
	std::vector<SQmClientServerDistribution> m_vQmClientServerDistribution;

	void InitQmClientLifecycle();
	void UpdateQmClientLifecycleAndServerTime();
	void EnsureQmClientPlaytimeClientId();
	bool ReadQmClientLifecycleMarker(int64_t &OutStartedAt, int64_t &OutLastSeenAt);
	void TouchQmClientLifecycleMarker(bool ForceWrite);
	void WriteQmClientLifecycleMarker();
	void ClearQmClientLifecycleMarker();

	void UpdateQmClientRecognition();
	void FinishQmClientUsers();
	bool EnsureQmClientMachineHash();
	void ClearQmClientServerDistribution();
	void InitQmDeveloperAuthentication();
	void ApplyQmRealtimeDevelopers(const json_value *pPayload);

	void UpdateQmDdnetPlayerStats();
	void FetchQmDdnetPlayerStats(const char *pPlayerName);
	void FinishQmDdnetPlayerStats();

public:
	void RedeemTitleCode(const char *pCode);
	void SaveTitleProfile(const char *pTitle, const char *pBoundName, const char *pStyle);
	void RefreshTitleProfile();
	bool TitleBusy() const { return m_pTitleOperation != nullptr; }
	bool TitleAuthenticated() const { return m_TitleAuthenticated; }
	const char *TitleStatus() const { return m_pTitleStatus; }
	const char *TitleText() const { return m_aTitleText; }
	const char *TitleBoundName() const { return m_aTitleBoundName; }
	const char *TitleProfileStyle() const { return m_aTitleProfileStyle; }
	int TitleRevision() const { return m_TitleRevision; }
	const char *PlayerTitle(int ClientId) const;
	// 该玩家头衔的动态风格 id；未分配或已过期时返回空串。
	const char *PlayerTitleStyle(int ClientId) const;
	// 头衔动画的相位基准（秒，已对齐服务端时间并取模）。服务端不可用时退回本地时间。
	double TitleAnimationTime() const;
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnShutdown() override;
	void OnUpdate() override;
	void OnStateChange(int NewState, int OldState) override;

	bool HasQmClientRecognitionService() const;
	bool HasQmServerTime() const { return m_QmClientServerNow > 0; }
	int64_t QmServerTimeNow() const { return m_QmClientServerNow; }
	int64_t QmServerSessionStartTime() const { return m_QmClientServerSessionStart; }
	bool HasQmServerPlaytime() const { return m_QmClientServerPlaytimeSeconds >= 0; }
	int64_t QmServerPlaytimeSeconds() const { return m_QmClientServerPlaytimeSeconds; }
	const std::vector<SQmClientServerDistribution> &QmClientServerDistribution() const { return m_vQmClientServerDistribution; }
	int QmClientOnlineUserCount() const { return m_QmClientOnlineUserCount; }
	int QmClientOnlineDummyCount() const { return m_QmClientOnlineDummyCount; }
	int QmDdnetTotalFinishes() const { return m_QmDdnetTotalFinishes; }
	const char *QmDdnetFavoritePartner() const { return m_aQmDdnetFavoritePartner; }

	// 「新功能」弹窗：内容全部来自中心服广播，本地只保留最近一次成功结果。
	bool HasDeveloperCredential() const { return m_aQmDeveloperToken[0] != '\0'; }
	const char *QmNewsMarkdown() const { return m_QmNewsMarkdown.c_str(); }
	const char *QmNewsDraft() const { return m_QmNewsDraft.c_str(); }
	EQmNewsStatus QmNewsStatus() const { return m_QmNewsStatus; }
	int QmNewsVersion() const { return m_QmNewsVersion; }
	int QmNewsRevision() const { return m_QmNewsRevision; }
	bool QmNewsPublishing() const { return m_QmNewsPublishing; }
	void QmNewsRefresh(bool Force);
	void QmNewsPublishDraft();
	void QmNewsReloadDraft();

	// 实时通道状态（供设置界面与诊断使用）。
	bool QmRealtimeAvailable() const { return m_pQmRealtime != nullptr && m_pQmRealtime->Available(); }
	bool QmRealtimeConnected() const { return m_pQmRealtime != nullptr && m_pQmRealtime->State() == EQmWebSocketState::CONNECTED; }
	const char *QmClientId() const { return m_aQmClientPlaytimeClientId; }
	const char *QmRealtimeStateName() const { return m_pQmRealtime != nullptr ? m_pQmRealtime->StateName() : "off"; }
	const char *QmRealtimeLastError() const { return m_pQmRealtime != nullptr ? m_pQmRealtime->LastError() : ""; }
	int QmRealtimePingRttMs() const { return m_pQmRealtime != nullptr ? m_pQmRealtime->LastPingRttMs() : -1; }
	int64_t QmRealtimeReconnectCount() const { return m_pQmRealtime != nullptr ? m_pQmRealtime->ReconnectCount() : 0; }
	// 强制重连（设置界面在地址变更后调用）。
	void QmRealtimeRestart();
	// 请求服务端重推当前服务器的头衔名单（兑换/保存头衔、发现服务端变更后调用）。
	void QmRealtimeRequestTitleRefresh();
	void SendQmRealtimeEmoticon(int Emoticon, int PlayerId, bool LaunchMode, bool SuperLaunch);
	bool PollQmRemoteEmoticonEvent(SQmRemoteEmoticonEvent &OutEvent);
};

#endif
