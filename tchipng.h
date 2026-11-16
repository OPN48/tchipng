#ifndef TCHIPNG_H
#define TCHIPNG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TCHIPNG_OK 0
#define TCHIPNG_ERR_ARG -1
#define TCHIPNG_ERR_NOMEM -2
#define TCHIPNG_ERR_ENCODE -3

/* 甜气可见色压缩。rgb 为行优先 RGB，stride 是每行字节数。成功时 *out_png 需用 tchipng_free 释放。 */
int tchipng_compress_rgb(
    const uint8_t *rgb,
    int width,
    int height,
    int stride,
    uint8_t **out_png,
    size_t *out_size);

/* 量化到最多 256 个原图真实色。out_rgb 为紧凑行优先 RGB，成功时用 tchipng_free 释放。 */
int tchipng_quantize_rgb(
    const uint8_t *rgb,
    int width,
    int height,
    int stride,
    uint8_t **out_rgb,
    size_t *out_size);

void tchipng_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif
