#include "MyTriangle.h"
#include <filesystem>
#include <stddef.h>
#include <slang.h>
#include <slang-com-ptr.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

MyTriangle::MyTriangle(std::string windowTitle, uint32_t width, uint32_t height) : 
			m_WindowTitle(windowTitle),
			m_Width(width),
			m_Height(height)
{}

void MyTriangle::Setup()
{
	InitWindow();
	InitVulkan();
}

void MyTriangle::Run()
{
	m_FrameIndex = 0;
	glm::vec3 instancePos = glm::vec3(3.0f, 0.0f, 0.0f);
	float lastTime = glfwGetTime();
	while (!glfwWindowShouldClose(m_Window))
	{
		glfwPollEvents();

		Check(vkWaitForFences(m_LogicalDevice, 1, &m_Fences[m_FrameIndex], true, UINT64_MAX));
		Check(vkResetFences(m_LogicalDevice, 1, &m_Fences[m_FrameIndex]));

		CheckSwapchain(vkAcquireNextImageKHR(m_LogicalDevice, m_Swapchain, UINT64_MAX, m_PresentSemaphores[m_FrameIndex], VK_NULL_HANDLE, &m_ImageIndex));

		m_ShaderData.projection = glm::perspective(glm::radians(60.0f), (float)m_Width / (float)m_Height, 0.1f, 32.0f);
		m_ShaderData.view = glm::translate(glm::mat4(1.0f), m_CamPos);
		m_ShaderData.model = glm::translate(glm::mat4(1.0f), instancePos) * glm::mat4_cast(glm::quat(m_ObjectRotation));

		memcpy(m_ShaderDataBuffers[m_FrameIndex].allocationInfo.pMappedData, &m_ShaderData, sizeof(ShaderData));

		VkCommandBuffer commandBuffer = m_CommandBuffers[m_FrameIndex];

		Check(vkResetCommandBuffer(commandBuffer, 0));

		VkCommandBufferBeginInfo beginInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		};

		Check(vkBeginCommandBuffer(commandBuffer, &beginInfo));

		std::array<VkImageMemoryBarrier2, 2> outputBarriers
		{
			VkImageMemoryBarrier2
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask = 0,
				.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
				.image = m_SwapchainImages[m_ImageIndex],
				.subresourceRange { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}
			},

			VkImageMemoryBarrier2
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
				.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
				.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
				.image = m_DepthImage,
				.subresourceRange { .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, .levelCount = 1, .layerCount = 1}
			}
		};

		VkDependencyInfo barrierDependencyInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = 2,
			.pImageMemoryBarriers = outputBarriers.data()
		};

		vkCmdPipelineBarrier2(commandBuffer, &barrierDependencyInfo);

		VkRenderingAttachmentInfo colorAttachmentInfo
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = m_SwapchainImageViews[m_ImageIndex],
			.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue { .color { m_ClearColor.r, m_ClearColor.g, m_ClearColor.b, m_ClearColor.a}}
		};

		VkRenderingAttachmentInfo depthAttachmentInfo
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = m_DepthImageView,
			.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.clearValue { .depthStencil {1.0f, 0}}
		};


		int width = 0.0f;
		int height = 0.0f;
		glfwGetWindowSize(m_Window, &width, &height);
		VkRenderingInfo renderingInfo
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea = {.extent { .width = static_cast<uint32_t>(width), .height = static_cast<uint32_t>(height)}},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachmentInfo,
			.pDepthAttachment = &depthAttachmentInfo
		};

		vkCmdBeginRendering(commandBuffer, &renderingInfo);

		VkViewport viewport
		{
			.width = static_cast<float>(m_Width),
			.height = static_cast<float>(m_Height),
			.minDepth = 0.0f,
			.maxDepth = 1.0f
		};

		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		VkRect2D scissor{ .extent {.width = m_Width, .height = m_Height} };
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);

		VkDeviceSize vOffset{ 0 };

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_DescriptorSetTex, 0, nullptr);
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_Buffer, &vOffset);

		vkCmdBindIndexBuffer(commandBuffer, m_Buffer, m_VertexBufferSize, VK_INDEX_TYPE_UINT16);

		vkCmdPushConstants(commandBuffer, m_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(VkDeviceAddress), &m_ShaderDataBuffers[m_FrameIndex].deviceAddress);

		vkCmdDrawIndexed(commandBuffer, m_IndexCount, 1, 0, 0, 0);

		vkCmdEndRendering(commandBuffer);

		VkImageMemoryBarrier2 barrierPresent
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstAccessMask = 0,
			.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.image = m_SwapchainImages[m_ImageIndex],
			.subresourceRange { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1,.layerCount = 1}
		};

		vkEndCommandBuffer(commandBuffer);

		VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo submitInfo
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &m_PresentSemaphores[m_FrameIndex],
			.pWaitDstStageMask = &waitStages,
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &m_RenderSemaphores[m_ImageIndex]
		};

		Check(vkQueueSubmit(m_Queue, 1, &submitInfo, m_Fences[m_FrameIndex]));

		m_FrameIndex = (m_FrameIndex + 1) % s_MaxFramesInFlight;

		VkPresentInfoKHR presentInfo
		{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &m_RenderSemaphores[m_ImageIndex],
			.swapchainCount = 1,
			.pSwapchains = &m_Swapchain,
			.pImageIndices = &m_ImageIndex
		};

		CheckSwapchain(vkQueuePresentKHR(m_Queue, &presentInfo));

		m_DeltaTime = glfwGetTime() - lastTime;
		lastTime = glfwGetTime();

		if (m_UpdateSwapchain == true)
		{
			m_UpdateSwapchain = false;
			vkDeviceWaitIdle(m_LogicalDevice);
			Check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevices[m_RequiredPhyDeviceIndex], m_Surface, &m_SurfaceCapabilities));
			m_SwapchainCreateInfo.oldSwapchain = m_Swapchain;
			m_SwapchainCreateInfo.imageExtent = { .width = m_Width, .height = m_Height };

			Check(vkCreateSwapchainKHR(m_LogicalDevice, &m_SwapchainCreateInfo, nullptr, &m_Swapchain));

			for (auto i = 0; i < m_ImagesCount; i++)
			{
				VkImageViewCreateInfo viewCreateInfo
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
					.image = m_SwapchainImages[i],
					.viewType = VK_IMAGE_VIEW_TYPE_2D,
					.format = m_ImageFormat,
					.subresourceRange { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}
				};

				Check(vkCreateImageView(m_LogicalDevice, &viewCreateInfo, nullptr, &m_SwapchainImageViews[i]));
			}

			vkDestroySwapchainKHR(m_LogicalDevice, m_SwapchainCreateInfo.oldSwapchain, nullptr);
			vmaDestroyImage(m_Allocator, m_DepthImage, m_DepthImageAllocation);
			vkDestroyImageView(m_LogicalDevice, m_DepthImageView, nullptr);
			m_DepthImageCreateInfo.extent = { .width = m_Width, .height = m_Height, .depth = 1 };

			VmaAllocationCreateInfo allocationCreateInfo
			{
				.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
				.usage = VMA_MEMORY_USAGE_AUTO
			};

			Check(vmaCreateImage(m_Allocator, &m_DepthImageCreateInfo, &allocationCreateInfo, &m_DepthImage, &m_DepthImageAllocation, nullptr));

			VkImageViewCreateInfo viewCreateInfo
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = m_DepthImage,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = m_DepthFormat,
				.subresourceRange { .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1}
			};

			Check(vkCreateImageView(m_LogicalDevice, &viewCreateInfo, nullptr, &m_DepthImageView));
		}

	}
}

void MyTriangle::Cleanup()
{
	Check(vkDeviceWaitIdle(m_LogicalDevice));
	for (int i = 0; i < s_MaxFramesInFlight; i++)
	{
		vkDestroyFence(m_LogicalDevice, m_Fences[i], nullptr);
		vkDestroySemaphore(m_LogicalDevice, m_PresentSemaphores[i], nullptr);
		vmaUnmapMemory(m_Allocator, m_ShaderDataBuffers[i].allocation);
		vmaDestroyBuffer(m_Allocator, m_ShaderDataBuffers[i].buffer, m_ShaderDataBuffers[i].allocation);
	}

	for (int i = 0; i < m_RenderSemaphores.size(); i++)
	{
		vkDestroySemaphore(m_LogicalDevice, m_RenderSemaphores[i], nullptr);
	}

	vmaDestroyBuffer(m_Allocator, m_Buffer, m_VertexBufferAllocation);

	for (int i = 0; i < m_Textures.size(); i++)
	{
		vkDestroyImageView(m_LogicalDevice, m_Textures[i].view, nullptr);
		vkDestroySampler(m_LogicalDevice, m_Textures[i].sampler, nullptr);
		vmaDestroyImage(m_Allocator, m_Textures[i].image, m_Textures[i].allocation);
	}

	vkDestroyDescriptorSetLayout(m_LogicalDevice, m_DescriptorSetLayoutTex, nullptr);
	vkDestroyDescriptorPool(m_LogicalDevice, m_DescriptorPool, nullptr);
	vkDestroyPipelineLayout(m_LogicalDevice, m_PipelineLayout, nullptr);
	vkDestroyPipeline(m_LogicalDevice, m_GraphicsPipeline, nullptr);
	vkDestroySwapchainKHR(m_LogicalDevice, m_Swapchain, nullptr);
	vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
	vkDestroyCommandPool(m_LogicalDevice, m_CommandPool, nullptr);
	vkDestroyShaderModule(m_LogicalDevice, m_ShaderModule, nullptr);
	vmaDestroyAllocator(m_Allocator);
	vkDestroyDevice(m_LogicalDevice, nullptr);
	vkDestroyInstance(m_Instance, nullptr);
	glfwDestroyWindow(m_Window);
}

void MyTriangle::SetUpdateSwapchainTrue()
{
	m_UpdateSwapchain = true;
}

void MyTriangle::RotateX(float moveInputX)
{
	m_ObjectRotation.y += moveInputX * m_MovementSpeed.y * m_DeltaTime;
}

void MyTriangle::RotateY(float moveInputY)
{
	m_ObjectRotation.x += moveInputY * m_MovementSpeed.x * m_DeltaTime;
}

void MyTriangle::InitWindow()
{
	if (!glfwInit())
	{
		std::cout << "GLFW could not be initialized" << std::endl;
		return;
	}

	if (!glfwVulkanSupported())
	{
		std::cout << "Vulkan is not supported!" << std::endl;
		return;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

	m_Window = glfwCreateWindow(m_Width, m_Height, m_WindowTitle.c_str(), nullptr, nullptr);
	
	glfwSetWindowUserPointer(m_Window, this);

	glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
		{
			auto app = reinterpret_cast<MyTriangle*>(glfwGetWindowUserPointer(window));
			app->SetResolution(width, height);
			app->SetUpdateSwapchainTrue();
		});

	glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scanCode, int action, int mods)
	{
		auto app = reinterpret_cast<MyTriangle*>(glfwGetWindowUserPointer(window));
		
		if (key == GLFW_KEY_W)
		{
			app->RotateY(1.0f);
		}
		else if(key == GLFW_KEY_S)
		{
			app->RotateY(-1.0f);
		}

		if (key == GLFW_KEY_A)
		{
			app->RotateX(-1.0f);
		}
		else if (key == GLFW_KEY_D)
		{
			app->RotateX(1.0f);
		}
	});


	if (!m_Window)
	{
		std::cout << "Couldn't create a window!" << std::endl;
		glfwTerminate();
		return;
	}
}

void MyTriangle::InitVulkan()
{
	CreateInstance();
	FindAndSelectPhysicalDevice();
	GetQueue();
	CreateMemoryAllocator();
	CreateSurface();
	CreateSwapchainAndImageViews();
	CheckDepthFormatAndCreateDepthImage();
	LoadModelRelatedDataAndShaderRelatedStuff();
	CreateFencesAndSemaphores();
	CreateCommandPoolAndCommandBuffers();
	LoadTexture();
	LoadShaders();
	SetupGraphicsPipeline();
}

void MyTriangle::SetResolution(uint32_t width, uint32_t height)
{
	m_Width = width;
	m_Height = height;

	std::cout << "Resolution: " << m_Width << " X " << m_Height << std::endl;
}

bool MyTriangle::Check(VkResult condition, std::string trueStatus, std::string falseStatus)
{
	if (condition != VK_SUCCESS)
	{
		std::cout << falseStatus << std::endl;
		exit(1);
		return false;
	}

	std::cout << std::endl;
	std::cout << trueStatus << std::endl;
	return true;
}

bool MyTriangle::Check(VkResult condition)
{
	if (condition != VK_SUCCESS)
	{
		exit(1);
		return false;
	}

	return true;
}

bool MyTriangle::CheckSwapchain(VkResult condition)
{
	if (condition < VK_SUCCESS)
	{
		if (condition == VK_ERROR_OUT_OF_DATE_KHR)
		{
			m_UpdateSwapchain = true;
			return true;
		}

		std::cout << "Vulkan call return an error: " << condition << std::endl;
		exit(condition);
		return false;
	}

	return true;
}

void MyTriangle::CreateInstance()
{
	VkApplicationInfo appInfo
	{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Vulkan Tuts Engine",
		.apiVersion = VK_API_VERSION_1_3
	};

	uint32_t instanceExtensionsCount = 0;
	const char** instanceExtensions = glfwGetRequiredInstanceExtensions(&instanceExtensionsCount);
	VkInstanceCreateInfo instanceCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &appInfo,
		.enabledExtensionCount = instanceExtensionsCount,
		.ppEnabledExtensionNames = instanceExtensions
	};



	if (Check(vkCreateInstance(&instanceCreateInfo, nullptr, &m_Instance), "Instance creation successful", "Failed to create instance!!"));
}

void MyTriangle::FindAndSelectPhysicalDevice()
{
	uint32_t deviceCount = 0;
	Check(vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr), "Found some GPU Devices ", "No GPU Device Found! Count: 0");

	m_PhysicalDevices.resize(deviceCount);

	Check(vkEnumeratePhysicalDevices(m_Instance, &deviceCount, m_PhysicalDevices.data()), "Found some GPU Devices: ", "No GPU Device Found! Count: 0");
	
	for (int i = 0; i < deviceCount; i++)
	{
		VkPhysicalDeviceProperties2 deviceProperties{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
		};

		vkGetPhysicalDeviceProperties2(m_PhysicalDevices[i], &deviceProperties);

		std::cout << "Device: " << deviceProperties.properties.deviceName << std::endl;
	}

	for (int i = 0; i < deviceCount; i++)
	{
		VkPhysicalDeviceProperties2 deviceProperties{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
		};

		vkGetPhysicalDeviceProperties2(m_PhysicalDevices[i], &deviceProperties);
		
		if (deviceProperties.properties.deviceType &
			(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU | VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU))
		{
			m_RequiredPhyDeviceIndex = i;
			break;
		}
	}
}

void MyTriangle::GetQueue()
{
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevices[m_RequiredPhyDeviceIndex],
		&queueFamilyCount,
		nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);

	vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevices[m_RequiredPhyDeviceIndex],
		&queueFamilyCount,
		queueFamilies.data());

	for (int i = 0; i < queueFamilyCount; i++)
	{
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			m_QueueFamilyIndex = i;
			break;
		}
	}

	const float queueFamilyProperties = 1.0f;
	VkDeviceQueueCreateInfo queueCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = m_QueueFamilyIndex,
		.queueCount = 1,
		.pQueuePriorities = &queueFamilyProperties
	};

	const std::vector<const char*> deviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	VkPhysicalDeviceVulkan12Features enabledVk12Features
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
		.descriptorIndexing = VK_TRUE,
		.descriptorBindingVariableDescriptorCount = VK_TRUE,
		.runtimeDescriptorArray = VK_TRUE,
		.bufferDeviceAddress = VK_TRUE
	};

	const VkPhysicalDeviceVulkan13Features enabledVk13Features
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.pNext = &enabledVk12Features,
		.synchronization2 = VK_TRUE,
		.dynamicRendering = VK_TRUE
	};

	const VkPhysicalDeviceFeatures enabledVk10Features
	{
		.samplerAnisotropy = VK_TRUE,
	};

	VkDeviceCreateInfo deviceCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &enabledVk13Features,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &queueCreateInfo,
		.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
		.ppEnabledExtensionNames = deviceExtensions.data(),
		.pEnabledFeatures = &enabledVk10Features
	};

	Check(vkCreateDevice(m_PhysicalDevices[m_RequiredPhyDeviceIndex],
		&deviceCreateInfo,
		nullptr,
		&m_LogicalDevice),
		"Logical Device Creation successful",
		"Failed to create Logical Device!");

	vkGetDeviceQueue(m_LogicalDevice, m_QueueFamilyIndex, 0, &m_Queue);
}

void MyTriangle::CreateMemoryAllocator()
{
	VmaVulkanFunctions vkFunctions
	{
		.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
		.vkGetDeviceProcAddr = vkGetDeviceProcAddr,
		.vkCreateImage = vkCreateImage
	};

	VmaAllocatorCreateInfo allocatorCreateInfo
	{
		.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
		.physicalDevice = m_PhysicalDevices[m_RequiredPhyDeviceIndex],
		.device = m_LogicalDevice,
		.pVulkanFunctions = &vkFunctions,
		.instance = m_Instance
	};

	Check(vmaCreateAllocator(&allocatorCreateInfo, &m_Allocator), 
							 "Memory allocator creation successful", 
						     "Failed to create memory allocator!");
}

void MyTriangle::CreateSurface()
{
	Check(glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface), "Surface creation successful", "Surface creation failed!");
	
	Check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		m_PhysicalDevices[m_RequiredPhyDeviceIndex],
		m_Surface,
		&m_SurfaceCapabilities),
		"Getting Physical Device Surface Capabilities Info successful",
		"Failed to get physical device surface capabilities info!");
}

void MyTriangle::CreateSwapchainAndImageViews()
{
	m_ImageFormat =  VK_FORMAT_B8G8R8A8_SRGB;
	m_SwapchainCreateInfo =
	{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = m_Surface,
		.minImageCount = m_SurfaceCapabilities.minImageCount,
		.imageFormat = m_ImageFormat,
		.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
		.imageExtent {.width = m_SurfaceCapabilities.currentExtent.width, .height = m_SurfaceCapabilities.currentExtent.height},
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_FIFO_KHR
	};

	Check(vkCreateSwapchainKHR(m_LogicalDevice, &m_SwapchainCreateInfo, nullptr, &m_Swapchain), "Swapchain creation successful", "Swapchain creation failed!");

	m_ImagesCount = 0;
	Check(vkGetSwapchainImagesKHR(m_LogicalDevice, m_Swapchain, &m_ImagesCount, nullptr), "Getting swapchain images count successful", "Getting swapchain images count failed!");
	
	m_SwapchainImages.resize(m_ImagesCount);

	Check(vkGetSwapchainImagesKHR(m_LogicalDevice, m_Swapchain, &m_ImagesCount, m_SwapchainImages.data()), "Getting swapchain images successful", "Getting swapchain images failed!");
	
	m_SwapchainImageViews.resize(m_ImagesCount);

	for (int i = 0; i < m_ImagesCount; i++)
	{
		VkImageViewCreateInfo imageViewCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = m_SwapchainImages[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = m_ImageFormat,
			.subresourceRange { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
		};

		Check(vkCreateImageView(m_LogicalDevice, &imageViewCreateInfo, nullptr, &m_SwapchainImageViews[i]),
								"Creation of image view" + std::to_string(i) + " successful",
								"Creation of image view " + std::to_string(i) + " failed !!");
	}

	

}

void MyTriangle::CheckDepthFormatAndCreateDepthImage()
{
	std::vector<VkFormat> depthFormatList{ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
	m_DepthFormat = VK_FORMAT_UNDEFINED;
	for (VkFormat& format : depthFormatList)
	{
		VkFormatProperties2 formatProperties
		{
			.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2
		};

		vkGetPhysicalDeviceFormatProperties2(m_PhysicalDevices[m_RequiredPhyDeviceIndex], format, &formatProperties);
		if (formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			m_DepthFormat = format;
			break;
		}
	}

	if (m_DepthFormat == VK_FORMAT_UNDEFINED)
	{
		std::cout << "Could not get required depth format for GPU at index: " << m_RequiredPhyDeviceIndex << std::endl;
		exit(1);
		return;
	}

	std::cout << std::endl;
	std::cout << "Found required depth format for GPU at index: " << m_RequiredPhyDeviceIndex << std::endl;

	m_DepthImageCreateInfo = 
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = m_DepthFormat,
		.extent {.width = m_Width, .height = m_Height, .depth = 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	VmaAllocationCreateInfo allocationCreateInfo
	{
		.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};


	Check(vmaCreateImage(m_Allocator,
		&m_DepthImageCreateInfo,
		&allocationCreateInfo,
		&m_DepthImage,
		&m_DepthImageAllocation,
		nullptr),
		"Depth image creation successful",
		"Depth image creation failed!");
	
	VkImageViewCreateInfo depthImageViewCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = m_DepthImage,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = m_DepthFormat,
		.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1}
	};

	
	Check(vkCreateImageView(m_LogicalDevice, &depthImageViewCreateInfo, nullptr, &m_DepthImageView),
		"Depth Image view creation successful",
		"Depth Image View creation failed!!");
}

void MyTriangle::LoadModelRelatedDataAndShaderRelatedStuff()
{
	std::cout << std::filesystem::current_path() << std::endl;
	
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warn;
	std::string error;
	if (tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &error, "bin/Debug/assets/christmas_latern.obj") != true)
	{
		std::cout << "Model loading failed!!" << std::endl;
		std::cout << "Warning: " << warn << std::endl;
		std::cout << "Error: " << error << std::endl;
		exit(1);
		return;
	}

	std::cout << std::endl;
	std::cout << "Model loading successful" << std::endl;

	m_IndexCount = { shapes[0].mesh.indices.size() };

	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;

	for (auto& index: shapes[0].mesh.indices)
	{
		Vertex vertex
		{
			.pos = { attrib.vertices[index.vertex_index * 3], -attrib.vertices[index.vertex_index * 3 + 1], attrib.vertices[index.vertex_index * 3 + 2]},
			.normal = {attrib.normals[index.normal_index * 3], -attrib.normals[index.normal_index * 3 + 1], attrib.normals[index.normal_index * 3 + 2]},
			.uv = {attrib.texcoords[index.texcoord_index * 2], 1.0f - attrib.normals[index.texcoord_index * 2 + 1]}
		};

		vertices.push_back(vertex);
		indices.push_back(indices.size());
	}

	m_VertexBufferSize = { sizeof(Vertex) * vertices.size() };
	m_IndexBufferSize = { sizeof(uint16_t) * indices.size() };

	VkBufferCreateInfo bufferCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = m_VertexBufferSize + m_IndexBufferSize,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
	};

	VmaAllocationCreateInfo bufferAllocateInfo
	{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | 
				 VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
				 VMA_ALLOCATION_CREATE_MAPPED_BIT,

		.usage = VMA_MEMORY_USAGE_AUTO
	};

	Check(vmaCreateBuffer(m_Allocator, &bufferCreateInfo, &bufferAllocateInfo, &m_Buffer, &m_VertexBufferAllocation, nullptr), 
						  "Buffer creation successful", 
						  "Buffer creation failed!!");

	//Now we will copy all the vertex and index data directly to VRAM.

	if (Check(vmaMapMemory(m_Allocator, m_VertexBufferAllocation, &m_BufferPtr),
		"Mapping memory to buffer successful",
		"Mapping memory to buffer ptr failed!!"))
	{
		memcpy(m_BufferPtr, vertices.data(), m_VertexBufferSize);
		memcpy(((char*)m_BufferPtr) + m_VertexBufferSize, indices.data(), m_IndexBufferSize);
		vmaUnmapMemory(m_Allocator, m_VertexBufferAllocation);

		for (uint32_t i = 0; i < s_MaxFramesInFlight; i++)
		{
			VkBufferCreateInfo uniformBufferCreateInfo
			{
				.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
				.size = sizeof(ShaderData),
				.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
			};

			VmaAllocationCreateInfo uniformBufferAllocateCreateInfo
			{
				.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
						 VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT |
						 VMA_ALLOCATION_CREATE_MAPPED_BIT,

				.usage = VMA_MEMORY_USAGE_AUTO
			};

			Check(vmaCreateBuffer(m_Allocator,
				&uniformBufferCreateInfo,
				&uniformBufferAllocateCreateInfo,
				&m_ShaderDataBuffers[i].buffer,
				&m_ShaderDataBuffers[i].allocation,
				&m_ShaderDataBuffers[i].allocationInfo),
				"Initializing buffer and allocation for shader data buffer at index" + std::to_string(i) + " successful",
				"Initializing buffer and allocation for shader data buffer at index " + std::to_string(i) + " failed!!");

			if (Check(vmaMapMemory(m_Allocator,
				m_ShaderDataBuffers[i].allocation,
				&m_ShaderDataBuffers[i].mapped),
				"Mapping allocation date to VRAM at index" + std::to_string(i) + " successful",
				"Mapping allocation data to VRAM at index " + std::to_string(i) + " failed!!"))
			{
				VkBufferDeviceAddressInfo uniformBufferDeviceAddressAccessInfo
				{
					.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
					.buffer = m_ShaderDataBuffers[i].buffer
				};

				m_ShaderDataBuffers[i].deviceAddress = vkGetBufferDeviceAddress(m_LogicalDevice, &uniformBufferDeviceAddressAccessInfo);
			}
		}
	}


	


}

void MyTriangle::CreateFencesAndSemaphores()
{
	VkSemaphoreCreateInfo semaphoreCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};

	VkFenceCreateInfo fenceCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};

	for (uint32_t i = 0; i < s_MaxFramesInFlight; i++)
	{
		Check(vkCreateFence(m_LogicalDevice, &fenceCreateInfo, nullptr, &m_Fences[i]),
			"Fence creation for index: " + std::to_string(i) + " successful",
			"Fence creation for index: " + std::to_string(i) + " failed!!");

		Check(vkCreateSemaphore(m_LogicalDevice, &semaphoreCreateInfo, nullptr, &m_PresentSemaphores[i]),
			"Present Semaphore creation for index: " + std::to_string(i) + " successful",
			"Present Semaphore creation for index: " + std::to_string(i) + " failed!!");
	}

	m_RenderSemaphores.resize(m_SwapchainImages.size());
	
	for (uint32_t i = 0; i < m_SwapchainImages.size(); i++)
	{
		Check(vkCreateSemaphore(m_LogicalDevice, &semaphoreCreateInfo, nullptr, &m_RenderSemaphores[i]),
			"Render Semaphore creation for index: " + std::to_string(i) + " successful.",
			"Render Semaphore creation for index: " + std::to_string(i) + " failed!!");
	}
}

void MyTriangle::CreateCommandPoolAndCommandBuffers()
{
	VkCommandPoolCreateInfo commandPoolCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = m_QueueFamilyIndex
	};

	Check(vkCreateCommandPool(m_LogicalDevice, &commandPoolCreateInfo, nullptr, &m_CommandPool),
		"Command pool creation successful",
		"Command pool creation failed!!");

	
	VkCommandBufferAllocateInfo commandBufferAllocateInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = m_CommandPool,
		.commandBufferCount = s_MaxFramesInFlight
	};

	Check(vkAllocateCommandBuffers(m_LogicalDevice, &commandBufferAllocateInfo, m_CommandBuffers.data()),
		"Command buffers creation successful",
		"Command Buffers creation failed!!");
}

void MyTriangle::LoadTexture()
{
	ktxTexture* texture{ nullptr };
	std::string fileName = "bin/Debug/assets/christmas_lantern_tex_ktx.ktx";
	
	
	ktx_error_code_e errorCode = ktxTexture_CreateFromNamedFile(
		fileName.c_str(),
		KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
		&texture);

	if (errorCode != KTX_SUCCESS)
	{
		std::cout << "Failed to create texture from file!" << std::endl;
		exit(1);
		return;
	}

	std::cout << std::endl;
	std::cout << "Creation of texture from file successful." << std::endl;

	VkImageCreateInfo textureImageCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = ktxTexture_GetVkFormat(texture),
		.extent = { .width = texture->baseWidth, .height = texture->baseHeight, .depth = 1},
		.mipLevels = texture->numLevels,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	VmaAllocationCreateInfo texImageAllocationCreateInfo{ .usage = VMA_MEMORY_USAGE_AUTO };
	
	Check(vmaCreateImage(m_Allocator,
		&textureImageCreateInfo,
		&texImageAllocationCreateInfo,
		&m_Textures[0].image,
		&m_Textures[0].allocation,
		nullptr),
		"Allocation of texture image is successful.",
		"Failed to create allocation for texture image!");

	VkImageViewCreateInfo texImageViewCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = m_Textures[0].image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = textureImageCreateInfo.format,
		.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = texture->numLevels, .layerCount = 1}
	};

	Check(vkCreateImageView(m_LogicalDevice, &texImageViewCreateInfo, nullptr, &m_Textures[0].view),
		"Creation of texture image view successful",
		"Failed to create texture image view!");

	VkBuffer imgSrcBuffer{};
	VmaAllocation imgSrcAllocation{};

	VkBufferCreateInfo imgSrcBufferCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = (uint32_t)texture->dataSize,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
	};

	VmaAllocationCreateInfo imgSrcAllocCreateInfo
	{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};

	VmaAllocationInfo imgSrcAllocInfo{};

	Check(vmaCreateBuffer(m_Allocator, &imgSrcBufferCreateInfo, &imgSrcAllocCreateInfo, &imgSrcBuffer, &imgSrcAllocation, &imgSrcAllocInfo),
		"Temporary image buffer creation successful",
		"Temporary image buffer creation failed!");

	memcpy(imgSrcAllocInfo.pMappedData, texture->pData, texture->dataSize);

	VkFenceCreateInfo fenceOneTimeCreateInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VkFence fenceOneTime{};
	
	Check(vkCreateFence(m_LogicalDevice, &fenceOneTimeCreateInfo, nullptr, &fenceOneTime),
		"Creation of temporary fence successful", "Creation of temporary fence failed!!");

	VkCommandBuffer tempCommandBuffer{};
	VkCommandBufferAllocateInfo tempCommandBufferAllocateInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = m_CommandPool,
		.commandBufferCount = 1
	};

	Check(vkAllocateCommandBuffers(m_LogicalDevice, &tempCommandBufferAllocateInfo, &tempCommandBuffer),
		"Creation of temporary command buffer successful.",
		"Creation of temporary command buffer failed!");

	//Record the commands required to get image to its destination.
	
	VkCommandBufferBeginInfo tempCommandBufferBeginInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};

	Check(vkBeginCommandBuffer(tempCommandBuffer, &tempCommandBufferBeginInfo), "Command Buffer recording started", "Command Buffer recording failed");

	VkImageMemoryBarrier2 barrierTexImage
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
		.srcAccessMask = VK_ACCESS_2_NONE,
		.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
		.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.image = m_Textures[0].image,
		.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = texture->numLevels, .layerCount = 1}
	};

	VkDependencyInfo barrierTexInfo
	{
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers = &barrierTexImage
	};

	vkCmdPipelineBarrier2(tempCommandBuffer, &barrierTexInfo);
	
	std::vector<VkBufferImageCopy> copyRegions;
	for (uint32_t j = 0; j < texture->numLevels; j++)
	{
		ktx_size_t mipOffset{ 0 };
		KTX_error_code errorCodeStatus = ktxTexture_GetImageOffset(texture, j, 0, 0, &mipOffset);
		
		VkBufferImageCopy copyRegion
		{
			.bufferOffset = mipOffset,
			.imageSubresource{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = j, .layerCount = 1},
			.imageExtent{ .width = texture->baseWidth >> j, .height = texture->baseHeight >> j, .depth = 1}
		};

		copyRegions.push_back(copyRegion);
	}

	vkCmdCopyBufferToImage(tempCommandBuffer, 
						   imgSrcBuffer, 
						   m_Textures[0].image, 
						   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
						   static_cast<uint32_t>(copyRegions.size()), 
						   copyRegions.data());


	VkImageMemoryBarrier2 barrierTexRead
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
		.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
		.image = m_Textures[0].image,
		.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = texture->numLevels, .layerCount = 1}
	};

	barrierTexInfo.pImageMemoryBarriers = &barrierTexRead;
	vkCmdPipelineBarrier2(tempCommandBuffer, &barrierTexInfo);
	Check(vkEndCommandBuffer(tempCommandBuffer), "Command Buffer Ended", "Failed to end command buffer!");

	VkSubmitInfo oneTimeSubmitInfo
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &tempCommandBuffer
	};

	//I am confused what to write in debug message.
	Check(vkQueueSubmit(m_Queue, 1, &oneTimeSubmitInfo, fenceOneTime), "Submitted queue to fence", "Failed to submit queue to fence!");
	Check(vkWaitForFences(m_LogicalDevice, 1, &fenceOneTime, VK_TRUE, UINT64_MAX), "Waiting for fences", "Failed to wait for fences!");

	vkDestroyFence(m_LogicalDevice, fenceOneTime, nullptr);
	vmaDestroyBuffer(m_Allocator, imgSrcBuffer, imgSrcAllocation);
	
	VkSamplerCreateInfo samplerCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.anisotropyEnable = VK_TRUE,
		.maxAnisotropy = 8.0f,
		.maxLod = (float)texture->numLevels
	};

	Check(vkCreateSampler(m_LogicalDevice, &samplerCreateInfo, nullptr, &m_Textures[0].sampler), 
						  "Sampler for texture created", 
						  "Sampler's creation for texture failed");

	
	ktxTexture_Destroy(texture);

	m_TextureDescriptors.clear();

	VkDescriptorImageInfo descriptorImageInfo
	{
		.sampler = m_Textures[0].sampler,
		.imageView = m_Textures[0].view,
		.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
	};

	m_TextureDescriptors.push_back(descriptorImageInfo);

	VkDescriptorBindingFlags descVariableFlag
	{
		VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
	};

	VkDescriptorSetLayoutBindingFlagsCreateInfo descBindingFlags
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.bindingCount = 1,
		.pBindingFlags = &descVariableFlag
	};

	VkDescriptorSetLayoutBinding descLayoutBindingTexture
	{
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = static_cast<uint32_t>(m_Textures.size()),
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
	};

	VkDescriptorSetLayoutCreateInfo descLayoutTexCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = &descBindingFlags,
		.bindingCount = 1,
		.pBindings = &descLayoutBindingTexture
	};

	Check(vkCreateDescriptorSetLayout(m_LogicalDevice, &descLayoutTexCreateInfo, nullptr, &m_DescriptorSetLayoutTex), 
										"Setting descriptor layout for texture successful", 
										"Setting descriptor layout for texture failed!");

	VkDescriptorPoolSize poolSize
	{
		.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = static_cast<uint32_t>(m_Textures.size())
	};

	VkDescriptorPoolCreateInfo poolCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 1,
		.poolSizeCount = 1,
		.pPoolSizes = &poolSize
	};

	Check(vkCreateDescriptorPool(m_LogicalDevice, &poolCreateInfo, nullptr, &m_DescriptorPool), 
								 "Descriptor pool for texture created successfully", 
								 "Failed to create descriptor pool for texture failed!");

	uint32_t variableDescCount = static_cast<uint32_t>(m_Textures.size());

	VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescCountAllocateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
		.descriptorSetCount = 1,
		.pDescriptorCounts = &variableDescCount
	};

	VkDescriptorSetAllocateInfo texDescSetAlloc
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = &variableDescCountAllocateInfo,
		.descriptorPool = m_DescriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &m_DescriptorSetLayoutTex
	};

	Check(vkAllocateDescriptorSets(m_LogicalDevice, &texDescSetAlloc, &m_DescriptorSetTex),
		"Allocated Descriptor set for texture",
		"Allocation of descriptor set for textures failed!");


	VkWriteDescriptorSet writeDescSet
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = m_DescriptorSetTex,
		.dstBinding = 0,
		.descriptorCount = static_cast<uint32_t>(m_TextureDescriptors.size()),
		.pImageInfo = m_TextureDescriptors.data()
	};

	vkUpdateDescriptorSets(m_LogicalDevice, 1, &writeDescSet, 0, nullptr);
}

void MyTriangle::LoadShaders()
{
	Slang::ComPtr<slang::IGlobalSession> globalSession;
	SlangGlobalSessionDesc desc = {};
	slang::createGlobalSession(&desc, globalSession.writeRef());

	auto slangTargets{ std::to_array<slang::TargetDesc>
	(
		{
			{
				.format{SLANG_SPIRV},
				.profile{globalSession->findProfile("spirv_1_4")}
			}
		}
	)};

	auto slangOptions{ std::to_array<slang::CompilerOptionEntry>
	(
		{
			{
				slang::CompilerOptionName::EmitSpirvDirectly,
				{
					slang::CompilerOptionValueKind::Int, 1
				}
			}
		}
	)};

	slang::SessionDesc slangSessionDesc
	{
		.targets{slangTargets.data()},
		.targetCount{SlangInt{slangTargets.size()}},
		.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
		.compilerOptionEntries{ slangOptions.data()},
		.compilerOptionEntryCount{ uint32_t(slangOptions.size())}
	};

	Slang::ComPtr<slang::ISession> slangSession;
	globalSession->createSession(slangSessionDesc, slangSession.writeRef());

	Slang::ComPtr<slang::IModule> slangModule
	{
		slangSession->loadModuleFromSource("triangle", "bin/Debug/assets/shader.slang", nullptr, nullptr)
	};

	Slang::ComPtr<ISlangBlob> spirv;
	slangModule->getTargetCode(0, spirv.writeRef());

	VkShaderModuleCreateInfo shaderModuleCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = spirv->getBufferSize(),
		.pCode = (uint32_t*)spirv->getBufferPointer()
	};

	
	Check(vkCreateShaderModule(m_LogicalDevice, &shaderModuleCreateInfo, nullptr, &m_ShaderModule), "Created Shader Module Successfully", "Creation of shader module failed!!");
}

void MyTriangle::SetupGraphicsPipeline()
{
	VkPushConstantRange pushConstantRange
	{
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.size = sizeof(VkDeviceAddress)
	};

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &m_DescriptorSetLayoutTex,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstantRange
	};

	Check(vkCreatePipelineLayout(m_LogicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_PipelineLayout), "Created Pipeline Layout successfully.", "Pipeline layout creation failed!");

	VkVertexInputBindingDescription vertexBinding
	{
		.binding = 0,
		.stride = sizeof(Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
	};

	std::vector<VkVertexInputAttributeDescription> vertexAttributes
	{
		{ .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT},
		{ .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(Vertex, normal)},
		{ .location = 2, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = offsetof(Vertex, uv)}
	};

	VkPipelineVertexInputStateCreateInfo vertexInputState
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vertexBinding,
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size()),
		.pVertexAttributeDescriptions = vertexAttributes.data()
	};

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
	};

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages
	{
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = m_ShaderModule,
			.pName = "main"
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = m_ShaderModule,
			.pName = "main"
		}
	};

	VkPipelineViewportStateCreateInfo viewportState
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1
	};

	std::vector<VkDynamicState> dynamicStates{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	
	VkPipelineDynamicStateCreateInfo dynamicState
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = dynamicStates.data()
	};

	VkPipelineDepthStencilStateCreateInfo depthStencilState
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL
	};

	VkPipelineRenderingCreateInfo renderingCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &m_ImageFormat,
		.depthAttachmentFormat = m_DepthFormat
	};

	VkPipelineColorBlendAttachmentState blendAttachment
	{
		.colorWriteMask = 0xF
	};

	VkPipelineColorBlendStateCreateInfo colorBlendState
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &blendAttachment
	};

	VkPipelineRasterizationStateCreateInfo rasterizationState
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.lineWidth = 1.0f
	};

	VkPipelineMultisampleStateCreateInfo multisampleState
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};

	VkGraphicsPipelineCreateInfo pipelineCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &renderingCreateInfo,
		.stageCount = 2,
		.pStages = shaderStages.data(),
		.pVertexInputState = &vertexInputState,
		.pInputAssemblyState = &inputAssemblyState,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizationState,
		.pMultisampleState = &multisampleState,
		.pDepthStencilState = &depthStencilState,
		.pColorBlendState = &colorBlendState,
		.pDynamicState = &dynamicState,
		.layout = m_PipelineLayout
	};

	Check(vkCreateGraphicsPipelines(m_LogicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_GraphicsPipeline), "Graphics pipeline created.", "Graphics pipeline creation failed!");
}








