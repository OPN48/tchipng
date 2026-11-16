#include "tchipng.h"

#include <android/bitmap.h>
#include <jni.h>
#include <stdlib.h>
#include <string.h>

static void expand_rgb565(uint16_t pixel, uint8_t rgb[3]) {
    uint8_t red = (uint8_t)((pixel >> 11) & 0x1fu);
    uint8_t green = (uint8_t)((pixel >> 5) & 0x3fu);
    uint8_t blue = (uint8_t)(pixel & 0x1fu);
    rgb[0] = (uint8_t)((red << 3) | (red >> 2));
    rgb[1] = (uint8_t)((green << 2) | (green >> 4));
    rgb[2] = (uint8_t)((blue << 3) | (blue >> 2));
}

static uint8_t *bitmap_to_rgb(const AndroidBitmapInfo *info, const void *pixels) {
    size_t count = (size_t)info->width * (size_t)info->height;
    uint8_t *rgb = (uint8_t *)malloc(count * 3u);
    int y;
    int x;
    if (rgb == NULL) {
        return NULL;
    }
    for (y = 0; y < (int)info->height; y++) {
        const uint8_t *row = (const uint8_t *)pixels + (size_t)y * (size_t)info->stride;
        uint8_t *dst = rgb + (size_t)y * (size_t)info->width * 3u;
        if (info->format == ANDROID_BITMAP_FORMAT_RGBA_8888) {
            for (x = 0; x < (int)info->width; x++) {
                dst[x * 3] = row[x * 4];
                dst[x * 3 + 1] = row[x * 4 + 1];
                dst[x * 3 + 2] = row[x * 4 + 2];
            }
        } else {
            const uint16_t *packed = (const uint16_t *)row;
            for (x = 0; x < (int)info->width; x++) {
                expand_rgb565(packed[x], dst + x * 3);
            }
        }
    }
    return rgb;
}

JNIEXPORT jbyteArray JNICALL
Java_com_xiaomei_album_core_image_TchiPng_nativeCompress(JNIEnv *env, jclass clazz, jobject bitmap) {
    AndroidBitmapInfo info;
    void *pixels = NULL;
    uint8_t *rgb = NULL;
    uint8_t *png = NULL;
    size_t png_size = 0;
    jbyteArray result = NULL;
    int status;
    (void)clazz;
    if (bitmap == NULL || AndroidBitmap_getInfo(env, bitmap, &info) != ANDROID_BITMAP_RESULT_SUCCESS) {
        return NULL;
    }
    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888 && info.format != ANDROID_BITMAP_FORMAT_RGB_565) {
        return NULL;
    }
    if (info.width <= 0 || info.height <= 0) {
        return NULL;
    }
    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) != ANDROID_BITMAP_RESULT_SUCCESS || pixels == NULL) {
        return NULL;
    }
    rgb = bitmap_to_rgb(&info, pixels);
    AndroidBitmap_unlockPixels(env, bitmap);
    if (rgb == NULL) {
        return NULL;
    }
    status = tchipng_compress_rgb(rgb, (int)info.width, (int)info.height, (int)info.width * 3, &png, &png_size);
    free(rgb);
    if (status != TCHIPNG_OK || png == NULL || png_size > 0x7fffffff) {
        tchipng_free(png);
        return NULL;
    }
    result = (*env)->NewByteArray(env, (jsize)png_size);
    if (result != NULL) {
        (*env)->SetByteArrayRegion(env, result, 0, (jsize)png_size, (const jbyte *)png);
    }
    tchipng_free(png);
    return result;
}
