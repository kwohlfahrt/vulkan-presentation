#include "tiff.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <vulkan/vulkan.h>

#define NELEMS(x) (sizeof(x) / sizeof(*x))

const VkExtent2D render_size = {
    .width = 1920,
    .height = 1440,
};
const size_t nchannels = 4;

struct Vertex {
    float pos[2] __attribute__((aligned(16)));
};

const struct Vertex lines[3][2] = {
    {{.pos={ 0.0,-0.5}}, {.pos={ 0.5, 0.5}}},
    {{.pos={ 0.5, 0.5}}, {.pos={-0.5, 0.5}}},
    {{.pos={-0.5, 0.5}}, {.pos={ 0.0,-0.5}}},
};

VKAPI_ATTR VkBool32 VKAPI_CALL
debugReportCallback(VkDebugReportFlagsEXT flags __attribute__((unused)),
                    VkDebugReportObjectTypeEXT objectType __attribute__((unused)),
                    uint64_t __attribute__((unused)) object,
                    size_t __attribute__((unused)) location,
                    int32_t __attribute__((unused)) messageCode,
                    const __attribute__((unused)) char* pLayerPrefix,
                    const char* pMessage, void* __attribute__((unused)) pUserData){
    fprintf(stdout, "%s\n", pMessage);
    return VK_FALSE;
}

size_t loadModule(char * filename, uint32_t ** data) {
    FILE * file;
    size_t size;
    *data = NULL;

    file = fopen(filename, "rb");
    if (file == NULL)
        return 0;

    fseek(file, 0, SEEK_END);
    size = ftell(file);
    rewind(file);
    *data = malloc(size);

    if (data == NULL)
        goto cleanup;

    if (fread(*data, 1, size, file) != size)
        goto cleanup;

    return size;

 cleanup:
    fclose(file);
    free(*data);
    return 0;
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

void cmdDraw(VkCommandBuffer draw_buffer, VkExtent2D size,
             VkPipeline pipeline, VkPipelineLayout pipeline_layout,
             VkDescriptorSet descriptor_set,
             VkRenderPass render_pass, VkFramebuffer framebuffer){
    VkClearValue clear_values[1] = {{
            .color.float32 = {0.1, 0.1, 0.1, 1.0},
        }};
    VkRenderPassBeginInfo renderpass_begin_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .renderPass = render_pass,
        .framebuffer = framebuffer,
        .renderArea = {
            .offset = {.x = 0, .y = 0},
            .extent = size,
        },
        .clearValueCount = NELEMS(clear_values),
        .pClearValues = clear_values,
    };
    vkCmdBeginRenderPass(draw_buffer, &renderpass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(draw_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(draw_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
                            0, 1, &descriptor_set, 0, NULL);

    VkRect2D scissor= {
        .offset = {.x = 0, .y = 0,},
        .extent = size,
    };
    vkCmdSetScissor(draw_buffer, 0, 1, &scissor);
    VkViewport viewport = {
        .x = 0,
        .y = 0,
        .width = size.width,
        .height = size.height,
        .minDepth = 0.0,
        .maxDepth = 1.0,
    };
    vkCmdSetViewport(draw_buffer, 0, 1, &viewport);

    vkCmdDraw(draw_buffer, 4, 1, 0, 1);
    vkCmdEndRenderPass(draw_buffer);
}

int main(void) {
    VkInstance instance;
    {
        const char debug_ext[] = "VK_EXT_debug_report";
        const char* extensions[] = {debug_ext,};

        const char validation_layer[] = "VK_LAYER_LUNARG_standard_validation";
        const char* layers[] = {validation_layer,};

        VkInstanceCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .pApplicationInfo = NULL,
            .enabledLayerCount = NELEMS(layers),
            .ppEnabledLayerNames = layers,
            .enabledExtensionCount = NELEMS(extensions),
            .ppEnabledExtensionNames = extensions,
        };
        assert(vkCreateInstance(&create_info, NULL, &instance) == VK_SUCCESS);
    }

    VkDebugReportCallbackEXT debug_callback;
    {
        VkDebugReportCallbackCreateInfoEXT create_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT,
            .pNext = NULL,
            .flags = (VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
                      VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT),
            .pfnCallback = &debugReportCallback,
            .pUserData = NULL,
        };

        PFN_vkCreateDebugReportCallbackEXT createDebugReportCallback =
            (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
        assert(createDebugReportCallback);
        assert(createDebugReportCallback(instance, &create_info, NULL, &debug_callback) == VK_SUCCESS);
    }

    VkPhysicalDevice phy_device;
    {
        uint32_t num_devices;
        vkEnumeratePhysicalDevices(instance, &num_devices, NULL);
        assert(num_devices >= 1);
        num_devices = 1;
        assert(vkEnumeratePhysicalDevices(instance, &num_devices, &phy_device) == VK_SUCCESS);
    }

    VkDevice device;
    {
        float queue_priorities[] = {1.0};
        const char validation_layer[] = "VK_LAYER_LUNARG_standard_validation";
        const char* layers[] = {validation_layer,};

        VkDeviceQueueCreateInfo queue_info[] = {{
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .queueFamilyIndex = 0,
                .queueCount = 1,
                .pQueuePriorities = queue_priorities,
            }};

        VkDeviceCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .queueCreateInfoCount = NELEMS(queue_info),
            .pQueueCreateInfos = queue_info,
            .enabledLayerCount = NELEMS(layers),
            .ppEnabledLayerNames = layers,
            .enabledExtensionCount = 0,
            .ppEnabledExtensionNames = NULL,
            .pEnabledFeatures = NULL,
        };

        assert(vkCreateDevice(phy_device, &create_info, NULL, &device) == VK_SUCCESS);
    }

    VkQueue queue;
    vkGetDeviceQueue(device, 0, 0, &queue);

    VkCommandPool cmd_pool;
    {
        VkCommandPoolCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = NULL,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = 0,
        };
        assert(vkCreateCommandPool(device, &create_info, NULL, &cmd_pool) == VK_SUCCESS);
    }

    VkRenderPass render_pass;
    {
        VkAttachmentDescription attachments[] = {{
                .flags = 0,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            }};
        VkAttachmentReference attachment_refs[NELEMS(attachments)] = {{
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            }};
        VkSubpassDescription subpasses[1] = {{
                .flags = 0,
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .inputAttachmentCount = 0,
                .pInputAttachments = NULL,
                .colorAttachmentCount = 1,
                .pColorAttachments = &attachment_refs[0],
                .pResolveAttachments = NULL,
                .pDepthStencilAttachment = NULL,
                .preserveAttachmentCount = 0,
                .pPreserveAttachments = NULL,
            }};
        VkRenderPassCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .attachmentCount = NELEMS(attachments),
            .pAttachments = attachments,
            .subpassCount = NELEMS(subpasses),
            .pSubpasses = subpasses,
            .dependencyCount = 0,
            .pDependencies = NULL,
        };
        assert(vkCreateRenderPass(device, &create_info, NULL, &render_pass) == VK_SUCCESS);
    }

    VkImage color_image;
    VkDeviceMemory color_image_memory;
    VkImageView color_view;
    createFrameImage(device, render_size, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT,
                     &color_image, &color_image_memory, &color_view);

    VkBuffer lines_buffer;
    VkDeviceMemory lines_memory;
    {
        VkBufferCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .size = sizeof(lines),
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        };
        assert(vkCreateBuffer(device, &create_info, NULL, &lines_buffer) == VK_SUCCESS);
        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(device, lines_buffer, &memory_requirements);

        VkMemoryAllocateInfo allocate_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = NULL,
            .allocationSize = memory_requirements.size,
            // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISISBLE_BIT
            // TODO: Use staging buffer
            .memoryTypeIndex = 0,
        };
        assert(vkAllocateMemory(device, &allocate_info, NULL, &lines_memory) == VK_SUCCESS);
        assert(vkBindBufferMemory(device, lines_buffer, lines_memory, 0) == VK_SUCCESS);

        void * data;
        assert(vkMapMemory(device, lines_memory, 0, VK_WHOLE_SIZE, 0, &data) == VK_SUCCESS);
        memcpy(data, lines, sizeof(lines));
        VkMappedMemoryRange flush_range = {
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, .pNext = NULL,
            .memory = lines_memory,
            .offset = 0,
            .size = VK_WHOLE_SIZE,
        };
        assert(vkFlushMappedMemoryRanges(device, 1, &flush_range) == VK_SUCCESS);
        vkUnmapMemory(device, lines_memory);
    }

    VkBuffer image_buffer;
    VkDeviceMemory image_buffer_memory;
    {
        VkBufferCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .size = render_size.height * render_size.width * nchannels,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        };
        assert(vkCreateBuffer(device, &create_info, NULL, &image_buffer) == VK_SUCCESS);
        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(device, image_buffer, &memory_requirements);

        VkMemoryAllocateInfo allocate_info = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = NULL,
            .allocationSize = memory_requirements.size,
            .memoryTypeIndex = 0, //VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        };
        assert(vkAllocateMemory(device, &allocate_info, NULL, &image_buffer_memory) == VK_SUCCESS);
        assert(vkBindBufferMemory(device, image_buffer, image_buffer_memory, 0) == VK_SUCCESS);
    }

    VkFramebuffer framebuffer;
    createFramebuffer(device, render_size, 1, &color_view, render_pass, &framebuffer);

    VkShaderModule vertex_shader, fragment_shader;
    {
        size_t code_sizes[2];
        uint32_t * code[2];
        assert((code_sizes[0] = loadModule("rasterize.vert.spv", &code[0])) != 0);
        assert((code_sizes[1] = loadModule("rasterize.frag.spv", &code[1])) != 0);

        VkShaderModuleCreateInfo create_info[2] = {{
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .codeSize = code_sizes[0],
                .pCode = code[0],
            }, {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .codeSize = code_sizes[1],
                .pCode = code[1],
            }};
        assert(vkCreateShaderModule(device, &create_info[0], NULL, &vertex_shader) == VK_SUCCESS);
        assert(vkCreateShaderModule(device, &create_info[1], NULL, &fragment_shader) == VK_SUCCESS);
        free(code[0]);
        free(code[1]);
    }

    VkDescriptorSetLayout descriptor_set_layout;
    {
        VkDescriptorSetLayoutBinding bindings[1] = {{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = NULL,
            }};

        VkDescriptorSetLayoutCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .bindingCount = NELEMS(bindings),
            .pBindings = bindings,
        };

        assert(vkCreateDescriptorSetLayout(device, &create_info, NULL, &descriptor_set_layout) == VK_SUCCESS);
    }

    VkDescriptorPool descriptor_pool;
    {
        VkDescriptorPoolSize pool_sizes[1] = {{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            }};
        VkDescriptorPoolCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .maxSets = 1,
            .poolSizeCount = NELEMS(pool_sizes),
            .pPoolSizes = pool_sizes,
        };
        assert(vkCreateDescriptorPool(device, &create_info, NULL, &descriptor_pool) == VK_SUCCESS);
    }

    VkDescriptorSet descriptor_set;
    {
        VkDescriptorSetAllocateInfo allocate_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = NULL,
            .descriptorPool = descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &descriptor_set_layout,
        };
        assert(vkAllocateDescriptorSets(device, &allocate_info, &descriptor_set) == VK_SUCCESS);

        VkDescriptorBufferInfo buffer_info = {
            .buffer = lines_buffer,
            .offset = 0,
            .range = VK_WHOLE_SIZE,
        };
        VkWriteDescriptorSet writes[1] = {{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = NULL,
                .dstSet = descriptor_set,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &buffer_info,
            }};
        vkUpdateDescriptorSets(device, 1, writes, 0, NULL);
    }

    VkPipelineLayout pipeline_layout;
    {
        VkPipelineLayoutCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .setLayoutCount = 1,
            .pSetLayouts = &descriptor_set_layout,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = NULL,
        };
        assert(vkCreatePipelineLayout(device, &create_info, NULL, &pipeline_layout) == VK_SUCCESS);
    }

    VkPipeline pipeline;
    {
        VkPipelineShaderStageCreateInfo stages[2] = {{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vertex_shader,
                .pName = "main",
                .pSpecializationInfo = NULL,
            },{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = fragment_shader,
                .pName = "main",
                .pSpecializationInfo = NULL,
            }};
        VkPipelineVertexInputStateCreateInfo vtx_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .vertexBindingDescriptionCount = 0,
            .pVertexBindingDescriptions = NULL,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions = NULL,
        };
        VkPipelineInputAssemblyStateCreateInfo ia_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
            .primitiveRestartEnable = VK_FALSE,
        };
        VkPipelineViewportStateCreateInfo viewport_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .viewportCount = 1,
            .pViewports = NULL, // Dynamic
            .scissorCount = 1,
            .pScissors = NULL, // Dynamic
        };
        VkPipelineRasterizationStateCreateInfo rasterization_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .lineWidth = 1.0,
        };
        VkPipelineMultisampleStateCreateInfo multisample_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 0.0,
            .pSampleMask = NULL,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE,
        };
        VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .depthTestEnable = VK_FALSE,
            .depthWriteEnable = VK_FALSE,
            .depthCompareOp = VK_COMPARE_OP_ALWAYS,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
            .front = {},
            .back = {},
            .minDepthBounds = 0.0,
            .maxDepthBounds = 1.0,
        };
        VkPipelineColorBlendAttachmentState color_blend_attachment = {
            .blendEnable = VK_FALSE,
            .colorWriteMask = ( VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT),
        };
        VkPipelineColorBlendStateCreateInfo color_blend_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .logicOpEnable = VK_FALSE, //.logicOp = 0,
            .attachmentCount = 1,
            .pAttachments = &color_blend_attachment,
            .blendConstants = {},
        };

        // TODO: Make static
        VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .dynamicStateCount = NELEMS(dynamic_states),
            .pDynamicStates = dynamic_states,
        };

        VkGraphicsPipelineCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .stageCount = NELEMS(stages),
            .pStages = stages,
            .pVertexInputState = &vtx_state,
            .pInputAssemblyState = &ia_state,
            .pTessellationState = NULL,
            .pViewportState = &viewport_state,
            .pRasterizationState = &rasterization_state,
            .pMultisampleState = &multisample_state,
            .pDepthStencilState = &depth_stencil_state,
            .pColorBlendState = &color_blend_state,
            .pDynamicState = &dynamic_state,
            .layout = pipeline_layout,
            .renderPass = render_pass,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = 0,
        };
        assert(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &create_info, NULL, &pipeline) == VK_SUCCESS);
    }

    VkCommandBuffer setup_buffer;
    VkCommandBuffer draw_buffer;
    {
        VkCommandBufferAllocateInfo allocate_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = NULL,
            .commandPool = cmd_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        assert(vkAllocateCommandBuffers(device, &allocate_info, &setup_buffer) == VK_SUCCESS);
        assert(vkAllocateCommandBuffers(device, &allocate_info, &draw_buffer) == VK_SUCCESS);
    }

    {
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = 0,
            .pInheritanceInfo = NULL,
        };
        assert(vkBeginCommandBuffer(setup_buffer, &begin_info) == VK_SUCCESS);
        cmdPrepareFrameImage(setup_buffer, color_image, VK_ACCESS_TRANSFER_READ_BIT,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        assert(vkEndCommandBuffer(setup_buffer) == VK_SUCCESS);

        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = NULL,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = NULL,
            .pWaitDstStageMask = NULL,
            .commandBufferCount = 1,
            .pCommandBuffers = &setup_buffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = NULL,
        };

        assert(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE) == VK_SUCCESS);
    }

    {
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = 0,
            .pInheritanceInfo = NULL,
        };
        assert(vkBeginCommandBuffer(draw_buffer, &begin_info) == VK_SUCCESS);
        cmdDraw(draw_buffer, render_size, pipeline, pipeline_layout, descriptor_set, render_pass, framebuffer);

        VkImageMemoryBarrier draw_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .image = color_image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        vkCmdPipelineBarrier(draw_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &draw_barrier);
        VkBufferImageCopy copy = {
            .bufferOffset = 0,
            .bufferRowLength = 0, // Tightly packed
            .bufferImageHeight = 0, // Tightly packed
            .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
            .imageOffset = {0, 0, 0},
            .imageExtent = {.width = render_size.width,
                            .height = render_size.height,
                            .depth = 1},
        };
        vkCmdCopyImageToBuffer(draw_buffer, color_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               image_buffer, 1, &copy);

        VkBufferMemoryBarrier transfer_barrier = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .pNext = 0,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = image_buffer,
            .offset = 0,
            .size = VK_WHOLE_SIZE,
        };
        vkCmdPipelineBarrier(draw_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &transfer_barrier, 0, NULL);
        assert(vkEndCommandBuffer(draw_buffer) == VK_SUCCESS);
    }

    VkFence fence;
    {
        VkFenceCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = 0,
            .flags = 0,
        };
        assert(vkCreateFence(device, &create_info, NULL, &fence) == VK_SUCCESS);
    }

    // Wait for set-up to finish
    assert(vkQueueWaitIdle(queue) == VK_SUCCESS);
    {
        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = NULL,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = NULL,
            .pWaitDstStageMask = NULL,
            .commandBufferCount = 1,
            .pCommandBuffers = &draw_buffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = NULL,
        };
        assert(vkQueueSubmit(queue, 1, &submit_info, fence) == VK_SUCCESS);

        assert(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS);
        char * image_data;
        assert(vkMapMemory(device, image_buffer_memory, 0, VK_WHOLE_SIZE,
                           0, (void **) &image_data) == VK_SUCCESS);
        VkMappedMemoryRange flush_range = {
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, .pNext = NULL,
            .memory = image_buffer_memory,
            .offset = 0,
            .size = VK_WHOLE_SIZE,
        };
        assert(vkInvalidateMappedMemoryRanges(device, 1, &flush_range) == VK_SUCCESS);
        assert(writeTiff("rasterize.tif", image_data, render_size, nchannels) == 0);
        vkUnmapMemory(device, image_buffer_memory);
    }

    assert(vkQueueWaitIdle(queue) == VK_SUCCESS);
    vkDestroyFence(device, fence, NULL);

    vkDestroyFramebuffer(device, framebuffer, NULL);
    vkDestroyImage(device, color_image, NULL);
    vkDestroyImageView(device, color_view, NULL);
    vkFreeMemory(device, color_image_memory, NULL);

    vkDestroyBuffer(device, image_buffer, NULL);
    vkFreeMemory(device, image_buffer_memory, NULL);

    vkDestroyBuffer(device, lines_buffer, NULL);
    vkFreeMemory(device, lines_memory, NULL);

    vkDestroyPipeline(device, pipeline, NULL);
    vkDestroyPipelineLayout(device, pipeline_layout, NULL);
    vkDestroyShaderModule(device, vertex_shader, NULL);
    vkDestroyShaderModule(device, fragment_shader, NULL);

    vkDestroyDescriptorSetLayout(device, descriptor_set_layout, NULL);
    vkDestroyDescriptorPool(device, descriptor_pool, NULL);

    vkDestroyRenderPass(device, render_pass, NULL);
    vkFreeCommandBuffers(device, cmd_pool, 1, &setup_buffer);
    vkFreeCommandBuffers(device, cmd_pool, 1, &draw_buffer);
    vkDestroyCommandPool(device, cmd_pool, NULL);
    vkDestroyDevice(device, NULL);
    {
        PFN_vkDestroyDebugReportCallbackEXT destroyDebugReportCallback =
            (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
        assert(destroyDebugReportCallback);
        destroyDebugReportCallback(instance, debug_callback, NULL);
    }
    vkDestroyInstance(instance, NULL);
    return 0;
}
