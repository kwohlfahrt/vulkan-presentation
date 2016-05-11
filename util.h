#pragma once

#include <vulkan/vulkan.h>

VKAPI_ATTR VkBool32 VKAPI_CALL
debugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
                    uint64_t object, size_t location, int32_t messageCode,
                    const char* pLayerPrefix, const char* pMessage, void* pUserData);

size_t loadModule(char * filename, uint32_t ** data);
