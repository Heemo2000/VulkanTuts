#include "MyTriangle.h"
#include <filesystem>


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
	LoadModelRelatedDataAndShaderRelatedStuff();
	CreateFencesAndSemaphores();
	CreateCommandPoolAndCommandBuffers();
	LoadTexture();
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

	Check(vkCreateSwapchainKHR(m_LogicalDevice, &swapchainCreateInfo, nullptr, &m_Swapchain), "Swapchain creation successful", "Swapchain creation failed!");

	uint32_t imagesCount = 0;
	Check(vkGetSwapchainImagesKHR(m_LogicalDevice, m_Swapchain, &imagesCount, nullptr), "Getting swapchain images count successful", "Getting swapchain images count failed!");
	
	m_SwapchainImages.resize(imagesCount);

	Check(vkGetSwapchainImagesKHR(m_LogicalDevice, m_Swapchain, &imagesCount, m_SwapchainImages.data()), "Getting swapchain images successful", "Getting swapchain images failed!");
	
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

		Check(vkCreateImageView(m_LogicalDevice, &imageViewCreateInfo, nullptr, &m_SwapchainImageViews[i]),
								"Creation of image view" + std::to_string(i) + " successful",
								"Creation of image view " + std::to_string(i) + " failed !!");
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


	Check(vmaCreateImage(m_Allocator,
		&depthImageCreateInfo,
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
		.format = depthFormat,
		.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1}
	};

	
	Check(vkCreateImageView(m_LogicalDevice, &depthImageViewCreateInfo, nullptr, &m_DepthImageView),
		"Depth Image view creation successful",
		"Depth Image View creation failed!!");
}

void MyTriangle::LoadModelRelatedDataAndShaderRelatedStuff()
{
	//std::cout << std::filesystem::current_path() << std::endl;
	
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warn;
	std::string error;
	if (tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &error, "assets/christmas_latern.obj") != true)
	{
		std::cout << "Model loading failed!!" << std::endl;
		std::cout << "Warning: " << warn << std::endl;
		std::cout << "Error: " << error << std::endl;
		exit(1);
		return;
	}

	std::cout << std::endl;
	std::cout << "Model loading successful" << std::endl;

	const VkDeviceSize indexCount{ shapes[0].mesh.indices.size() };

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

	VkDeviceSize vertexBufferSize{ sizeof(Vertex) * vertices.size() };
	VkDeviceSize indexBufferSize{ sizeof(uint16_t) * indices.size() };

	VkBufferCreateInfo bufferCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = vertexBufferSize + indexBufferSize,
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
		memcpy(m_BufferPtr, vertices.data(), vertexBufferSize);
		memcpy(((char*)m_BufferPtr) + vertexBufferSize, indices.data(), indexBufferSize);
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
				nullptr),
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
	std::string fileName = "assets/christmas_lantern_tex_ktx.ktx";
	
	
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
	

}


