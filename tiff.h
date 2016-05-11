#pragma once

#include <stddef.h>
#include <vulkan/vulkan.h>

int writeTiff(char const * const filename, char const * const data,
              const VkExtent2D size, const size_t nchannels);
