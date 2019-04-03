#include "info.h"

#include <stdlib.h>

// Note: VK_DEVICE_TRANSFER_BIT implied by GRAPHICS_BIT and COMPUTE_BIT
void matchingQueues(VkPhysicalDevice const phy_device, VkQueueFlags const flags,
                    uint32_t * const nmatching_queues, uint32_t * const matching_queues) {
    uint32_t nqueues;
    vkGetPhysicalDeviceQueueFamilyProperties(phy_device, &nqueues, NULL);

    VkQueueFamilyProperties * properties = malloc(nqueues * sizeof(*properties));
    vkGetPhysicalDeviceQueueFamilyProperties(phy_device, &nqueues, properties);

    if (matching_queues == NULL) {
        *nmatching_queues = 0;
        for (size_t i = 0; i < nqueues; i++)
            if ((flags & properties[i].queueFlags) == flags)
                *nmatching_queues += 1;
    } else {
        for (size_t i = 0, j = 0; i < nqueues && j < *nmatching_queues; i++)
            if ((flags & properties[i].queueFlags) == flags) {
                matching_queues[j] = i;
                j++;
            }
    }
}
