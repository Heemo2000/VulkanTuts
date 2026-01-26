#include "MyTriangle.h"

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
	while (!glfwWindowShouldClose(m_Window))
	{
		glfwPollEvents();
	}
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
}

void MyTriangle::SetResolution(uint32_t width, uint32_t height)
{
	m_Width = width;
	m_Height = height;

	std::cout << "Resolution: " << m_Width << " X " << m_Height << std::endl;
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

	if (vkCreateInstance(&instanceCreateInfo, nullptr, &m_Instance) != VK_SUCCESS)
	{
		std::cout << "Failed to create instance!!" << std::endl;
		exit(1);
		return;
	}
	else
	{
		std::cout << "Instance creation successful" << std::endl;
	}
}

void MyTriangle::FindAndSelectPhysicalDevice()
{
	uint32_t deviceCount = 0;
	if (vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr) != VK_SUCCESS)
	{
		std::cout << "No GPU Device Found! Count: 0" << std::endl;
		exit(1);
		return;
	}

	m_PhysicalDevices.resize(deviceCount);

	if (vkEnumeratePhysicalDevices(m_Instance, &deviceCount, m_PhysicalDevices.data()) != VK_SUCCESS)
	{
		std::cout << "No GPU Device Found!" << std::endl;
		exit(0);
		return;
	}

	std::cout  << std::endl << "Found some GPU Devices: " << std::endl;
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

	if (vkCreateDevice(m_PhysicalDevices[m_RequiredPhyDeviceIndex], 
					   &deviceCreateInfo, 
					   nullptr, 
					   &m_LogicalDevice) != 
					   VK_SUCCESS)
	{
		std::cout << "Failed to create Logical Device!" << std::endl;
		exit(1);
		return;
	}
	else
	{
		std::cout << std::endl;
		std::cout << "Logical Device Creation successful" << std::endl;
	}

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

	if (vmaCreateAllocator(&allocatorCreateInfo, &m_Allocator) != VK_SUCCESS)
	{
		std::cout << "Failed to create memory allocator!" << std::endl;
		exit(1);
		return;
	}
	else
	{
		std::cout << std::endl;
		std::cout << "Memory allocator creation successful" << std::endl;
	}
}

void MyTriangle::CreateSurface()
{
	if (glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface) != VK_SUCCESS)
	{
		std::cout << "Surface creation failed!" << std::endl;
		exit(1);
		return;
	}
	else
	{
		std::cout << std::endl;
		std::cout << "Surface creation successful" << std::endl;
	}

	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		m_PhysicalDevices[m_RequiredPhyDeviceIndex], 
		m_Surface, 
		&m_SurfaceCapabilities) != VK_SUCCESS)
	{
		std::cout << "Failed to get physical device surface capabilities info!" << std::endl;
		exit(1);
		return;
	}
	else
	{
		std::cout << std::endl;
		std::cout << "Getting Physical Device Surface Capabilities Info successful" << std::endl;
	}
}

void MyTriangle::CreateSwapchainAndImageViews()
{
	const VkFormat imageFormat{ VK_FORMAT_B8G8R8A8_SRGB };
	VkSwapchainCreateInfoKHR swapchainCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = m_Surface,
		.minImageCount = m_SurfaceCapabilities.minImageCount,
		.imageFormat = imageFormat,
		.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
		.imageExtent {.width = m_SurfaceCapabilities.currentExtent.width, .height = m_SurfaceCapabilities.currentExtent.height},
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = VK_PRESENT_MODE_FIFO_KHR
	};

	if (vkCreateSwapchainKHR(m_LogicalDevice, &swapchainCreateInfo, nullptr, &m_Swapchain) != VK_SUCCESS)
	{
		std::cout << "Swapchain creation failed!" << std::endl;
		exit(1);
		return;
	}
	else
	{
		std::cout << std::endl;
		std::cout << "Swapchain creation successful" << std::endl;
	}

	uint32_t imagesCount = 0;
	if (vkGetSwapchainImagesKHR(m_LogicalDevice, m_Swapchain, &imagesCount, nullptr) != VK_SUCCESS)
	{
		std::cout << "Getting swapchain images count failed!" << std::endl;
		exit(1);
		return;
	}

	std::cout << std::endl;
	std::cout << "Getting swapchain images count successful" << std::endl;

	m_SwapchainImages.resize(imagesCount);

	if (vkGetSwapchainImagesKHR(m_LogicalDevice, m_Swapchain, &imagesCount, m_SwapchainImages.data()) != VK_SUCCESS)
	{
		std::cout << "Getting swapchain images failed!" << std::endl;
		exit(1);
		return;
	}


	std::cout << std::endl;
	std::cout << "Getting swapchain images successful" << std::endl;

	m_SwapchainImageViews.resize(imagesCount);

	for (int i = 0; i < imagesCount; i++)
	{
		VkImageViewCreateInfo imageViewCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = m_SwapchainImages[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = imageFormat,
			.subresourceRange { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
		};

		if (vkCreateImageView(m_LogicalDevice, &imageViewCreateInfo, nullptr, &m_SwapchainImageViews[i]) != VK_SUCCESS)
		{
			std::cout << "Creation of image view " << i << " failed !!" << std::endl;
			exit(1);
			return;
		}

		std::cout << std::endl;
		std::cout << "Creation of image view" << i << " successful" << std::endl;
	}

	

}

void MyTriangle::CheckDepthFormatAndCreateDepthImage()
{
	std::vector<VkFormat> depthFormatList{ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
	VkFormat depthFormat = VK_FORMAT_UNDEFINED;
	for (VkFormat& format : depthFormatList)
	{
		VkFormatProperties2 formatProperties
		{
			.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2
		};

		vkGetPhysicalDeviceFormatProperties2(m_PhysicalDevices[m_RequiredPhyDeviceIndex], format, &formatProperties);
		if (formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			depthFormat = format;
			break;
		}
	}

	if (depthFormat == VK_FORMAT_UNDEFINED)
	{
		std::cout << "Could not get required depth format for GPU at index: " << m_RequiredPhyDeviceIndex << std::endl;
		exit(1);
		return;
	}

	std::cout << std::endl;
	std::cout << "Found required depth format for GPU at index: " << m_RequiredPhyDeviceIndex << std::endl;

	VkImageCreateInfo depthImageCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = depthFormat,
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

	if (vmaCreateImage(m_Allocator,
		&depthImageCreateInfo,
		&allocationCreateInfo,
		&m_DepthImage,
		&m_DepthImageAllocation,
		nullptr) != VK_SUCCESS)
	{
		std::cout << "Depth image creation failed!" << std::endl;
		exit(1);
		return;
	}


}
