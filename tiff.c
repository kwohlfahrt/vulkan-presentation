#include "tiff.h"

#include <tiffio.h>

int writeTiff(char const * const filename, char const * const data,
              const VkExtent2D size, const size_t nchannels) {
    TIFF* tif = TIFFOpen(filename, "w");
    if (tif == NULL)
        return 1;

    const size_t sample_bits = 8;

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, (uint32) size.width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, (uint32) size.height);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, nchannels);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, sample_bits);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);

    for (size_t i = 0; i < size.height; i++) {
        if (TIFFWriteScanline(tif, (void *) &data[i * size.width * nchannels], i, 0) < 0) {
            TIFFClose(tif);
            return 1;
        }
    }

    TIFFClose(tif);
    return 0;
}

int readTiffRGBA(char const * const filename, VkExtent2D * const size, uint32_t * const data) {
    TIFF * tif = TIFFOpen(filename, "r");
    if (tif == NULL)
        return 1;

    if (data == NULL) {
        TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &size->height);
        TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &size->width);
    } else {
        if (!TIFFReadRGBAImage(tif, size->width, size->height, data, 0)){
            TIFFClose(tif);
            return 1;
        }
    }

    TIFFClose(tif);
    return 0;
}
