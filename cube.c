#include "util.h"
#include "vkutil.h"
#include "info.h"
#include "tiff.h"

#include <stdlib.h>
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
    float pos[3] __attribute__((aligned(16)));
};

const struct Vertex verts[8] = {
    {.pos={-1.0,-1.0,-1.0}},
    {.pos={ 1.0,-1.0,-1.0}},
    {.pos={-1.0, 1.0,-1.0}},
    {.pos={ 1.0, 1.0,-1.0}},
    {.pos={-1.0,-1.0, 1.0}},
    {.pos={ 1.0,-1.0, 1.0}},
    {.pos={-1.0, 1.0, 1.0}},
    {.pos={ 1.0, 1.0, 1.0}},
};

const uint32_t indices[20] = {
    0, 1, 2, 3, 6, 7, 4, 5, 0, 1, 0xFFFFFFFF,
    1, 5, 3, 7, 0xFFFFFFFF,
    4, 0, 6, 2,
};

void cmdDraw(VkCommandBuffer draw_buffer, VkExtent2D size,
             size_t npipelines, VkPipeline * pipelines,
             VkBuffer vertex_buffer, VkBuffer index_buffer,
             VkRenderPass render_pass, VkFramebuffer framebuffer){
    VkClearValue clear_values[] = {{
            .color.float32 = {0.0, 0.0, 0.0, 1.0},
        }, {
            .depthStencil = {.depth = 1.0},
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
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(draw_buffer, 0, 1, &vertex_buffer, &offset);
    vkCmdBindIndexBuffer(draw_buffer, index_buffer, 0, VK_INDEX_TYPE_UINT32);
    for (size_t i = 0; i < npipelines; i++) {
        vkCmdBindPipeline(draw_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[i]);
        vkCmdDrawIndexed(draw_buffer, 20, 27, 0, 0, 0);
    }
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

        uint32_t nqueues;
        matchingQueues(phy_device, VK_QUEUE_GRAPHICS_BIT, &nqueues, NULL);
        assert(nqueues > 0);
        uint32_t * queue_family_idxs = malloc(sizeof(*queue_family_idxs) * nqueues);
        matchingQueues(phy_device, VK_QUEUE_GRAPHICS_BIT, &nqueues, queue_family_idxs);

        VkDeviceQueueCreateInfo queue_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .queueFamilyIndex = queue_family_idxs[0],
            .queueCount = 1,
            .pQueuePriorities = queue_priorities,
        };
        free(queue_family_idxs);

        VkPhysicalDeviceFeatures features = {
            .geometryShader = VK_TRUE,
        };

        VkDeviceCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queue_info,
            .enabledLayerCount = NELEMS(layers),
            .ppEnabledLayerNames = layers,
            .enabledExtensionCount = 0,
            .ppEnabledExtensionNames = NULL,
            .pEnabledFeatures = &features,
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
                .samples = VK_SAMPLE_COUNT_8_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            }, {
                .flags = 0,
                .format = VK_FORMAT_D16_UNORM,
                .samples = VK_SAMPLE_COUNT_8_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            }, {
                .flags = 0,
                .format = VK_FORMAT_D16_UNORM,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            }};
        VkAttachmentReference attachment_refs[NELEMS(attachments)] = {{
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            }, {
                .attachment = 1,
                .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            }, {
                .attachment = 2,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            }};
        VkSubpassDescription subpasses[1] = {{
                .flags = 0,
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .inputAttachmentCount = 0,
                .pInputAttachments = NULL,
                .colorAttachmentCount = 1,
                .pColorAttachments = &attachment_refs[0],
                .pResolveAttachments = &attachment_refs[2],
                .pDepthStencilAttachment = &attachment_refs[1],
                .preserveAttachmentCount = 0,
                .pPreserveAttachments = NULL,
            }};
        VkSubpassDependency dependencies[] = {{
                .srcSubpass = 0,
                .dstSubpass = VK_SUBPASS_EXTERNAL,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .dependencyFlags = 0,
            }};
        VkRenderPassCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .attachmentCount = NELEMS(attachments),
            .pAttachments = attachments,
            .subpassCount = NELEMS(subpasses),
            .pSubpasses = subpasses,
            .dependencyCount = NELEMS(dependencies),
            .pDependencies = dependencies,
        };
        assert(vkCreateRenderPass(device, &create_info, NULL, &render_pass) == VK_SUCCESS);
    }

    VkImage images[3];
    VkDeviceMemory image_memories[NELEMS(images)];
    VkImageView views[NELEMS(images)];
    createFrameImage(device, render_size, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_8_BIT,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                     &images[0], &image_memories[0], &views[0]);
    createFrameImage(device, render_size, VK_FORMAT_D16_UNORM, VK_SAMPLE_COUNT_8_BIT,
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT,
                     &images[1], &image_memories[1], &views[1]);
    createFrameImage(device, render_size, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT,
                     &images[2], &image_memories[2], &views[2]);

    VkBuffer verts_buffer;
    VkDeviceMemory verts_memory;
    createBuffer(device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(verts), verts, &verts_buffer, &verts_memory);

    VkBuffer index_buffer;
    VkDeviceMemory index_memory;
    createBuffer(device, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, sizeof(indices), indices, &index_buffer, &index_memory);

    VkBuffer image_buffer;
    VkDeviceMemory image_buffer_memory;
    createBuffer(device, VK_BUFFER_USAGE_TRANSFER_DST_BIT, render_size.height * render_size.width * 4, NULL, &image_buffer, &image_buffer_memory);

    VkFramebuffer framebuffer;
    createFramebuffer(device, render_size, 3, views, render_pass, &framebuffer);

    VkShaderModule shaders[5];
    {
        char* filenames[NELEMS(shaders)] = {"cube.vert.spv", "cube.geom.spv", "cube.frag.spv", "wireframe.geom.spv", "device_coords.frag.spv"};
        for (size_t i = 0; i < NELEMS(shaders); i++){
            size_t code_size;
            uint32_t * code;
            assert((code_size = loadModule(filenames[i], &code)) != 0);

            VkShaderModuleCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .codeSize = code_size,
                .pCode = code,
            };
            assert(vkCreateShaderModule(device, &create_info, NULL, &shaders[i]) == VK_SUCCESS);
            free(code);
        }
    }

    VkPipelineLayout pipeline_layout;
    {
        VkPipelineLayoutCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .setLayoutCount = 0,
            .pSetLayouts = NULL,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = NULL,
        };
        assert(vkCreatePipelineLayout(device, &create_info, NULL, &pipeline_layout) == VK_SUCCESS);
    }

    VkPipeline pipelines[2];
    {
        VkPipelineShaderStageCreateInfo stages[3] = {{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = shaders[0],
                .pName = "main",
                .pSpecializationInfo = NULL,
            },{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .stage = VK_SHADER_STAGE_GEOMETRY_BIT,
                .module = shaders[1],
                .pName = "main",
                .pSpecializationInfo = NULL,
            },{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = NULL,
                .flags = 0,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = shaders[2],
                .pName = "main",
                .pSpecializationInfo = NULL,
            }};
        VkVertexInputBindingDescription vtx_binding = {
            .binding = 0,
            .stride = sizeof(struct Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };
        VkVertexInputAttributeDescription vtx_attr = {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(struct Vertex, pos),
        };
        VkPipelineVertexInputStateCreateInfo vtx_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &vtx_binding,
            .vertexAttributeDescriptionCount = 1,
            .pVertexAttributeDescriptions = &vtx_attr,
        };
        VkPipelineInputAssemblyStateCreateInfo ia_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
            .primitiveRestartEnable = VK_TRUE,
        };
        VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = render_size.width,
            .height = render_size.height,
            .minDepth = 0.0,
            .maxDepth = 1.0,
        };
        VkRect2D scissor= {
            .offset = {.x = 0, .y = 0,},
            .extent = render_size,
        };
        VkPipelineViewportStateCreateInfo viewport_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .viewportCount = 1,
            .pViewports = &viewport,
            .scissorCount = 1,
            .pScissors = &scissor,
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
            .rasterizationSamples = VK_SAMPLE_COUNT_8_BIT,
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
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
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
            .pDynamicState = NULL,
            .layout = pipeline_layout,
            .renderPass = render_pass,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = 0,
        };
        assert(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &create_info, NULL, &pipelines[0]) == VK_SUCCESS);
        stages[1].module = shaders[3];
        stages[2].module = shaders[4];
        rasterization_state.polygonMode = VK_POLYGON_MODE_LINE;
        assert(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &create_info, NULL, &pipelines[1]) == VK_SUCCESS);
    }

    VkCommandBuffer draw_buffer;
    {
        VkCommandBufferAllocateInfo allocate_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = NULL,
            .commandPool = cmd_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        assert(vkAllocateCommandBuffers(device, &allocate_info, &draw_buffer) == VK_SUCCESS);
    }

    {
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = 0,
            .pInheritanceInfo = NULL,
        };
        assert(vkBeginCommandBuffer(draw_buffer, &begin_info) == VK_SUCCESS);
        cmdDraw(draw_buffer, render_size, 2, pipelines, verts_buffer, index_buffer, render_pass, framebuffer);

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
        vkCmdCopyImageToBuffer(draw_buffer, images[2], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image_buffer, 1, &copy);

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

    {
        char * image_data;
        assert(vkMapMemory(device, image_buffer_memory, 0, VK_WHOLE_SIZE, 0, (void **) &image_data) == VK_SUCCESS);
        VkMappedMemoryRange image_flush = {
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, .pNext = NULL,
            .memory = image_buffer_memory,
            .offset = 0,
            .size = VK_WHOLE_SIZE,
        };

        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = NULL,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = NULL,
            .pWaitDstStageMask = NULL,
            .commandBufferCount = 1,
            .pCommandBuffers = NULL,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = NULL,
        };

        submit_info.pCommandBuffers = &draw_buffer;
        assert(vkResetFences(device, 1, &fence) == VK_SUCCESS);
        assert(vkQueueSubmit(queue, 1, &submit_info, fence) == VK_SUCCESS);
        assert(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS);
        assert(vkInvalidateMappedMemoryRanges(device, 1, &image_flush) == VK_SUCCESS);
        assert(writeTiff("cube.tif", image_data, render_size, nchannels) == 0);

        vkUnmapMemory(device, image_buffer_memory);
    }

    assert(vkQueueWaitIdle(queue) == VK_SUCCESS);
    vkDestroyFence(device, fence, NULL);

    vkDestroyFramebuffer(device, framebuffer, NULL);
    for (size_t i = 0; i < NELEMS(images); i++){
        vkDestroyImage(device, images[i], NULL);
        vkDestroyImageView(device, views[i], NULL);
        vkFreeMemory(device, image_memories[i], NULL);
    }

    vkDestroyBuffer(device, image_buffer, NULL);
    vkFreeMemory(device, image_buffer_memory, NULL);

    vkDestroyBuffer(device, verts_buffer, NULL);
    vkFreeMemory(device, verts_memory, NULL);

    vkDestroyBuffer(device, index_buffer, NULL);
    vkFreeMemory(device, index_memory, NULL);

    for (size_t i = 0; i < NELEMS(pipelines); i++){
        vkDestroyPipeline(device, pipelines[i], NULL);
    }
    vkDestroyPipelineLayout(device, pipeline_layout, NULL);
    for(size_t i = 0; i < NELEMS(shaders); i++)
        vkDestroyShaderModule(device, shaders[i], NULL);

    vkDestroyRenderPass(device, render_pass, NULL);
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