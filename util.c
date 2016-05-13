#include "util.h"

#include <stdio.h>
#include <stdlib.h>

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
