#include "egpch.h"
#include "VulkanPipelineCache.h"
#include "VulkanContext.h"

#include "Eagle/Core/Project.h"

namespace Eagle
{
	static VkPipelineCache s_Cache = VK_NULL_HANDLE;
	static std::filesystem::path s_FullPath;

	void VulkanPipelineCache::Init()
	{
		// Pipeline cache
		const auto& props = VulkanContext::GetDevice()->GetPhysicalDevice()->GetProperties();
		std::stringstream cachePath;
		cachePath << Project::GetRendererCachePath().u8string()
			<< "/" << std::to_string(props.deviceID)
			<< "_" << std::to_string(props.vendorID)
			<< "_" << std::to_string(props.apiVersion)
			<< "_" << std::to_string(props.driverVersion)
			<< "_";

		for (auto uuid_element : props.pipelineCacheUUID)
		{
			cachePath << (std::hex) << (std::uint32_t)uuid_element << (std::dec);
		}
		cachePath << "_vkpipeline.cache";

		std::vector<char> cacheData;
		s_FullPath = cachePath.str();
		std::ifstream in(s_FullPath, std::ios_base::in | std::ios_base::binary);
		if (in)
		{
			in.seekg(0, std::ios_base::end);
			std::size_t size = in.tellg();
			in.seekg(0, std::ios_base::beg);
			cacheData.resize(size);

			in.read(cacheData.data(), size);
			in.close();
		}

		VkPipelineCacheCreateInfo cacheCI{};
		cacheCI.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		cacheCI.initialDataSize = cacheData.size();
		cacheCI.pInitialData = cacheData.data();

		VK_CHECK(vkCreatePipelineCache(VulkanContext::GetDevice()->GetVulkanDevice(), &cacheCI, nullptr, &s_Cache));
	}

	void VulkanPipelineCache::Shutdown()
	{
		std::filesystem::path path = Project::GetRendererCachePath();
		if (!std::filesystem::exists(path))
			std::filesystem::create_directories(path);

		VkDevice device = VulkanContext::GetDevice()->GetVulkanDevice();

		std::size_t cacheSize;
		vkGetPipelineCacheData(device, s_Cache, &cacheSize, 0);
		std::vector<char> cache_data(cacheSize);
		vkGetPipelineCacheData(device, s_Cache, &cacheSize, cache_data.data());

		// Saving cache to file
		std::ofstream out(s_FullPath, std::ios_base::binary | std::ios_base::out | std::ios_base::trunc);
		out.write(cache_data.data(), cacheSize);
		out.close();

		vkDestroyPipelineCache(device, s_Cache, nullptr);
	}

	VkPipelineCache VulkanPipelineCache::GetCache()
	{
		return s_Cache;
	}
}
