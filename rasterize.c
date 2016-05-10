#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <vulkan/vulkan.h>
#include <xcb/xcb.h>

#define NELEMS(x) (sizeof(x) / sizeof(*x))

struct timespec frame_time = {.tv_sec = 0, .tv_nsec = 1000 * 1000 * 20,};

VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
                                                   uint64_t object, size_t location, int32_t messageCode,
                                                   const char* pLayerPrefix, const char* pMessage, void* pUserData){
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

void createSwapchain(VkDevice device, VkSurfaceKHR surface, VkExtent2D size,
                     uint32_t min_images, VkSwapchainKHR * swapchain,
                     uint32_t * nimages, VkImage ** images, VkImageView** views) {
    {
        VkSwapchainKHR old_swapchain = *swapchain;
        VkSwapchainCreateInfoKHR create_info = {
            .sType =  VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .surface = surface,
            .minImageCount = min_images,
            .imageFormat = VK_FORMAT_B8G8R8A8_SRGB, //vkGetPhysicalDeviceSurfaceFormatsKHR
            .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
            .imageExtent = size,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = NULL,
            .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = VK_PRESENT_MODE_MAILBOX_KHR,
            .clipped = VK_TRUE,
            .oldSwapchain = old_swapchain,
        };
        assert(vkCreateSwapchainKHR(device, &create_info, NULL, swapchain) == VK_SUCCESS);
        if (old_swapchain != VK_NULL_HANDLE)
            vkDestroySwapchainKHR(device, old_swapchain, NULL);
    }

    vkGetSwapchainImagesKHR(device, *swapchain, nimages, NULL);
    *images = malloc(*nimages * sizeof(**images));
    vkGetSwapchainImagesKHR(device, *swapchain, nimages, *images);
    *views = malloc(*nimages * sizeof(**views));

    {
        VkImageViewCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .image = NULL,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_B8G8R8A8_SRGB,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        for (size_t i = 0; i < *nimages; i++){
            create_info.image = (*images)[i];
            assert(vkCreateImageView(device, &create_info, NULL, &(*views)[i]) == VK_SUCCESS);
        }
    }
}
void cmdPrepareSwapchainImages(VkCommandBuffer cmd_buffer, uint32_t nswapchain_images,
                               VkImage * swapchain_images) {
    for (size_t i = 0; i < nswapchain_images; i++){
        VkImageMemoryBarrier color_layout_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image = swapchain_images[i],
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        vkCmdPipelineBarrier(cmd_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &color_layout_barrier);
    }
}

void createFramebuffers(VkDevice device, VkExtent2D size,
                        uint32_t naux_views, VkImageView* aux_views,
                        uint32_t nswapchain_images, VkImageView* swapchain_views,
                        VkRenderPass render_pass, VkFramebuffer** framebuffers){
    *framebuffers = malloc(nswapchain_images * sizeof(**framebuffers));

    VkImageView * attachments = malloc((1 + naux_views) * sizeof(*attachments));
    memcpy(&attachments[1], aux_views, sizeof(*aux_views) * naux_views);
    VkFramebufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .renderPass = render_pass,
        .attachmentCount = 1 + naux_views,
        .pAttachments = attachments,
        .width = size.width,
        .height = size.height,
        .layers = 1,
    };
    for (size_t i = 0; i < nswapchain_images; i++){
        attachments[0] = swapchain_views[i];
        assert(vkCreateFramebuffer(device, &create_info, NULL, &(*framebuffers)[i]) == VK_SUCCESS);
    }
}

void cmdDraw(VkCommandBuffer draw_buffer, VkExtent2D size,
             VkPipeline pipeline, VkPipelineLayout pipeline_layout,
             VkRenderPass render_pass, VkFramebuffer framebuffer){
    VkClearValue clear_values[1] = {{
            .color.float32 = {0.1, 0.1, 0.1, 1.0},
        }};
    VkImageSubresourceRange clear_ranges[1] = {{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
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
    VkSubpassContents subpass_contents[1] = {VK_SUBPASS_CONTENTS_INLINE,};
    vkCmdBeginRenderPass(draw_buffer, &renderpass_begin_info, subpass_contents[0]);
    vkCmdBindPipeline(draw_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

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

VkSurfaceCapabilitiesKHR getSurfaceCapabilities(VkPhysicalDevice phy_device, VkSurfaceKHR surface){
    VkSurfaceCapabilitiesKHR surface_caps;
    assert(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phy_device, surface, &surface_caps) == VK_SUCCESS);
    return surface_caps;
}

int main(void) {
    xcb_connection_t* xcb_conn = xcb_connect(NULL, NULL);
    assert(xcb_connection_has_error(xcb_conn) == 0);
    xcb_screen_t* screen = xcb_setup_roots_iterator(xcb_get_setup(xcb_conn)).data;
    xcb_window_t window = xcb_generate_id(xcb_conn);
    {
        uint32_t value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
        uint32_t values[32] = {
            [0] = screen->black_pixel,
            [1] = XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY,
        };
        xcb_create_window(xcb_conn, XCB_COPY_FROM_PARENT, window, screen->root, 0, 0, 240, 240, 1, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, value_mask, values);
        xcb_map_window(xcb_conn, window);
        xcb_flush(xcb_conn);
    }

    VkInstance instance;
    {
        const char surface_ext[] = "VK_KHR_surface";
        const char xcb_ext[] = "VK_KHR_xcb_surface";
        const char debug_ext[] = "VK_EXT_debug_report";
        const char* extensions[] = {surface_ext, xcb_ext, debug_ext};

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

    VkSurfaceKHR surface;
    {
        VkXcbSurfaceCreateInfoKHR create_info = {
            .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .connection = xcb_conn,
            .window = window,
        };
        assert(vkCreateXcbSurfaceKHR(instance, &create_info, NULL, &surface) == VK_SUCCESS);
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
        const char swapchain_ext[] = "VK_KHR_swapchain";
        const char* extensions[] = {swapchain_ext,};

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
            .enabledExtensionCount = NELEMS(extensions),
            .ppEnabledExtensionNames = extensions,
            .pEnabledFeatures = NULL,
        };

        assert(vkCreateDevice(phy_device, &create_info, NULL, &device) == VK_SUCCESS);
    }

    VkSurfaceCapabilitiesKHR surface_caps = getSurfaceCapabilities(phy_device, surface);

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
                .format = VK_FORMAT_B8G8R8A8_SRGB,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
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

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    uint32_t nswapchain_images;
    VkImage* swapchain_images;
    VkImageView* swapchain_views;
    // vkAcquireNextImageKHR guaranteed to be non blocking with min+1 images and mailbox mode
    createSwapchain(device, surface, surface_caps.currentExtent, surface_caps.minImageCount + 1,
                    &swapchain, &nswapchain_images, &swapchain_images, &swapchain_views);
    VkFramebuffer* framebuffers;
    createFramebuffers(device, surface_caps.currentExtent, 0, NULL, nswapchain_images, swapchain_views, render_pass, &framebuffers);

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

    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    {
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
    }

    VkCommandBuffer setup_buffer;
    {
        VkCommandBufferAllocateInfo allocate_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = NULL,
            .commandPool = cmd_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        assert(vkAllocateCommandBuffers(device, &allocate_info, &setup_buffer) == VK_SUCCESS);
    }

    {
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = 0,
            .pInheritanceInfo = NULL,
        };
        assert(vkBeginCommandBuffer(setup_buffer, &begin_info) == VK_SUCCESS);
        cmdPrepareSwapchainImages(setup_buffer, nswapchain_images, swapchain_images);
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

    VkCommandBuffer* draw_buffers = malloc(nswapchain_images * sizeof(*draw_buffers));
    {
        VkCommandBufferAllocateInfo allocate_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = NULL,
            .commandPool = cmd_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = nswapchain_images,
        };
        assert(vkAllocateCommandBuffers(device, &allocate_info, draw_buffers) == VK_SUCCESS);
    }

    {
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = NULL,
            .flags = 0,
            .pInheritanceInfo = NULL,
        };
        for (size_t i = 0; i < nswapchain_images; i++){
            assert(vkBeginCommandBuffer(draw_buffers[i], &begin_info) == VK_SUCCESS);
            cmdDraw(draw_buffers[i], surface_caps.currentExtent, pipeline, pipeline_layout,
                    render_pass, framebuffers[i]);
            assert(vkEndCommandBuffer(draw_buffers[i]) == VK_SUCCESS);
        }
    }

    VkSemaphore image_acquire_sem;
    VkSemaphore render_sem;
    {
        VkSemaphoreCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
        };
        assert(vkCreateSemaphore(device, &create_info, NULL, &image_acquire_sem) == VK_SUCCESS);
        assert(vkCreateSemaphore(device, &create_info, NULL, &render_sem) == VK_SUCCESS);
    }

    // Wait for set-up to finish
    assert(vkQueueWaitIdle(queue) == VK_SUCCESS);
    {
        while (true) {
            uint32_t next_image_idx;
            VkResult acquire_err;
            acquire_err = vkAcquireNextImageKHR(device, swapchain, 0, image_acquire_sem, VK_NULL_HANDLE, &next_image_idx);
            if (acquire_err == VK_ERROR_OUT_OF_DATE_KHR
                || acquire_err == VK_SUBOPTIMAL_KHR){
                // Window closed
                if (xcb_connection_has_error(xcb_conn))
                    break;

                for (size_t i = 0; i < nswapchain_images; i++){
                    vkDestroyFramebuffer(device, framebuffers[i], NULL);
                    vkDestroyImageView(device, swapchain_views[i], NULL);
                }

                surface_caps = getSurfaceCapabilities(phy_device, surface);

                createSwapchain(device, surface, surface_caps.currentExtent, surface_caps.minImageCount + 1,
                                &swapchain, &nswapchain_images, &swapchain_images, &swapchain_views);
                createFramebuffers(device, surface_caps.currentExtent, 0, NULL, nswapchain_images, swapchain_views, render_pass, &framebuffers);

                VkCommandBufferBeginInfo begin_info = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    .pNext = NULL,
                    .flags = 0,
                    .pInheritanceInfo = NULL,
                };
                for (size_t i = 0; i < nswapchain_images; i++){
                    assert(vkResetCommandBuffer(draw_buffers[i], 0) == VK_SUCCESS);
                    assert(vkBeginCommandBuffer(draw_buffers[i], &begin_info) == VK_SUCCESS);
                    cmdDraw(draw_buffers[i], surface_caps.currentExtent, pipeline, pipeline_layout,
                            render_pass, framebuffers[i]);
                    assert(vkEndCommandBuffer(draw_buffers[i]) == VK_SUCCESS);
                }

                vkDestroySemaphore(device, image_acquire_sem, NULL);
                VkSemaphoreCreateInfo create_info = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                    .pNext = NULL,
                    .flags = 0,
                };
                assert(vkCreateSemaphore(device, &create_info, NULL, &image_acquire_sem) == VK_SUCCESS);

                assert(vkResetCommandBuffer(setup_buffer, 0) == VK_SUCCESS);
                assert(vkBeginCommandBuffer(setup_buffer, &begin_info) == VK_SUCCESS);
                cmdPrepareSwapchainImages(setup_buffer, nswapchain_images, swapchain_images);
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
                assert(vkQueueWaitIdle(queue) == VK_SUCCESS);
                continue;
            } else {
                assert(acquire_err == VK_SUCCESS);
            }

            VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            VkSubmitInfo submit_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .pNext = NULL,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &image_acquire_sem,
                .pWaitDstStageMask = &wait_stage,
                .commandBufferCount = 1,
                .pCommandBuffers = &draw_buffers[next_image_idx],
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = &render_sem,
            };
            assert(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE) == VK_SUCCESS);

            VkPresentInfoKHR present_info = {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .pNext = NULL,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &render_sem,
                .swapchainCount = 1,
                .pSwapchains = &swapchain,
                .pImageIndices = &next_image_idx,
                .pResults = NULL,
            };
            assert(vkQueuePresentKHR(queue, &present_info) == VK_SUCCESS);
            assert(vkQueueWaitIdle(queue) == VK_SUCCESS);
            nanosleep(&frame_time, NULL);
        }
    }

    for (size_t i = 0; i < nswapchain_images; i++){
        vkDestroyFramebuffer(device, framebuffers[i], NULL);
        vkDestroyImageView(device, swapchain_views[i], NULL);
    }

    vkDestroyPipeline(device, pipeline, NULL);
    vkDestroyPipelineLayout(device, pipeline_layout, NULL);
    vkDestroyShaderModule(device, vertex_shader, NULL);
    vkDestroyShaderModule(device, fragment_shader, NULL);

    vkDestroyRenderPass(device, render_pass, NULL);
    vkDestroySwapchainKHR(device, swapchain, NULL);
    free(swapchain_images);
    free(swapchain_views);
    free(framebuffers);
    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroySemaphore(device, image_acquire_sem, NULL);
    vkDestroySemaphore(device, render_sem, NULL);
    vkFreeCommandBuffers(device, cmd_pool, 1, &setup_buffer);
    vkFreeCommandBuffers(device, cmd_pool, nswapchain_images, draw_buffers);
    free(draw_buffers);
    vkDestroyCommandPool(device, cmd_pool, NULL);
    vkDestroyDevice(device, NULL);
    {
        PFN_vkDestroyDebugReportCallbackEXT destroyDebugReportCallback =
            (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
        assert(destroyDebugReportCallback);
        destroyDebugReportCallback(instance, debug_callback, NULL);
    }
    vkDestroyInstance(instance, NULL);
    xcb_unmap_window(xcb_conn, window);
    xcb_destroy_window(xcb_conn, window);
    xcb_disconnect(xcb_conn);
    return 0;
}
