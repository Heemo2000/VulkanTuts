cmake_minimum_required(VERSION 3.28)
project(VulkanTuts VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


find_package(Vulkan REQUIRED)
find_package(ktx CONFIG REQUIRED)

get_property(allTargets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
message(STATUS "All targets: ${allTargets}")


file(GLOB_RECURSE VULKAN_TUTS_SOURCE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src"
    PREFIX "VulkanTuts"
    FILES ${VULKAN_TUTS_SOURCE_FILES}
)

add_subdirectory(third-party/glfw)
add_subdirectory(third-party/glm)
add_subdirectory(third-party/tinyobjloader)
add_subdirectory(third-party/VulkanMemoryAllocator)

set(SLANG_ROOT "${CMAKE_SOURCE_DIR}/third-party/slang")

add_library(slang SHARED IMPORTED)

set_target_properties(slang PROPERTIES
                      IMPORTED_LOCATION "${SLANG_ROOT}/bin/slang.dll"
                      IMPORTED_IMPLIB "${SLANG_ROOT}/lib/slang.lib"
                      INTERFACE_INCLUDE_DIRECTORIES "${SLANG_ROOT}/include")


add_executable(VulkanTuts ${VULKAN_TUTS_SOURCE_FILES})

target_link_libraries(VulkanTuts PRIVATE
			glfw
			glm::glm
			KTX::ktx
			tinyobjloader
			GPUOpen::VulkanMemoryAllocator
            slang
			Vulkan::Vulkan
		      )


add_custom_command(TARGET VulkanTuts POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Copying assets..."
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/src/assets"
            "$<TARGET_FILE_DIR:VulkanTuts>/assets"

    COMMAND ${CMAKE_COMMAND} -E echo "Copying slang.dll..."
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SLANG_ROOT}/bin/slang.dll"
            "${SLANG_ROOT}/bin/slang-compiler.dll"
            "$<TARGET_FILE_DIR:VulkanTuts>"
)


set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTY VS_STARTUP_PROJECT VulkanTuts)

