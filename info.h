#pragma once

#include <vulkan/vulkan.h>

void printPhysicalDeviceInfo(VkPhysicalDevice phy_device, VkSurfaceKHR surface);
void printLayers();
void printExtensions();
void printSurfaceCapabilities(VkPhysicalDevice phy_device, VkSurfaceKHR surface);
