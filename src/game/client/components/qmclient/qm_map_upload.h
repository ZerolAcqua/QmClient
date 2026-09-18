#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_MAP_UPLOAD_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_MAP_UPLOAD_H

#include <base/system.h>

#include <engine/engine.h>
#include <engine/shared/http.h>
#include <engine/shared/json.h>
#include <engine/storage.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace qm_map_upload
{

	// 限制客户端组装 multipart 时的内存占用，不代表服务端的大小限制。
	constexpr size_t MAX_MAP_SIZE = 64 * 1024 * 1024;
	constexpr size_t MAX_RESPONSE_SIZE = 16 * 1024;

	inline bool IsMapFilename(const char *pFilename)
	{
		if(pFilename == nullptr)
			return false;
		const int Length = str_length(pFilename);
		return Length >= 4 && str_comp_nocase(pFilename + Length - 4, ".map") == 0;
	}

	inline bool ValidateFilename(const char *pFilename)
	{
		if(!IsMapFilename(pFilename) || !str_utf8_check(pFilename))
			return false;
		const std::string_view Filename(pFilename);
		const size_t StemLength = Filename.size() - 4;
		if(StemLength == 0 || Filename[StemLength - 1] == ' ')
			return false;
		// 与官网上传页保持一致，同时拒绝可能破坏 multipart 头的控制字符。
		for(size_t Index = 0; Index < Filename.size(); ++Index)
		{
			const unsigned char Character = Filename[Index];
			if(Character < 32 || Character == 127 ||
				std::string_view("!@#$%^&*()+|\\/[]{};:'\",<>=").find(Character) != std::string_view::npos ||
				(Character == '.' && Index != StemLength))
				return false;
		}
		return true;
	}

	inline bool BuildMultipart(const char *pFilename, const char *pPlayerName, const unsigned char *pData, size_t Size, const char *pBoundary, std::string &Body)
	{
		Body.clear();
		if(!ValidateFilename(pFilename) || pPlayerName == nullptr ||
			!str_utf8_check(pPlayerName) || *str_utf8_skip_whitespaces(pPlayerName) == '\0' ||
			pData == nullptr || Size == 0 || Size > MAX_MAP_SIZE || pBoundary == nullptr)
			return false;
		const std::string_view Boundary(pBoundary);
		if(Boundary.empty() || Boundary.size() > 70)
			return false;
		for(const char Character : Boundary)
		{
			if(!((Character >= 'a' && Character <= 'z') || (Character >= 'A' && Character <= 'Z') ||
				   (Character >= '0' && Character <= '9') || Character == '-'))
				return false;
		}
		const std::string_view Data(reinterpret_cast<const char *>(pData), Size);
		if(Data.find(Boundary) != std::string_view::npos ||
			std::string_view(pFilename).find(Boundary) != std::string_view::npos ||
			std::string_view(pPlayerName).find(Boundary) != std::string_view::npos)
			return false;

		Body.reserve(Size + str_length(pPlayerName) + str_length(pFilename) + 512);
		Body = "--";
		Body += Boundary;
		Body += "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"";
		Body += pFilename;
		Body += "\"\r\nContent-Type: application/octet-stream\r\n\r\n";
		Body.append(Data);
		Body += "\r\n--";
		Body += Boundary;
		Body += "\r\nContent-Disposition: form-data; name=\"player_id\"\r\n\r\n";
		Body += pPlayerName;
		Body += "\r\n--";
		Body += Boundary;
		Body += "--\r\n";
		return true;
	}

	struct CResponse
	{
		bool m_Success = false;
		bool m_Valid = false;
		std::string m_Message;
	};

	inline CResponse ParseResponse(int StatusCode, const char *pData, size_t Length)
	{
		CResponse Result;
		if(pData == nullptr || Length == 0 || Length > MAX_RESPONSE_SIZE)
			return Result;
		json_value *pRoot = JsonParse(pData, Length);
		if(pRoot == nullptr)
			return Result;
		if(pRoot->type == json_object)
		{
			const json_value *pSuccess = json_object_get(pRoot, "success");
			Result.m_Valid = pSuccess->type == json_boolean;
			Result.m_Success = Result.m_Valid && pSuccess->u.boolean && StatusCode >= 200 && StatusCode < 300;
			const json_value *pMessage = json_object_get(pRoot, "message");
			if(pMessage->type == json_string)
			{
				Result.m_Message.assign(pMessage->u.string.ptr, pMessage->u.string.length);
				for(char &Character : Result.m_Message)
				{
					if(static_cast<unsigned char>(Character) < 32 || Character == 127)
						Character = ' ';
				}
			}
		}
		json_value_free(pRoot);
		return Result;
	}

	enum class EStatus
	{
		IDLE,
		UPLOADING,
		SUCCESS,
		CANCELLED,
		INVALID_FILE,
		TOO_LARGE,
		READ_FAILED,
		NETWORK_ERROR,
		SERVER_ERROR,
		INVALID_RESPONSE,
		MISSING_PLAYER,
	};

	// 后台任务只持有路径、名字和自己的结果，不访问菜单或存储对象的生命周期。
	class CPrepareJob : public IJob
	{
		std::string m_Path;
		std::string m_Filename;
		std::string m_PlayerName;
		std::atomic<bool> m_Cancelled{false};
		EStatus m_Status = EStatus::UPLOADING;
		std::shared_ptr<CHttpRequest> m_pRequest;

		bool StopIfCancelled()
		{
			if(!m_Cancelled.load(std::memory_order_relaxed))
				return false;
			m_Status = EStatus::CANCELLED;
			return true;
		}

		void Run() override
		{
			if(StopIfCancelled())
				return;
			unsigned char aRandom[16];
			secure_random_fill(aRandom, sizeof(aRandom));
			std::string Boundary = "QmClientMapUpload";
			for(const unsigned char Byte : aRandom)
			{
				Boundary += "0123456789abcdef"[Byte >> 4];
				Boundary += "0123456789abcdef"[Byte & 15];
			}
			std::string Body;
			{
				IOHANDLE File = io_open(m_Path.c_str(), IOFLAG_READ);
				if(File == nullptr)
				{
					m_Status = EStatus::READ_FAILED;
					return;
				}
				const int64_t Size = io_length(File);
				if(Size <= 0 || Size > static_cast<int64_t>(MAX_MAP_SIZE))
				{
					io_close(File);
					m_Status = Size > static_cast<int64_t>(MAX_MAP_SIZE) ? EStatus::TOO_LARGE : EStatus::READ_FAILED;
					return;
				}
				std::vector<unsigned char> vData(static_cast<size_t>(Size));
				size_t Read = 0;
				while(Read < vData.size())
				{
					if(StopIfCancelled())
					{
						io_close(File);
						return;
					}
					const unsigned Chunk = static_cast<unsigned>(std::min<size_t>(vData.size() - Read, 1024 * 1024));
					const unsigned Received = io_read(File, vData.data() + Read, Chunk);
					Read += Received;
					if(Received != Chunk)
						break;
				}
				unsigned char Extra;
				const bool Complete = Read == vData.size() && io_read(File, &Extra, 1) == 0;
				io_close(File);
				if(!Complete)
				{
					m_Status = EStatus::READ_FAILED;
					return;
				}
				if(StopIfCancelled())
					return;
				if(!BuildMultipart(m_Filename.c_str(), m_PlayerName.c_str(), vData.data(), vData.size(), Boundary.c_str(), Body))
				{
					m_Status = EStatus::INVALID_FILE;
					return;
				}
			}

			if(StopIfCancelled())
				return;
			m_pRequest = std::make_shared<CHttpRequest>("https://shengyan.art/api/upload");
			m_pRequest->Post(reinterpret_cast<const unsigned char *>(Body.data()), Body.size());
			const std::string ContentType = "multipart/form-data; boundary=" + Boundary;
			m_pRequest->HeaderString("Content-Type", ContentType.c_str());
			// 服主授权所有客户端共享的公开上传凭据，不用于日志或界面展示。
			m_pRequest->HeaderString("Authorization", "Bearer at0mSpxHerUXTyoWplwMY7EODqPfh6AaO11IVpYDc1U");
			m_pRequest->HeaderString("Accept", "application/json");
			m_pRequest->MaxResponseSize(MAX_RESPONSE_SIZE);
			m_pRequest->FailOnErrorStatus(false);
			m_pRequest->Timeout(CTimeout{10000, 300000, 1024, 30});
			m_pRequest->LogProgress(HTTPLOG::NONE);
			if(StopIfCancelled())
				m_pRequest.reset();
		}

	public:
		CPrepareJob(const char *pPath, const char *pFilename, const char *pPlayerName) :
			m_Path(pPath), m_Filename(pFilename), m_PlayerName(pPlayerName) {}

		void Cancel() { m_Cancelled.store(true, std::memory_order_relaxed); }
		// 普通结果字段仅在 STATE_DONE 后由主线程读取。
		EStatus Status() const { return m_Status; }
		std::shared_ptr<CHttpRequest> Request() const { return m_pRequest; }
	};

	class CUpload
	{
		std::shared_ptr<CPrepareJob> m_pPrepareJob;
		IHttp *m_pHttp = nullptr;
		std::shared_ptr<CHttpRequest> m_pRequest;
		EStatus m_Status = EStatus::IDLE;
		std::string m_Detail;
		int m_StatusCode = 0;

	public:
		CUpload() = default;
		CUpload(const CUpload &) = delete;
		CUpload &operator=(const CUpload &) = delete;
		~CUpload() { Cancel(); }

		bool Busy() const { return m_pPrepareJob != nullptr || m_pRequest != nullptr; }
		EStatus Status() const { return m_Status; }
		const std::string &Detail() const { return m_Detail; }
		int StatusCode() const { return m_StatusCode; }

		void Reset()
		{
			if(Busy())
				return;
			m_Status = EStatus::IDLE;
			m_Detail.clear();
			m_StatusCode = 0;
		}

		void Start(IStorage *pStorage, IHttp *pHttp, IEngine *pEngine, const char *pPath, int StorageType, const char *pPlayerName)
		{
			if(Busy())
				return;
			Reset();
			if(pPath == nullptr || !ValidateFilename(fs_filename(pPath)))
			{
				m_Status = EStatus::INVALID_FILE;
				return;
			}
			if(pPlayerName == nullptr || !str_utf8_check(pPlayerName) || *str_utf8_skip_whitespaces(pPlayerName) == '\0')
			{
				m_Status = EStatus::MISSING_PLAYER;
				return;
			}
			if(StorageType < IStorage::TYPE_SAVE || StorageType >= pStorage->NumPaths())
			{
				m_Status = EStatus::READ_FAILED;
				return;
			}
			char aAbsolutePath[IO_MAX_PATH_LENGTH];
			pStorage->GetCompletePath(StorageType, pPath, aAbsolutePath, sizeof(aAbsolutePath));
			m_pHttp = pHttp;
			m_pPrepareJob = std::make_shared<CPrepareJob>(aAbsolutePath, fs_filename(pPath), pPlayerName);
			m_Status = EStatus::UPLOADING;
			pEngine->AddJob(m_pPrepareJob);
		}

		void Poll()
		{
			if(m_pPrepareJob)
			{
				if(!m_pPrepareJob->Done())
					return;
				if(m_Status != EStatus::CANCELLED && m_pPrepareJob->State() == IJob::STATE_DONE)
				{
					m_Status = m_pPrepareJob->Status();
					m_pRequest = m_pPrepareJob->Request();
					if(m_pRequest)
						m_pHttp->Run(m_pRequest);
				}
				else
					m_Status = EStatus::CANCELLED;
				m_pPrepareJob.reset();
			}
			if(!m_pRequest || !m_pRequest->Done())
				return;
			if(m_Status == EStatus::CANCELLED)
			{
				m_pRequest.reset();
				return;
			}
			if(m_pRequest->State() != EHttpState::DONE)
			{
				m_Status = m_pRequest->State() == EHttpState::ABORTED ? EStatus::CANCELLED : EStatus::NETWORK_ERROR;
				m_pRequest.reset();
				return;
			}
			m_StatusCode = m_pRequest->StatusCode();
			unsigned char *pData;
			size_t Length;
			m_pRequest->Result(&pData, &Length);
			const CResponse Response = ParseResponse(m_StatusCode, reinterpret_cast<const char *>(pData), Length);
			m_Detail = Response.m_Message;
			if(Response.m_Success)
				m_Status = EStatus::SUCCESS;
			else if(m_StatusCode < 200 || m_StatusCode >= 300 || Response.m_Valid)
				m_Status = EStatus::SERVER_ERROR;
			else
				m_Status = EStatus::INVALID_RESPONSE;
			m_pRequest.reset();
		}

		void Cancel()
		{
			if(!Busy())
				return;
			m_Status = EStatus::CANCELLED;
			if(m_pPrepareJob)
				m_pPrepareJob->Cancel();
			if(m_pRequest)
				m_pRequest->Abort();
		}
	};
}

#endif
