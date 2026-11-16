#include "tchipng.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define TCHIPNG_JPEG_QUALITY 90

static void usage(void) {
    fprintf(stderr, "usage: tchipng [-q quality] <input> <output>\n");
}

static int ends_with_ci(const char *path, const char *suffix) {
    size_t path_len = strlen(path);
    size_t suffix_len = strlen(suffix);
    size_t index;
    if (path_len < suffix_len) {
        return 0;
    }
    for (index = 0; index < suffix_len; index++) {
        char left = path[path_len - suffix_len + index];
        char right = suffix[index];
        if (left >= 'A' && left <= 'Z') {
            left = (char)(left - 'A' + 'a');
        }
        if (left != right) {
            return 0;
        }
    }
    return 1;
}

static int parse_quality(const char *text, int *quality) {
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (text[0] == '\0' || *end != '\0' || value < 1 || value > 100) {
        return 0;
    }
    *quality = (int)value;
    return 1;
}

static int write_all(const char *path, const uint8_t *data, size_t size) {
    FILE *file = fopen(path, "wb");
    size_t written;
    if (file == NULL) {
        return 0;
    }
    written = fwrite(data, 1, size, file);
    if (fclose(file) != 0 || written != size) {
        return 0;
    }
    return 1;
}

static int write_png(const char *output, const uint8_t *rgb, int width, int height) {
    uint8_t *png = NULL;
    size_t png_size = 0;
    int status = tchipng_compress_rgb(rgb, width, height, width * 3, &png, &png_size);
    int ok;
    if (status != TCHIPNG_OK) {
        fprintf(stderr, "tchipng: compress failed (%d)\n", status);
        return 0;
    }
    ok = write_all(output, png, png_size);
    tchipng_free(png);
    if (!ok) {
        fprintf(stderr, "tchipng: write failed: %s\n", output);
    }
    return ok;
}

static int write_jpeg(const char *output, const uint8_t *rgb, int width, int height, int quality) {
    uint8_t *quantized = NULL;
    size_t quant_size = 0;
    int status = tchipng_quantize_rgb(rgb, width, height, width * 3, &quantized, &quant_size);
    int ok;
    if (status != TCHIPNG_OK) {
        fprintf(stderr, "tchipng: quantize failed (%d)\n", status);
        return 0;
    }
    ok = stbi_write_jpg(output, width, height, 3, quantized, quality) != 0;
    tchipng_free(quantized);
    if (!ok) {
        fprintf(stderr, "tchipng: write failed: %s\n", output);
    }
    return ok;
}

static int parse_args(int argc, char **argv, const char **input, const char **output, int *quality) {
    int index;
    *quality = TCHIPNG_JPEG_QUALITY;
    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "-q") == 0) {
            if (index + 1 >= argc || !parse_quality(argv[index + 1], quality)) {
                usage();
                return 0;
            }
            index++;
            continue;
        }
        if (*input == NULL) {
            *input = argv[index];
            continue;
        }
        if (*output == NULL) {
            *output = argv[index];
            continue;
        }
        usage();
        return 0;
    }
    if (*input == NULL || *output == NULL) {
        usage();
        return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    const char *input = NULL;
    const char *output = NULL;
    int quality = TCHIPNG_JPEG_QUALITY;
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char *pixels;
    int ok;
    if (!parse_args(argc, argv, &input, &output, &quality)) {
        return 1;
    }
    pixels = stbi_load(input, &width, &height, &channels, 3);
    if (pixels == NULL || width <= 0 || height <= 0) {
        fprintf(stderr, "tchipng: decode failed: %s\n", input);
        return 1;
    }
    if (ends_with_ci(output, ".png")) {
        ok = write_png(output, pixels, width, height);
    } else if (ends_with_ci(output, ".jpg") || ends_with_ci(output, ".jpeg")) {
        ok = write_jpeg(output, pixels, width, height, quality);
    } else {
        fprintf(stderr, "tchipng: output must be .png, .jpg, or .jpeg\n");
        ok = 0;
    }
    stbi_image_free(pixels);
    return ok ? 0 : 1;
}
