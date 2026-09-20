#ifndef ENGINE_CLIENT_SERVERBROWSER_HTTP_PARSE_H
#define ENGINE_CLIENT_SERVERBROWSER_HTTP_PARSE_H

#include <base/system.h>

#include <engine/shared/json.h>
#include <engine/shared/serverinfo.h>

#include <utility>
#include <vector>

// 先构建完整结果，失败时保留调用方的旧列表；解析函数不访问客户端或配置。
inline bool ServerBrowserParseHttpList(json_value *pJson, std::vector<CServerInfo> *pvServers)
{
	if(pJson == nullptr)
		return true;
	std::vector<CServerInfo> vServers;

	const json_value &Json = *pJson;
	const json_value &Servers = Json["servers"];
	if(Servers.type != json_array)
	{
		return true;
	}
	for(unsigned int i = 0; i < Servers.u.array.length; i++)
	{
		const json_value &Server = Servers[i];
		const json_value &Addresses = Server["addresses"];
		const json_value &Info = Server["info"];
		const json_value &Location = Server["location"];
		int ParsedLocation = CServerInfo::LOC_UNKNOWN;
		CServerInfo2 ParsedInfo;
		if(Addresses.type != json_array || (Location.type != json_string && Location.type != json_none))
		{
			return true;
		}
		if(Location.type == json_string)
		{
			if(CServerInfo::ParseLocation(&ParsedLocation, Location))
			{
				return true;
			}
		}
		if(CServerInfo2::FromJson(&ParsedInfo, &Info))
		{
			// 单个服务器字段无效只跳过本项，保持主列表的现有解析规则。
			continue;
		}
		CServerInfo SetInfo = ParsedInfo;
		SetInfo.m_Location = ParsedLocation;
		SetInfo.m_NumAddresses = 0;
		bool GotVersion6 = false;
		for(unsigned int a = 0; a < Addresses.u.array.length; a++)
		{
			const json_value &Address = Addresses[a];
			if(Address.type != json_string)
			{
				return true;
			}
			if(str_startswith(Addresses[a], "tw-0.6+udp://"))
			{
				GotVersion6 = true;
				break;
			}
		}
		for(unsigned int a = 0; a < Addresses.u.array.length; a++)
		{
			const json_value &Address = Addresses[a];
			if(Address.type != json_string)
			{
				return true;
			}
			if(GotVersion6 && str_startswith(Addresses[a], "tw-0.7+udp://"))
			{
				continue;
			}
			NETADDR ParsedAddr;
			if(net_addr_from_url(&ParsedAddr, Addresses[a], nullptr, 0) || ParsedAddr.port == 0)
			{
				// 跳过未知地址。
				continue;
			}
			if(SetInfo.m_NumAddresses < (int)std::size(SetInfo.m_aAddresses))
			{
				SetInfo.m_aAddresses[SetInfo.m_NumAddresses] = ParsedAddr;
				SetInfo.m_NumAddresses += 1;
			}
		}
		if(SetInfo.m_NumAddresses > 0)
		{
			vServers.push_back(SetInfo);
		}
	}
	*pvServers = std::move(vServers);
	return false;
}

#endif
