#ifndef ENGINE_CLIENT_BACKEND_VULKAN_BACKEND_VULKAN_H
#define ENGINE_CLIENT_BACKEND_VULKAN_BACKEND_VULKAN_H

#include <algorithm>
#include <cstddef>
#include <limits>

class CCommandProcessorFragment_GLBase;

class CQmVulkanRenderScheduler
{
	// 避免为少量 2D 绘制唤醒多个线程；具体粒度仍需实机对比。
	static constexpr size_t MIN_DRAWS_PER_WORKER = 64;
	size_t m_WorkerCount = 0;
	size_t m_DrawsPerWorker = 1;
	size_t m_RecordedDraws = 0;
	size_t m_ThreadIndex = 1;
	bool m_MainThreadOnly = false;

public:
	void StartCommands(size_t ThreadCount, size_t EstimatedDraws)
	{
		const size_t AvailableWorkers = ThreadCount > 1 ? ThreadCount - 1 : 0;
		m_WorkerCount = std::min(AvailableWorkers, EstimatedDraws / MIN_DRAWS_PER_WORKER);
		if(m_WorkerCount < 2)
			m_WorkerCount = 0;
		m_DrawsPerWorker = m_WorkerCount > 0 ? EstimatedDraws / m_WorkerCount + (EstimatedDraws % m_WorkerCount != 0) : 1;
		m_RecordedDraws = 0;
	}

	size_t CurrentThreadIndex() const { return m_MainThreadOnly ? 0 : m_ThreadIndex; }

	size_t ThreadIndex(bool ForceMainThread)
	{
		if(ForceMainThread || m_WorkerCount == 0)
			UseMainThread();
		if(m_MainThreadOnly)
			return 0;
		// 按连续绘制工作段分配，不能回到已经封口的较早线程。
		const size_t Worker = std::min(m_RecordedDraws / m_DrawsPerWorker, m_WorkerCount - 1);
		m_ThreadIndex = std::max(m_ThreadIndex, Worker + 1);
		return m_ThreadIndex;
	}

	void RecordDrawCalls(size_t DrawCalls)
	{
		m_RecordedDraws += std::min(DrawCalls, std::numeric_limits<size_t>::max() - m_RecordedDraws);
	}

	void UseMainThread() { m_MainThreadOnly = true; }

	void NewFrame()
	{
		// 帧内主线程尾段执行在所有工作线程之后，跨命令缓冲也必须维持此顺序。
		m_ThreadIndex = 1;
		m_MainThreadOnly = false;
	}
};

struct SVulkanVersion
{
	int m_Major;
	int m_Minor;
	int m_Patch;
};

static constexpr SVulkanVersion gs_BackendVulkanMinimumVersion = {1, 1, 0};
static constexpr SVulkanVersion gs_BackendVulkanMaximumVersion = {1, 4, 0};

constexpr bool IsVulkanVersionAtLeast(const SVulkanVersion &Version, const SVulkanVersion &Required)
{
	if(Version.m_Major != Required.m_Major)
		return Version.m_Major > Required.m_Major;
	if(Version.m_Minor != Required.m_Minor)
		return Version.m_Minor > Required.m_Minor;
	return Version.m_Patch >= Required.m_Patch;
}

constexpr SVulkanVersion MinVulkanVersion(const SVulkanVersion &Left, const SVulkanVersion &Right)
{
	return IsVulkanVersionAtLeast(Left, Right) ? Right : Left;
}

constexpr SVulkanVersion ClampVulkanVersionToSupportedRange(const SVulkanVersion &Version)
{
	if(!IsVulkanVersionAtLeast(Version, gs_BackendVulkanMinimumVersion))
		return gs_BackendVulkanMinimumVersion;
	return MinVulkanVersion(Version, gs_BackendVulkanMaximumVersion);
}

constexpr SVulkanVersion ResolveConfiguredVulkanApiVersion(int ConfigValue)
{
	return ConfigValue == 14 ? gs_BackendVulkanMaximumVersion : gs_BackendVulkanMinimumVersion;
}

CCommandProcessorFragment_GLBase *CreateVulkanCommandProcessorFragment();

#endif
