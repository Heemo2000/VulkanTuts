#pragma once

#include <iostream>
#include <string>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vma/vk_mem_alloc.h>

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

	private:
		void CreateInstance();
		void FindAndSelectPhysicalDevice();
		void GetQueue();
		void CreateMemoryAllocator();
		void CreateSurface();
		void CreateSwapchainAndImageViews();

	private:
		GLFWwindow* m_Window = nullptr;
		std::string m_WindowTitle;
		uint32_t m_Width;
		uint32_t m_Height;
	
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
};