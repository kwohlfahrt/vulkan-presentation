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
    float pos[2] __attribute__((aligned(16)));
};

const struct Vertex verts[3] = {
    {.pos={ 0.0,-0.5}},
    {.pos={ 0.5, 0.5}},
    {.pos={-0.5, 0.5}},
};

void cmdDraw(VkCommandBuffer draw_buffer, VkExtent2D size,
             VkPipeline pipeline, VkBuffer vertex_buffer,
             VkRenderPass render_pass, VkFramebuffer framebuffer){
    VkClearValue clear_values[] = {{
            .depthStencil = {.depth = 1.0},
        }, {
            .color.float32 = {0.0, 0.0, 0.0, 1.0},
        }, {
            // Not cleared
        }, {
            .color.float32 = {1.0, 1.0, 1.0, 1.0},
        }, {
            // Not cleared
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
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(draw_buffer, 0, 1, &vertex_buffer, &offset);
    vkCmdDraw(draw_buffer, 3, 4, 0, 0);
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
        assert(vkEnumeratePhysicalDevices(instance, &num_devices, NULL) == VK_SUCCESS);
        assert(num_devices >= 1);
        VkPhysicalDevice * phy_devices = malloc(sizeof(*phy_devices) * num_devices);
        assert(vkEnumeratePhysicalDevices(instance, &num_devices, phy_devices) == VK_SUCCESS);
        phy_device = phy_devices[0];
        free(phy_devices);
    }

    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(phy_device, &memory_properties);

    VkDevice device;
    {
        float queue_priorities[] = {1.0};
        const char validation_layer[] = "VK_LAYER_LUNARG_standard_validation";
        const char* layers[] = {validation_layer,};
        VkPhysicalDeviceFeatures enabled_features = {
            .independentBlend = VK_TRUE,
        };

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
            .pEnabledFeatures = &enabled_features,
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
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            }, {
                .flags = 0,
                .format = VK_FORMAT_R8_UNORM,
                .samples = VK_SAMPLE_COUNT_8_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            }, {
                .flags = 0,
                .format = VK_FORMAT_R8_UNORM,
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
                .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            }, {
                .attachment = 1,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            }, {
                .attachment = 2,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            }, {
                .attachment = 3,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            }, {
                .attachment = 4,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            }};
        VkAttachmentReference color_attachments[2] = {
            attachment_refs[1], attachment_refs[3]
        };
        VkAttachmentReference resolve_attachments[2] = {
            attachment_refs[2], attachment_refs[4]
        };
        VkSubpassDescription subpasses[1] = {{
                .flags = 0,
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .inputAttachmentCount = 0,
                .pInputAttachments = NULL,
                .colorAttachmentCount = 2,
                .pColorAttachments = color_attachments,
                .pResolveAttachments = resolve_attachments,
                .pDepthStencilAttachment = &attachment_refs[0],
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
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
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

    VkImage images[5];
    VkDeviceMemory image_memories[NELEMS(images)];
    VkImageView views[NELEMS(images)];
    createFrameImage(memory_properties, device, render_size,
                     VK_FORMAT_D16_UNORM, VK_SAMPLE_COUNT_8_BIT,
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT,
                     &images[0], &image_memories[0], &views[0]);
    createFrameImage(memory_properties, device, render_size,
                     VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_8_BIT,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                     &images[1], &image_memories[1], &views[1]);
    createFrameImage(memory_properties, device, render_size,
                     VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT,
                     &images[2], &image_memories[2], &views[2]);
    createFrameImage(memory_properties, device, render_size,
                     VK_FORMAT_R8_UNORM, VK_SAMPLE_COUNT_8_BIT,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                     &images[3], &image_memories[3], &views[3]);
    createFrameImage(memory_properties, device, render_size,
                     VK_FORMAT_R8_UNORM, VK_SAMPLE_COUNT_1_BIT,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT,
                     &images[4], &image_memories[4], &views[4]);

    VkBuffer verts_buffer;
    VkDeviceMemory verts_memory;
    createBuffer(memory_properties, device, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(verts), verts, &verts_buffer, &verts_memory);

    VkBuffer image_buffers[2];
    VkImage output_images[NELEMS(image_buffers)] = {images[2], images[4]};
    char output_channels[NELEMS(image_buffers)] = {4, 1};
    VkDeviceMemory image_buffer_memories[NELEMS(image_buffers)];
    for (size_t i = 0; i < NELEMS(image_buffers); i++)
        createBuffer(memory_properties, device, VK_BUFFER_USAGE_TRANSFER_DST_BIT, render_size.height * render_size.width * output_channels[i], NULL, &image_buffers[i], &image_buffer_memories[i]);

    VkFramebuffer framebuffer;
    {
        createFramebuffer(device, render_size, 5, views, render_pass, &framebuffer);
    }

    VkShaderModule vertex_shader, fragment_shader;
    {
        char* filenames[] = {"blend.vert.spv", "blend.frag.spv"};
        VkShaderModule * modules[NELEMS(filenames)] = {&vertex_shader, &fragment_shader};
        for (size_t i = 0; i < NELEMS(filenames); i++){
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
            assert(vkCreateShaderModule(device, &create_info, NULL, modules[i]) == VK_SUCCESS);
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

    VkPipeline pipelines[3];
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
        VkVertexInputBindingDescription vtx_binding = {
            .binding = 0,
            .stride = sizeof(struct Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };
        VkVertexInputAttributeDescription vtx_attr = {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
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
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
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
            .depthTestEnable = VK_FALSE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
            .front = {},
            .back = {},
            .minDepthBounds = 0.0,
            .maxDepthBounds = 1.0,
        };
        VkPipelineColorBlendAttachmentState color_blend_attachments[2] = {{
                .blendEnable = VK_FALSE,
                .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
                .colorBlendOp = VK_BLEND_OP_ADD,
                // Not sure alpha blending works right
                .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                .alphaBlendOp = VK_BLEND_OP_MAX,
                .colorWriteMask = ( VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                    | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT),
            }, {
                .blendEnable = VK_FALSE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT,
            }};
        VkPipelineColorBlendStateCreateInfo color_blend_state = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .logicOpEnable = VK_FALSE, //.logicOp = 0,
            .attachmentCount = NELEMS(color_blend_attachments),
            .pAttachments = color_blend_attachments,
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
        depth_stencil_state.depthTestEnable = VK_TRUE;
        assert(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &create_info, NULL, &pipelines[1]) == VK_SUCCESS);
        depth_stencil_state.depthTestEnable = VK_FALSE;
        color_blend_attachments[0].blendEnable = VK_TRUE;
        assert(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &create_info, NULL, &pipelines[2]) == VK_SUCCESS);
    }

    VkCommandBuffer draw_buffers[3];
    {
        VkCommandBufferAllocateInfo allocate_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = NULL,
            .commandPool = cmd_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        allocate_info.commandBufferCount = NELEMS(draw_buffers);
        assert(vkAllocateCommandBuffers(device, &allocate_info, draw_buffers) == VK_SUCCESS);
    }

    for (size_t i = 0; i < NELEMS(draw_buffers); i++) {
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = 0,
            .pInheritanceInfo = NULL,
        };
        assert(vkBeginCommandBuffer(draw_buffers[i], &begin_info) == VK_SUCCESS);
        cmdDraw(draw_buffers[i], render_size, pipelines[i], verts_buffer, render_pass, framebuffer);

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
        for (size_t j = 0; j < NELEMS(image_buffers); j++) {
            vkCmdCopyImageToBuffer(draw_buffers[i], output_images[j], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image_buffers[j], 1, &copy);
        }

        VkBufferMemoryBarrier transfer_barriers[NELEMS(image_buffers)];
        for (size_t j = 0; j < NELEMS(transfer_barriers); j++) {
            transfer_barriers[j] = (VkBufferMemoryBarrier) {
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .pNext = 0,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .buffer = image_buffers[j],
                .offset = 0,
                .size = VK_WHOLE_SIZE,
            };
        }
        vkCmdPipelineBarrier(draw_buffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, NELEMS(transfer_barriers), transfer_barriers, 0, NULL);
        assert(vkEndCommandBuffer(draw_buffers[i]) == VK_SUCCESS);
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
        char * filenames[NELEMS(image_buffers)][NELEMS(draw_buffers)] = {
            {"blend_no-z-test.tif", "blend_z-test.tif", "blend_add.tif"},
            {"blend_no-z-test_z.tif", "blend_z-test_z.tif", NULL},

        };
        char * image_data[NELEMS(image_buffers)];
        VkMappedMemoryRange image_flushes[NELEMS(image_buffers)];
        for (size_t i = 0; i < NELEMS(image_buffers); i++) {
            assert(vkMapMemory(device, image_buffer_memories[i], 0, VK_WHOLE_SIZE, 0, (void **) &image_data[i]) == VK_SUCCESS);

            image_flushes[i] = (VkMappedMemoryRange) {
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .pNext = NULL,
                .memory = image_buffer_memories[i],
                .offset = 0,
                .size = VK_WHOLE_SIZE,
            };
        }

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

        for (size_t i = 0; i < NELEMS(draw_buffers); i++) {
            submit_info.pCommandBuffers = &draw_buffers[i];
            assert(vkResetFences(device, 1, &fence) == VK_SUCCESS);
            assert(vkQueueSubmit(queue, 1, &submit_info, fence) == VK_SUCCESS);
            assert(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS);
            assert(vkInvalidateMappedMemoryRanges(device, NELEMS(image_flushes), image_flushes) == VK_SUCCESS);
            for (size_t j = 0; j < NELEMS(image_data); j++)
                if (filenames[j][i] != NULL)
                    assert(writeTiff(filenames[j][i], image_data[j], render_size, output_channels[j]) == 0);
        }

        for (size_t i = 0; i < NELEMS(image_data); i++)
            vkUnmapMemory(device, image_buffer_memories[i]);
    }

    assert(vkQueueWaitIdle(queue) == VK_SUCCESS);
    vkDestroyFence(device, fence, NULL);

    vkDestroyFramebuffer(device, framebuffer, NULL);
    for (size_t i = 0; i < NELEMS(images); i++) {
        vkDestroyImage(device, images[i], NULL);
        vkDestroyImageView(device, views[i], NULL);
        vkFreeMemory(device, image_memories[i], NULL);
    }

    for (size_t i = 0; i < NELEMS(image_buffers); i++) {
        vkDestroyBuffer(device, image_buffers[i], NULL);
        vkFreeMemory(device, image_buffer_memories[i], NULL);
    }

    vkDestroyBuffer(device, verts_buffer, NULL);
    vkFreeMemory(device, verts_memory, NULL);

    for (size_t i = 0; i < NELEMS(pipelines); i++)
        vkDestroyPipeline(device, pipelines[i], NULL);
    vkDestroyPipelineLayout(device, pipeline_layout, NULL);
    vkDestroyShaderModule(device, vertex_shader, NULL);
    vkDestroyShaderModule(device, fragment_shader, NULL);

    vkDestroyRenderPass(device, render_pass, NULL);
    vkFreeCommandBuffers(device, cmd_pool, NELEMS(draw_buffers), draw_buffers);
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
