#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_SPONSORS_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_SPONSORS_H

#include <cstddef>
#include <string>
#include <vector>

namespace qm_sponsors
{
	// 每个非空列表条目对应一名赞助者；姓名保留字面内容，不解释行内 Markdown。
	inline std::vector<std::string> ParseNames(const char *pMarkdown)
	{
		std::vector<std::string> vNames;
		if(!pMarkdown)
			return vNames;

		constexpr size_t MaxBytes = 64 * 1024;
		constexpr size_t MaxNames = 400;
		size_t Length = 0;
		while(Length < MaxBytes && pMarkdown[Length])
			++Length;
		const bool Truncated = Length == MaxBytes && pMarkdown[Length];
		const auto IsBlank = [](char Char) { return Char == ' ' || Char == '\t'; };

		size_t Position = 0;
		if(Length >= 3 && static_cast<unsigned char>(pMarkdown[0]) == 0xEF &&
			static_cast<unsigned char>(pMarkdown[1]) == 0xBB && static_cast<unsigned char>(pMarkdown[2]) == 0xBF)
			Position = 3;

		while(Position < Length && vNames.size() < MaxNames)
		{
			size_t Begin = Position;
			while(Position < Length && pMarkdown[Position] != '\n' && pMarkdown[Position] != '\r')
				++Position;
			size_t End = Position;
			// 字节上限落在行内时舍弃整行，避免把 UTF-8 姓名截成残缺内容。
			if(Position == Length && Truncated)
				break;
			if(Position < Length && pMarkdown[Position++] == '\r' && Position < Length && pMarkdown[Position] == '\n')
				++Position;

			while(Begin < End && IsBlank(pMarkdown[Begin]))
				++Begin;
			while(End > Begin && IsBlank(pMarkdown[End - 1]))
				--End;
			if(Begin == End)
				continue;

			if(pMarkdown[Begin] == '-' || pMarkdown[Begin] == '*' || pMarkdown[Begin] == '+')
				++Begin;
			else
			{
				const size_t NumberBegin = Begin;
				while(Begin < End && pMarkdown[Begin] >= '0' && pMarkdown[Begin] <= '9')
					++Begin;
				if(Begin == NumberBegin || Begin == End || pMarkdown[Begin] != '.')
					continue;
				++Begin;
			}
			if(Begin == End || !IsBlank(pMarkdown[Begin]))
				continue;
			while(Begin < End && IsBlank(pMarkdown[Begin]))
				++Begin;
			if(Begin < End)
				vNames.emplace_back(pMarkdown + Begin, End - Begin);
		}
		return vNames;
	}
}

#endif
