#include "util.h"

#include <stdio.h>
#include <stdlib.h>

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
