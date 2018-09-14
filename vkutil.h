#pragma once

#include <vulkan/vulkan.h>

VKAPI_ATTR VkBool32 VKAPI_CALL
debugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
                    uint64_t object, size_t location, int32_t messageCode,
                    const char* pLayerPrefix, const char* pMessage, void* pUserData);

void createFrameImage(VkPhysicalDeviceMemoryProperties memory_properties, VkDevice device,
                      VkExtent2D size, VkFormat format, VkSampleCountFlagBits samples,
                      VkImageUsageFlags usage, VkImageAspectFlags aspect,
                      VkImage* image, VkDeviceMemory* memory, VkImageView* view);

void createFramebuffer(VkDevice device, VkExtent2D size,
                       uint32_t nviews, VkImageView* views,
                       VkRenderPass render_pass, VkFramebuffer* framebuffer);

void createBuffer(VkPhysicalDeviceMemoryProperties memory_properties, VkDevice device,
                  VkBufferUsageFlags usage, size_t size, const void * data,
                  VkBuffer* buffer, VkDeviceMemory* memory);

int32_t findMemory(VkPhysicalDeviceMemoryProperties properties,
                   uint32_t memory_type_bits, VkMemoryPropertyFlags requirements);
