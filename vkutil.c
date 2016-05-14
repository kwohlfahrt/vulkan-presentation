#include "vkutil.h"

#include <stdio.h>
#include <assert.h>

VKAPI_ATTR VkBool32 VKAPI_CALL
debugReportCallback(VkDebugReportFlagsEXT flags __attribute__((unused)),
                    VkDebugReportObjectTypeEXT objectType __attribute__((unused)),
                    uint64_t object __attribute__((unused)),
                    size_t location __attribute__((unused)),
                    int32_t messageCode __attribute__((unused)),
                    const char* pLayerPrefix __attribute__((unused)),
                    const char* pMessage, void* pUserData __attribute__((unused))){
    fprintf(stdout, "%s\n", pMessage);
    return VK_FALSE;
}

void createFrameImage(VkDevice device, VkExtent2D size,
                      VkFormat format, VkSampleCountFlagBits samples,
                      VkImageUsageFlags usage, VkImageAspectFlags aspect,
                      VkImage* image, VkDeviceMemory* memory, VkImageView* view) {
    {
        VkImageCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = {size.width, size.height, 1,},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = samples,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        assert(vkCreateImage(device, &create_info, NULL, image) == VK_SUCCESS);
    }
    {
        VkMemoryRequirements memory_requirements;
        vkGetImageMemoryRequirements(device, *image, &memory_requirements);

        VkMemoryAllocateInfo allocate_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = NULL,
            .allocationSize = memory_requirements.size,
            .memoryTypeIndex = 0,
        };
        assert(vkAllocateMemory(device, &allocate_info, NULL, memory) == VK_SUCCESS);
    }
    assert(vkBindImageMemory(device, *image, *memory, 0) == VK_SUCCESS);
    {
        VkImageViewCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .image = *image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = aspect,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        assert(vkCreateImageView(device, &create_info, NULL, view) == VK_SUCCESS);
    }
}

void cmdPrepareFrameImage(VkCommandBuffer cmd_buffer, VkImage image, VkAccessFlags access,
                          VkImageLayout layout, VkImageAspectFlags aspect) {
    VkImageMemoryBarrier layout_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = 0,
        .dstAccessMask = access,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = layout,
        .image = image,
        .subresourceRange = {
            .aspectMask = aspect,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &layout_barrier);
}

void createFramebuffer(VkDevice device, VkExtent2D size,
                       uint32_t nviews, VkImageView* views,
                       VkRenderPass render_pass, VkFramebuffer* framebuffer){
    VkFramebufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .renderPass = render_pass,
        .attachmentCount = nviews,
        .pAttachments = views,
        .width = size.width,
        .height = size.height,
        .layers = 1,
    };
    assert(vkCreateFramebuffer(device, &create_info, NULL, framebuffer) == VK_SUCCESS);
}