#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "info.h"

void printPhysicalDeviceInfo(VkPhysicalDevice phy_device, VkSurfaceKHR surface) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(phy_device, &properties);
    printf("- %s: \n", properties.deviceName);

    {
        uint32_t nlayers;
        vkEnumerateDeviceLayerProperties(phy_device, &nlayers, NULL);
        VkLayerProperties * layers = malloc(nlayers * sizeof(*layers));
        vkEnumerateDeviceLayerProperties(phy_device, &nlayers, layers);
        fputs("  - Available layers:", stdout);
        for (size_t j = 0; j < nlayers; j++) {
            printf("%s ", layers[j].layerName);
        }
        puts("");
        free(layers);
    }

    {
        uint32_t nextensions;
        vkEnumerateDeviceExtensionProperties(phy_device, NULL, &nextensions, NULL);
        VkExtensionProperties * extensions = malloc(nextensions * sizeof(*extensions));
        vkEnumerateDeviceExtensionProperties(phy_device, NULL, &nextensions, extensions);
        fputs("  - Available extensions:", stdout);
        for (size_t j = 0; j < nextensions; j++) {
            printf("%s ", extensions[j].extensionName);
        }
        puts("");
        free(extensions);
    }

    {
        uint32_t num_queues;
        vkGetPhysicalDeviceQueueFamilyProperties(phy_device, &num_queues, NULL);
        assert(num_queues > 0);
        VkQueueFamilyProperties queue_family_properties;
        vkGetPhysicalDeviceQueueFamilyProperties(phy_device, &num_queues, &queue_family_properties);
        printf("  - %u x", queue_family_properties.queueCount);
        if (queue_family_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            fputs(" graphics", stdout);
        if (queue_family_properties.queueFlags & VK_QUEUE_COMPUTE_BIT)
            fputs(" compute", stdout);
        if (queue_family_properties.queueFlags & VK_QUEUE_TRANSFER_BIT)
            fputs(" transfer", stdout);
        if (queue_family_properties.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
            fputs(" sparse-binding", stdout);
        puts("");
    }

    {
        VkBool32 present_support;
        assert(vkGetPhysicalDeviceSurfaceSupportKHR(phy_device, 0, surface, &present_support) == VK_SUCCESS);
        if (present_support)
            puts("  - surface supported");
        else
            puts("  - surface NOT supported");
    }

    {
        VkPhysicalDeviceMemoryProperties memory_properties;
        vkGetPhysicalDeviceMemoryProperties(phy_device, &memory_properties);
        for (size_t i = 0; i < memory_properties.memoryHeapCount; i++){
            printf("  - %s heap %zu: %zu bytes\n",
                   (memory_properties.memoryHeaps[i].flags
                    & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? "Device" : "Host",
                   i, memory_properties.memoryHeaps[i].size);
            for (size_t j = 0; j < memory_properties.memoryTypeCount; j++){
                if (memory_properties.memoryTypes[j].heapIndex != i)
                    continue;
                printf("    - Type %zu:", j);
                if (memory_properties.memoryTypes[j].propertyFlags
                    & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                    printf(" device-local");
                if (memory_properties.memoryTypes[j].propertyFlags
                    & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
                    printf(" host-visible");
                if (memory_properties.memoryTypes[j].propertyFlags
                    & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
                    printf(" host-coherent");
                if (memory_properties.memoryTypes[j].propertyFlags
                    & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
                    printf(" host-cached");
                if (memory_properties.memoryTypes[j].propertyFlags
                    & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
                    printf(" lazily-allocated");
                puts("");
            }
        }
    }
}

void printLayers(void) {
    uint32_t nlayers;
    vkEnumerateInstanceLayerProperties(&nlayers, NULL);
    VkLayerProperties * layers = malloc(nlayers * sizeof(*layers));
    vkEnumerateInstanceLayerProperties(&nlayers, layers);
    fputs("- Available layers:", stdout);
    for (size_t i = 0; i < nlayers; i++) {
        printf("%s ", layers[i].layerName);
    }
    puts("");
    free(layers);
}

void printExtensions(void) {
    uint32_t nextensions;
    vkEnumerateInstanceExtensionProperties(NULL, &nextensions, NULL);
    VkExtensionProperties * extensions = malloc(nextensions * sizeof(*extensions));
    vkEnumerateInstanceExtensionProperties(NULL, &nextensions, extensions);
    fputs("- Available extensions:", stdout);
    for (size_t i = 0; i < nextensions; i++) {
        printf("%s ", extensions[i].extensionName);
    }
    puts("");
    free(extensions);
}

void printSurfaceCapabilities(VkPhysicalDevice phy_device, VkSurfaceKHR surface) {
    VkSurfaceCapabilitiesKHR surface_capabilities;
    assert(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phy_device, surface, &surface_capabilities) == VK_SUCCESS);

    puts("- Surface properties:");

    {
        uint32_t npresent_modes;
        vkGetPhysicalDeviceSurfacePresentModesKHR(phy_device, surface, &npresent_modes, NULL);
        VkPresentModeKHR * present_modes = malloc(npresent_modes * sizeof(*present_modes));
        vkGetPhysicalDeviceSurfacePresentModesKHR(phy_device, surface, &npresent_modes, present_modes);
        fputs("  - Present modes:", stdout);
        for (size_t i = 0; i < npresent_modes; i++) {
            if (present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
                fputs(" immediate", stdout);
            if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
                fputs(" mailbox", stdout);
            if (present_modes[i] == VK_PRESENT_MODE_FIFO_KHR)
                fputs(" fifo", stdout);
            if (present_modes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR)
                fputs(" fifo-relaxed", stdout);
        }
        puts("");
        free(present_modes);
    }

    printf("  - Images: %u - %u\n", surface_capabilities.minImageCount, surface_capabilities.maxImageCount);
    printf("  - Size: %u x %u\n", surface_capabilities.currentExtent.width, surface_capabilities.currentExtent.height);
    fputs("  - Supported transforms:", stdout);
    {
        if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
            fputs(" identity", stdout);
        if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR)
            fputs(" rotate-90", stdout);
        if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR)
            fputs(" rotate-180", stdout);
        if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR)
            fputs(" rotate-270", stdout);
        if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR)
            fputs(" horizontal-mirror", stdout);
        if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR)
            fputs(" horizontal-mirror-rotate-90", stdout);
        if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR)
            fputs(" horizontal-mirror-rotate-180", stdout);
        if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR)
            fputs(" horizontal-mirror-rotate-270", stdout);
        if (surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR)
            fputs(" inherit", stdout);
        puts("");
    }
    fputs("  - Supported composite alpha:", stdout);
    {
        if (surface_capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
            fputs(" opaque", stdout);
        if (surface_capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
            fputs(" pre-multiplied", stdout);
        if (surface_capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
            fputs(" post-multiplied", stdout);
        if (surface_capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
            fputs(" inherit", stdout);
        puts("");
    }
    {
        uint32_t nformats;
        assert(vkGetPhysicalDeviceSurfaceFormatsKHR(phy_device, surface, &nformats, NULL) == VK_SUCCESS);
        VkSurfaceFormatKHR * formats = malloc(nformats * sizeof(*formats));
        assert(vkGetPhysicalDeviceSurfaceFormatsKHR(phy_device, surface, &nformats, formats) == VK_SUCCESS);
        fputs("  - Supported surface formats:", stdout);
        for (size_t i = 0; i < nformats; i++){
            printf(" %i", formats[i].format); // Look-up by hand
        }
        puts("");
        free(formats);
    }
    {
        fputs("  - Supported vertex formats:", stdout);
        VkFormatProperties format_properties;
        for (VkFormat format = 0; format < VK_FORMAT_ASTC_12x12_SRGB_BLOCK; format++){
            vkGetPhysicalDeviceFormatProperties(phy_device, (VkFormat) format, &format_properties);
            if (format_properties.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT)
                printf(" %i", format); // Look-up by hand
        }
        fputs("\n", stdout);
    }
    {
        fputs("  - Supported depth formats:", stdout);
        VkFormatProperties format_properties;
        for (VkFormat format = 0; format < VK_FORMAT_ASTC_12x12_SRGB_BLOCK; format++){
            vkGetPhysicalDeviceFormatProperties(phy_device, (VkFormat) format, &format_properties);
            if (format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
                printf(" %i", format); // Look-up by hand
        }
        fputs("\n", stdout);
    }
    {
        fputs("  - Supported color formats:", stdout);
        VkFormatProperties format_properties;
        for (VkFormat format = 0; format < VK_FORMAT_ASTC_12x12_SRGB_BLOCK; format++){
            vkGetPhysicalDeviceFormatProperties(phy_device, (VkFormat) format, &format_properties);
            if (format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
                printf(" %i", format); // Look-up by hand
        }
        fputs("\n", stdout);
    }
}
