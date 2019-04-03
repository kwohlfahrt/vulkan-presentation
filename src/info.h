#pragma once

#include <vulkan/vulkan.h>

void matchingQueues(VkPhysicalDevice const phy_device, VkQueueFlags const flags,
                    uint32_t * const nmatching_queues, uint32_t * const matching_queues);
