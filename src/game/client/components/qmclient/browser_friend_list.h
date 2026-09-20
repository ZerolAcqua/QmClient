#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_BROWSER_FRIEND_LIST_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_BROWSER_FRIEND_LIST_H

#include <base/math.h>
#include <base/system.h>

#include <engine/friends.h>
#include <engine/serverbrowser.h>

#include <algorithm>
#include <vector>

// 好友列表按数据版本重建；每帧绘制和自动跟随仍读取当前结果。
class CQmBrowserFriendList
{
public:
	class CItem
	{
		char m_aName[MAX_NAME_LENGTH];
		char m_aClan[MAX_CLAN_LENGTH];
		char m_aCategory[IFriends::MAX_FRIEND_CATEGORY_LENGTH];
		const CServerInfo *m_pServerInfo;
		int m_FriendState;
		bool m_IsPlayer;
		bool m_IsAfk;
		// 0.6 皮肤信息
		char m_aSkin[MAX_SKIN_LENGTH];
		bool m_CustomSkinColors;
		int m_CustomSkinColorBody;
		int m_CustomSkinColorFeet;
		// 0.7 皮肤信息
		char m_aaSkin7[protocol7::NUM_SKINPARTS][protocol7::MAX_SKIN_LENGTH];
		bool m_aUseCustomSkinColor7[protocol7::NUM_SKINPARTS];
		int m_aCustomSkinColor7[protocol7::NUM_SKINPARTS];

	public:
		CItem(const CFriendInfo *pFriendInfo) :
			m_pServerInfo(nullptr),
			m_IsPlayer(false),
			m_IsAfk(false),
			m_CustomSkinColors(false),
			m_CustomSkinColorBody(0),
			m_CustomSkinColorFeet(0)
		{
			str_copy(m_aName, pFriendInfo->m_aName);
			str_copy(m_aClan, pFriendInfo->m_aClan);
			str_copy(m_aCategory, pFriendInfo->m_aCategory[0] != '\0' ? pFriendInfo->m_aCategory : IFriends::DEFAULT_CATEGORY);
			m_FriendState = m_aName[0] == '\0' ? IFriends::FRIEND_CLAN : IFriends::FRIEND_PLAYER;
			m_aSkin[0] = '\0';
			for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
			{
				m_aaSkin7[Part][0] = '\0';
				m_aUseCustomSkinColor7[Part] = false;
				m_aCustomSkinColor7[Part] = 0;
			}
		}
		CItem(const CServerInfo::CClient &CurrentClient, const CServerInfo *pServerInfo, const char *pCategory) :
			m_pServerInfo(pServerInfo),
			m_FriendState(CurrentClient.m_FriendState),
			m_IsPlayer(CurrentClient.m_Player),
			m_IsAfk(CurrentClient.m_Afk),
			m_CustomSkinColors(CurrentClient.m_CustomSkinColors),
			m_CustomSkinColorBody(CurrentClient.m_CustomSkinColorBody),
			m_CustomSkinColorFeet(CurrentClient.m_CustomSkinColorFeet)
		{
			str_copy(m_aName, CurrentClient.m_aName);
			str_copy(m_aClan, CurrentClient.m_aClan);
			str_copy(m_aCategory, pCategory != nullptr && pCategory[0] != '\0' ? pCategory : IFriends::DEFAULT_CATEGORY);
			str_copy(m_aSkin, CurrentClient.m_aSkin);
			for(int Part = 0; Part < protocol7::NUM_SKINPARTS; Part++)
			{
				str_copy(m_aaSkin7[Part], CurrentClient.m_aaSkin7[Part]);
				m_aUseCustomSkinColor7[Part] = CurrentClient.m_aUseCustomSkinColor7[Part];
				m_aCustomSkinColor7[Part] = CurrentClient.m_aCustomSkinColor7[Part];
			}
		}

		const char *Name() const { return m_aName; }
		const char *Clan() const { return m_aClan; }
		const char *Category() const { return m_aCategory; }
		const CServerInfo *ServerInfo() const { return m_pServerInfo; }
		int FriendState() const { return m_FriendState; }
		bool IsPlayer() const { return m_IsPlayer; }
		bool IsAfk() const { return m_IsAfk; }
		// 0.6 皮肤
		const char *Skin() const { return m_aSkin; }
		bool CustomSkinColors() const { return m_CustomSkinColors; }
		int CustomSkinColorBody() const { return m_CustomSkinColorBody; }
		int CustomSkinColorFeet() const { return m_CustomSkinColorFeet; }
		// 0.7 皮肤
		const char *Skin7(int Part) const { return m_aaSkin7[Part]; }
		bool UseCustomSkinColor7(int Part) const { return m_aUseCustomSkinColor7[Part]; }
		int CustomSkinColor7(int Part) const { return m_aCustomSkinColor7[Part]; }

		const void *ListItemId() const { return &m_aName; }
		const void *RemoveButtonId() const { return &m_FriendState; }
		const void *CommunityTooltipId() const { return &m_IsPlayer; }
		const void *SkinTooltipId() const { return &m_aSkin; }

		bool operator<(const CItem &Other) const
		{
			const int Result = str_comp_nocase(m_aName, Other.m_aName);
			return Result < 0 || (Result == 0 && str_comp_nocase(m_aClan, Other.m_aClan) < 0);
		}
	};

private:
	std::vector<std::vector<CItem>> m_vvFriends;
	const IFriends *m_pFriends = nullptr;
	const IServerBrowser *m_pBrowser = nullptr;
	uint64_t m_FriendsRevision = 0;
	uint64_t m_BrowserRevision = 0;
	bool m_IgnoreClan = false;

public:
	const std::vector<std::vector<CItem>> &Groups() const { return m_vvFriends; }

	bool Update(const IFriends &Friends, const IServerBrowser &Browser, bool IgnoreClan)
	{
		const uint64_t FriendsRevision = Friends.Revision();
		const uint64_t BrowserRevision = Browser.FriendListRevision();
		if(m_pFriends == &Friends && m_pBrowser == &Browser && m_FriendsRevision == FriendsRevision && m_BrowserRevision == BrowserRevision && m_IgnoreClan == IgnoreClan)
			return false;

		const int NumCategories = maximum(1, Friends.NumCategories());
		m_vvFriends.resize(NumCategories);
		for(auto &vFriends : m_vvFriends)
			vFriends.clear();
		const int OfflineCategoryIndex = maximum(0, Friends.FindCategory(IFriends::OFFLINE_CATEGORY));
		for(int FriendIndex = 0; FriendIndex < Friends.NumFriends(); ++FriendIndex)
		{
			const CFriendInfo *pFriendInfo = Friends.GetFriend(FriendIndex);
			if(pFriendInfo->m_aName[0] == '\0')
				continue;

			m_vvFriends[OfflineCategoryIndex].emplace_back(pFriendInfo);
		}

		for(int ServerIndex = 0; ServerIndex < Browser.NumServers(); ++ServerIndex)
		{
			const CServerInfo *pEntry = Browser.Get(ServerIndex);
			if(pEntry->m_FriendState == IFriends::FRIEND_NO)
				continue;

			for(int ClientIndex = 0; ClientIndex < pEntry->m_NumClients; ++ClientIndex)
			{
				const CServerInfo::CClient &CurrentClient = pEntry->m_aClients[ClientIndex];
				if(CurrentClient.m_FriendState == IFriends::FRIEND_NO)
					continue;

				const bool ClanOnlyMatch = CurrentClient.m_FriendState == IFriends::FRIEND_CLAN;
				const char *pCategory = ClanOnlyMatch ? IFriends::CLAN_MEMBERS_CATEGORY : Friends.GetFriendCategory(CurrentClient.m_aName, CurrentClient.m_aClan);
				if(!ClanOnlyMatch && (pCategory != nullptr && str_comp_nocase(pCategory, IFriends::OFFLINE_CATEGORY) == 0))
					pCategory = Friends.DefaultCategory();

				int CategoryIndex = Friends.FindCategory(pCategory);
				if(CategoryIndex < 0 || CategoryIndex >= NumCategories)
					CategoryIndex = 0;

				m_vvFriends[CategoryIndex].emplace_back(CurrentClient, pEntry, pCategory);

				if(!ClanOnlyMatch)
				{
					auto &vOfflineFriends = m_vvFriends[OfflineCategoryIndex];
					vOfflineFriends.erase(std::remove_if(vOfflineFriends.begin(), vOfflineFriends.end(), [&](const CItem &Friend) {
						return Friend.ServerInfo() == nullptr && Friend.Name()[0] != '\0' && str_comp(Friend.Name(), CurrentClient.m_aName) == 0 && (IgnoreClan || str_comp(Friend.Clan(), CurrentClient.m_aClan) == 0);
					}),
						vOfflineFriends.end());
				}
			}
		}
		for(auto &vFriends : m_vvFriends)
		{
			std::sort(vFriends.begin(), vFriends.end(), [](const CItem &Left, const CItem &Right) {
				const bool LeftOnline = Left.ServerInfo() != nullptr;
				const bool RightOnline = Right.ServerInfo() != nullptr;
				if(LeftOnline != RightOnline)
					return LeftOnline;
				return Left < Right;
			});
		}

		m_pFriends = &Friends;
		m_pBrowser = &Browser;
		m_FriendsRevision = FriendsRevision;
		m_BrowserRevision = BrowserRevision;
		m_IgnoreClan = IgnoreClan;
		return true;
	}
};

#endif
