#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <array>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vma/vk_mem_alloc.h>


#include <glm/glm.hpp>

#include <tiny_obj_loader.h>

#include <ktx.h>
#include <ktxvulkan.h>

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 uv;
};

struct ShaderDataBuffer
{
	VmaAllocation allocation = VK_NULL_HANDLE;
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceAddress deviceAddress{};
	void* mapped = nullptr;
};

struct ShaderData
{
	glm::mat4 projection;
	glm::mat4 view;
	glm::mat4 model;
	glm::vec4 lightPos{ 0.0f, -10.0f, 10.0f, 0.0f };
	uint32_t selected{1};
};

struct Texture
{
	VmaAllocation allocation{ VK_NULL_HANDLE };
	VkImage image{ VK_NULL_HANDLE };
	VkImageView view{ VK_NULL_HANDLE };
	VkSampler sampler{ VK_NULL_HANDLE };
};

class MyTriangle
{
	public:
		MyTriangle(std::string windowTitle, uint32_t width, uint32_t height);
		void Setup();
		void Run();

	private:
		void InitWindow();
		void InitVulkan();

	private:
		void SetResolution(uint32_t width, uint32_t height);
		bool Check(VkResult condition, std::string trueStatus, std::string falseStatus);

	private:
		void CreateInstance();
		void FindAndSelectPhysicalDevice();
		void GetQueue();
		void CreateMemoryAllocator();
		void CreateSurface();
		void CreateSwapchainAndImageViews();
		void CheckDepthFormatAndCreateDepthImage();
		void LoadModelRelatedDataAndShaderRelatedStuff();
		void CreateFencesAndSemaphores();
		void CreateCommandPoolAndCommandBuffers();
		void LoadTexture();
		void LoadShaders();
		void SetupGraphicsPipeline();

	private:
		GLFWwindow* m_Window = nullptr;
		std::string m_WindowTitle;
		uint32_t m_Width;
		uint32_t m_Height;
	
	private:
		static const uint32_t s_MaxFramesInFlight = 2;
	
	private:
		VkInstance m_Instance = VK_NULL_HANDLE;
		std::vector<VkPhysicalDevice> m_PhysicalDevices;
		uint32_t m_RequiredPhyDeviceIndex = -1;
		uint32_t m_QueueFamilyIndex = -1;
		VkQueue m_Queue;
		VkDevice m_LogicalDevice;
		VmaAllocator m_Allocator;
		VkSurfaceKHR m_Surface;
		VkSurfaceCapabilitiesKHR m_SurfaceCapabilities{};
		VkSwapchainKHR m_Swapchain;
		std::vector<VkImage> m_SwapchainImages;
		std::vector<VkImageView> m_SwapchainImageViews;
		VkFormat m_ImageFormat = VK_FORMAT_UNDEFINED;
		VkFormat m_DepthFormat = VK_FORMAT_UNDEFINED;
		VkImage m_DepthImage;
		VmaAllocation m_DepthImageAllocation{ VK_NULL_HANDLE };
		VkImageView m_DepthImageView;
		VkBuffer m_Buffer;
		VmaAllocation m_VertexBufferAllocation{ VK_NULL_HANDLE };
		void* m_BufferPtr = nullptr;
		std::array<ShaderDataBuffer, s_MaxFramesInFlight> m_ShaderDataBuffers;
		std::array<VkCommandBuffer, s_MaxFramesInFlight> m_CommandBuffers;
		std::array<VkFence, s_MaxFramesInFlight> m_Fences;
		std::array<VkSemaphore, s_MaxFramesInFlight> m_PresentSemaphores;
		std::vector<VkSemaphore> m_RenderSemaphores;
		VkCommandPool m_CommandPool;
		std::array<Texture, 1> m_Textures{};
		std::vector<VkDescriptorImageInfo> m_TextureDescriptors;
		VkDescriptorSetLayout m_DescriptorSetLayoutTex{ VK_NULL_HANDLE };
		VkDescriptorPool m_DescriptorPool{ VK_NULL_HANDLE };
		VkDescriptorSet m_DescriptorSetTex{ VK_NULL_HANDLE };
		VkShaderModule m_ShaderModule{};
		VkPipelineLayout m_PipelineLayout{ VK_NULL_HANDLE };
};