#include "tchipng.h"

#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define TCHIPNG_PALETTE 256

typedef struct ColorRec {
    uint8_t rgb[3];
    uint64_t count;
} ColorRec;

typedef struct Box {
    ColorRec *items;
    int count;
} Box;

static int cmp_u32(const void *left, const void *right) {
    uint32_t a = *(const uint32_t *)left;
    uint32_t b = *(const uint32_t *)right;
    if (a < b) {
        return -1;
    }
    if (a > b) {
        return 1;
    }
    return 0;
}

static int collect_unique(const uint8_t *rgb, int width, int height, int stride, ColorRec **out_items, int *out_count) {
    size_t pixels = (size_t)width * (size_t)height;
    uint32_t *keys = (uint32_t *)malloc(pixels * sizeof(uint32_t));
    ColorRec *items;
    size_t index;
    int unique = 0;
    if (keys == NULL) {
        return TCHIPNG_ERR_NOMEM;
    }
    for (int y = 0; y < height; y++) {
        const uint8_t *row = rgb + (size_t)y * (size_t)stride;
        for (int x = 0; x < width; x++) {
            keys[(size_t)y * (size_t)width + (size_t)x] =
                ((uint32_t)row[x * 3] << 16) | ((uint32_t)row[x * 3 + 1] << 8) | (uint32_t)row[x * 3 + 2];
        }
    }
    qsort(keys, pixels, sizeof(uint32_t), cmp_u32);
    for (index = 0; index < pixels; index++) {
        if (index == 0 || keys[index] != keys[index - 1]) {
            unique++;
        }
    }
    items = (ColorRec *)calloc((size_t)unique, sizeof(ColorRec));
    if (items == NULL) {
        free(keys);
        return TCHIPNG_ERR_NOMEM;
    }
    unique = 0;
    for (index = 0; index < pixels; index++) {
        if (index == 0 || keys[index] != keys[index - 1]) {
            items[unique].rgb[0] = (uint8_t)(keys[index] >> 16);
            items[unique].rgb[1] = (uint8_t)(keys[index] >> 8);
            items[unique].rgb[2] = (uint8_t)keys[index];
            items[unique].count = 1;
            unique++;
        } else {
            items[unique - 1].count++;
        }
    }
    free(keys);
    *out_items = items;
    *out_count = unique;
    return TCHIPNG_OK;
}

static void box_score(const Box *box, int *channel, double *score) {
    double sum_w = 0.0;
    double sum[3] = {0.0, 0.0, 0.0};
    double var[3] = {0.0, 0.0, 0.0};
    double mean[3];
    int best = 0;
    int i;
    int c;
    if (box->count < 2) {
        *channel = 0;
        *score = -1.0;
        return;
    }
    for (i = 0; i < box->count; i++) {
        double weight = (double)box->items[i].count;
        sum_w += weight;
        for (c = 0; c < 3; c++) {
            sum[c] += (double)box->items[i].rgb[c] * weight;
        }
    }
    for (c = 0; c < 3; c++) {
        mean[c] = sum[c] / sum_w;
    }
    for (i = 0; i < box->count; i++) {
        double weight = (double)box->items[i].count;
        for (c = 0; c < 3; c++) {
            double delta = (double)box->items[i].rgb[c] - mean[c];
            var[c] += delta * delta * weight;
        }
    }
    for (c = 0; c < 3; c++) {
        var[c] /= sum_w;
    }
    for (c = 1; c < 3; c++) {
        if (var[c] > var[best]) {
            best = c;
        }
    }
    *channel = best;
    *score = var[best] * sum_w;
}

typedef struct SortRef {
    int index;
    uint8_t value;
} SortRef;

static int cmp_channel_stable(const void *left, const void *right) {
    const SortRef *a = (const SortRef *)left;
    const SortRef *b = (const SortRef *)right;
    if (a->value < b->value) {
        return -1;
    }
    if (a->value > b->value) {
        return 1;
    }
    if (a->index < b->index) {
        return -1;
    }
    if (a->index > b->index) {
        return 1;
    }
    return 0;
}

static int split_box(Box *box, int channel, Box *left, Box *right) {
    SortRef *refs = (SortRef *)malloc((size_t)box->count * sizeof(SortRef));
    ColorRec *ordered;
    uint64_t *cumulative;
    uint64_t total = 0;
    int midpoint;
    int i;
    if (refs == NULL) {
        return TCHIPNG_ERR_NOMEM;
    }
    for (i = 0; i < box->count; i++) {
        refs[i].index = i;
        refs[i].value = box->items[i].rgb[channel];
    }
    qsort(refs, (size_t)box->count, sizeof(SortRef), cmp_channel_stable);
    ordered = (ColorRec *)malloc((size_t)box->count * sizeof(ColorRec));
    cumulative = (uint64_t *)malloc((size_t)box->count * sizeof(uint64_t));
    if (ordered == NULL || cumulative == NULL) {
        free(refs);
        free(ordered);
        free(cumulative);
        return TCHIPNG_ERR_NOMEM;
    }
    for (i = 0; i < box->count; i++) {
        ordered[i] = box->items[refs[i].index];
        total += ordered[i].count;
        cumulative[i] = total;
    }
    free(refs);
    {
        double target = (double)total / 2.0;
        int lo = 0;
        int hi = box->count;
        while (lo < hi) {
            int mid = (lo + hi) / 2;
            if ((double)cumulative[mid] < target) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        midpoint = lo;
    }
    free(cumulative);
    if (midpoint < 1) {
        midpoint = 1;
    }
    if (midpoint > box->count - 1) {
        midpoint = box->count - 1;
    }
    left->items = (ColorRec *)malloc((size_t)midpoint * sizeof(ColorRec));
    right->items = (ColorRec *)malloc((size_t)(box->count - midpoint) * sizeof(ColorRec));
    if (left->items == NULL || right->items == NULL) {
        free(left->items);
        free(right->items);
        free(ordered);
        left->items = NULL;
        right->items = NULL;
        return TCHIPNG_ERR_NOMEM;
    }
    memcpy(left->items, ordered, (size_t)midpoint * sizeof(ColorRec));
    memcpy(right->items, ordered + midpoint, (size_t)(box->count - midpoint) * sizeof(ColorRec));
    left->count = midpoint;
    right->count = box->count - midpoint;
    free(ordered);
    return TCHIPNG_OK;
}

static void snap_color(const Box *box, uint8_t out[3]) {
    double sum_w = 0.0;
    double sum[3] = {0.0, 0.0, 0.0};
    double best = 0.0;
    int best_index = 0;
    int i;
    int c;
    for (i = 0; i < box->count; i++) {
        double weight = (double)box->items[i].count;
        sum_w += weight;
        for (c = 0; c < 3; c++) {
            sum[c] += (double)box->items[i].rgb[c] * weight;
        }
    }
    for (c = 0; c < 3; c++) {
        sum[c] /= sum_w;
    }
    for (i = 0; i < box->count; i++) {
        double distance = 0.0;
        for (c = 0; c < 3; c++) {
            double delta = (double)box->items[i].rgb[c] - sum[c];
            distance += delta * delta;
        }
        if (i == 0 || distance < best) {
            best = distance;
            best_index = i;
        }
    }
    memcpy(out, box->items[best_index].rgb, 3);
}

static int cmp_rgb3(const void *left, const void *right) {
    const uint8_t *a = (const uint8_t *)left;
    const uint8_t *b = (const uint8_t *)right;
    int i;
    for (i = 0; i < 3; i++) {
        if (a[i] < b[i]) {
            return -1;
        }
        if (a[i] > b[i]) {
            return 1;
        }
    }
    return 0;
}

static int nearest_index(const uint8_t rgb[3], const uint8_t *palette, int palette_count) {
    int best = 0;
    int best_distance = 0;
    int i;
    for (i = 0; i < palette_count; i++) {
        int dr = (int)rgb[0] - (int)palette[i * 3];
        int dg = (int)rgb[1] - (int)palette[i * 3 + 1];
        int db = (int)rgb[2] - (int)palette[i * 3 + 2];
        int distance = dr * dr + dg * dg + db * db;
        if (i == 0 || distance < best_distance) {
            best_distance = distance;
            best = i;
        }
    }
    return best;
}

static int fill_palette(Box *boxes, int box_count, ColorRec *all_colors, int color_count, uint8_t *palette, int *palette_count) {
    uint8_t raw[TCHIPNG_PALETTE * 3];
    uint8_t unique[TCHIPNG_PALETTE * 3];
    int unique_count = 0;
    int i;
    for (i = 0; i < box_count; i++) {
        snap_color(&boxes[i], raw + i * 3);
    }
    qsort(raw, (size_t)box_count, 3, cmp_rgb3);
    for (i = 0; i < box_count; i++) {
        if (unique_count == 0 || memcmp(unique + (unique_count - 1) * 3, raw + i * 3, 3) != 0) {
            memcpy(unique + unique_count * 3, raw + i * 3, 3);
            unique_count++;
        }
    }
    if (unique_count < TCHIPNG_PALETTE && unique_count < color_count) {
        typedef struct {
            int index;
            int distance;
        } Extra;
        Extra *extras = (Extra *)malloc((size_t)color_count * sizeof(Extra));
        int extra_count = 0;
        int e;
        if (extras == NULL) {
            return TCHIPNG_ERR_NOMEM;
        }
        for (i = 0; i < color_count; i++) {
            int seen = 0;
            int u;
            for (u = 0; u < unique_count; u++) {
                if (memcmp(all_colors[i].rgb, unique + u * 3, 3) == 0) {
                    seen = 1;
                    break;
                }
            }
            if (!seen) {
                extras[extra_count].index = i;
                extras[extra_count].distance = 0;
                {
                    int best = 0;
                    int best_distance = 0;
                    int p;
                    for (p = 0; p < unique_count; p++) {
                        int dr = (int)all_colors[i].rgb[0] - (int)unique[p * 3];
                        int dg = (int)all_colors[i].rgb[1] - (int)unique[p * 3 + 1];
                        int db = (int)all_colors[i].rgb[2] - (int)unique[p * 3 + 2];
                        int distance = dr * dr + dg * dg + db * db;
                        if (p == 0 || distance < best_distance) {
                            best_distance = distance;
                            best = p;
                        }
                    }
                    (void)best;
                    extras[extra_count].distance = best_distance;
                }
                extra_count++;
            }
        }
        for (e = 1; e < extra_count; e++) {
            Extra key = extras[e];
            int slot = e;
            while (slot > 0) {
                Extra prev = extras[slot - 1];
                int greater = key.distance > prev.distance ||
                    (key.distance == prev.distance && key.index > prev.index);
                if (!greater) {
                    break;
                }
                extras[slot] = prev;
                slot--;
            }
            extras[slot] = key;
        }
        for (e = 0; e < extra_count && unique_count < TCHIPNG_PALETTE; e++) {
            memcpy(unique + unique_count * 3, all_colors[extras[e].index].rgb, 3);
            unique_count++;
        }
        free(extras);
    }
    memcpy(palette, unique, (size_t)unique_count * 3);
    *palette_count = unique_count;
    return TCHIPNG_OK;
}

static int build_boxes(ColorRec *colors, int color_count, Box **out_boxes, int *out_count) {
    Box *boxes = (Box *)calloc(TCHIPNG_PALETTE, sizeof(Box));
    int count = 1;
    if (boxes == NULL) {
        return TCHIPNG_ERR_NOMEM;
    }
    boxes[0].items = (ColorRec *)malloc((size_t)color_count * sizeof(ColorRec));
    if (boxes[0].items == NULL) {
        free(boxes);
        return TCHIPNG_ERR_NOMEM;
    }
    memcpy(boxes[0].items, colors, (size_t)color_count * sizeof(ColorRec));
    boxes[0].count = color_count;
    while (count < TCHIPNG_PALETTE) {
        int best = -1;
        double best_score = -1.0;
        int best_channel = 0;
        int i;
        Box left;
        Box right;
        int status;
        for (i = 0; i < count; i++) {
            int channel;
            double score;
            box_score(&boxes[i], &channel, &score);
            if (score > best_score) {
                best_score = score;
                best = i;
                best_channel = channel;
            }
        }
        if (best < 0 || best_score < 0.0) {
            break;
        }
        status = split_box(&boxes[best], best_channel, &left, &right);
        if (status != TCHIPNG_OK) {
            for (i = 0; i < count; i++) {
                free(boxes[i].items);
            }
            free(boxes);
            return status;
        }
        free(boxes[best].items);
        boxes[best] = left;
        boxes[count] = right;
        count++;
    }
    *out_boxes = boxes;
    *out_count = count;
    return TCHIPNG_OK;
}

static void order_palette(uint8_t *palette, int palette_count, uint8_t *indexes, size_t pixels) {
    int order[TCHIPNG_PALETTE];
    int luma[TCHIPNG_PALETTE];
    uint8_t remap[TCHIPNG_PALETTE];
    uint8_t sorted[TCHIPNG_PALETTE * 3];
    int i;
    size_t p;
    for (i = 0; i < palette_count; i++) {
        order[i] = i;
        luma[i] = (int)palette[i * 3] * 2 + (int)palette[i * 3 + 1] * 5 + (int)palette[i * 3 + 2];
    }
    for (i = 1; i < palette_count; i++) {
        int key = order[i];
        int slot = i;
        while (slot > 0) {
            int prev = order[slot - 1];
            int before = luma[key] < luma[prev] || (luma[key] == luma[prev] && key < prev);
            if (!before) {
                break;
            }
            order[slot] = prev;
            slot--;
        }
        order[slot] = key;
    }
    for (i = 0; i < palette_count; i++) {
        remap[order[i]] = (uint8_t)i;
        memcpy(sorted + i * 3, palette + order[i] * 3, 3);
    }
    memcpy(palette, sorted, (size_t)palette_count * 3);
    for (p = 0; p < pixels; p++) {
        indexes[p] = remap[indexes[p]];
    }
}

static uint32_t png_crc_table[256];
static int png_crc_ready = 0;

static void init_png_crc(void) {
    uint32_t n;
    if (png_crc_ready) {
        return;
    }
    for (n = 0; n < 256; n++) {
        uint32_t c = n;
        int k;
        for (k = 0; k < 8; k++) {
            c = (c & 1u) ? (0xedb88320u ^ (c >> 1)) : (c >> 1);
        }
        png_crc_table[n] = c;
    }
    png_crc_ready = 1;
}

static uint32_t png_crc(const uint8_t *data, size_t size) {
    uint32_t c = 0xffffffffu;
    size_t i;
    init_png_crc();
    for (i = 0; i < size; i++) {
        c = png_crc_table[(c ^ data[i]) & 0xffu] ^ (c >> 8);
    }
    return c ^ 0xffffffffu;
}

static int append_bytes(uint8_t **buffer, size_t *length, size_t *capacity, const void *data, size_t size) {
    if (*length + size > *capacity) {
        size_t next = *capacity == 0 ? 4096 : *capacity;
        uint8_t *grown;
        while (next < *length + size) {
            next *= 2;
        }
        grown = (uint8_t *)realloc(*buffer, next);
        if (grown == NULL) {
            return TCHIPNG_ERR_NOMEM;
        }
        *buffer = grown;
        *capacity = next;
    }
    memcpy(*buffer + *length, data, size);
    *length += size;
    return TCHIPNG_OK;
}

static int append_chunk(uint8_t **buffer, size_t *length, size_t *capacity, const char type[4], const uint8_t *data, size_t size) {
    uint8_t header[8];
    uint8_t *crc_bytes;
    uint32_t crc;
    uint32_t stored_size = (uint32_t)size;
    int status;
    header[0] = (uint8_t)(stored_size >> 24);
    header[1] = (uint8_t)(stored_size >> 16);
    header[2] = (uint8_t)(stored_size >> 8);
    header[3] = (uint8_t)stored_size;
    memcpy(header + 4, type, 4);
    status = append_bytes(buffer, length, capacity, header, 8);
    if (status != TCHIPNG_OK) {
        return status;
    }
    if (size > 0) {
        status = append_bytes(buffer, length, capacity, data, size);
        if (status != TCHIPNG_OK) {
            return status;
        }
    }
    crc_bytes = (uint8_t *)malloc(4 + size);
    if (crc_bytes == NULL) {
        return TCHIPNG_ERR_NOMEM;
    }
    memcpy(crc_bytes, type, 4);
    if (size > 0) {
        memcpy(crc_bytes + 4, data, size);
    }
    crc = png_crc(crc_bytes, 4 + size);
    free(crc_bytes);
    header[0] = (uint8_t)(crc >> 24);
    header[1] = (uint8_t)(crc >> 16);
    header[2] = (uint8_t)(crc >> 8);
    header[3] = (uint8_t)crc;
    return append_bytes(buffer, length, capacity, header, 4);
}

static int encode_indexed_png(int width, int height, const uint8_t *indexes, const uint8_t *palette, int palette_count, uint8_t **out_png, size_t *out_size) {
    uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    uint8_t ihdr[13];
    size_t raw_size = (size_t)height * ((size_t)width + 1);
    uint8_t *raw = (uint8_t *)malloc(raw_size);
    uLongf bound;
    uint8_t *compressed;
    uint8_t *png = NULL;
    size_t length = 0;
    size_t capacity = 0;
    int y;
    int status = TCHIPNG_OK;
    if (raw == NULL) {
        return TCHIPNG_ERR_NOMEM;
    }
    for (y = 0; y < height; y++) {
        raw[(size_t)y * ((size_t)width + 1)] = 0;
        memcpy(raw + (size_t)y * ((size_t)width + 1) + 1, indexes + (size_t)y * (size_t)width, (size_t)width);
    }
    bound = compressBound((uLong)raw_size);
    compressed = (uint8_t *)malloc(bound);
    if (compressed == NULL) {
        free(raw);
        return TCHIPNG_ERR_NOMEM;
    }
    if (compress2(compressed, &bound, raw, (uLong)raw_size, 9) != Z_OK) {
        free(raw);
        free(compressed);
        return TCHIPNG_ERR_ENCODE;
    }
    free(raw);
    ihdr[0] = (uint8_t)((uint32_t)width >> 24);
    ihdr[1] = (uint8_t)((uint32_t)width >> 16);
    ihdr[2] = (uint8_t)((uint32_t)width >> 8);
    ihdr[3] = (uint8_t)width;
    ihdr[4] = (uint8_t)((uint32_t)height >> 24);
    ihdr[5] = (uint8_t)((uint32_t)height >> 16);
    ihdr[6] = (uint8_t)((uint32_t)height >> 8);
    ihdr[7] = (uint8_t)height;
    ihdr[8] = 8;
    ihdr[9] = 3;
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;
    status = append_bytes(&png, &length, &capacity, signature, 8);
    if (status == TCHIPNG_OK) {
        status = append_chunk(&png, &length, &capacity, "IHDR", ihdr, 13);
    }
    if (status == TCHIPNG_OK) {
        status = append_chunk(&png, &length, &capacity, "PLTE", palette, (size_t)palette_count * 3);
    }
    if (status == TCHIPNG_OK) {
        status = append_chunk(&png, &length, &capacity, "IDAT", compressed, (size_t)bound);
    }
    if (status == TCHIPNG_OK) {
        status = append_chunk(&png, &length, &capacity, "IEND", NULL, 0);
    }
    free(compressed);
    if (status != TCHIPNG_OK) {
        free(png);
        return status;
    }
    *out_png = png;
    *out_size = length;
    return TCHIPNG_OK;
}

static int build_indexed(
    const uint8_t *rgb,
    int width,
    int height,
    int stride,
    uint8_t **out_indexes,
    uint8_t *palette,
    int *palette_count) {
    ColorRec *colors = NULL;
    int color_count = 0;
    Box *boxes = NULL;
    int box_count = 0;
    uint8_t *indexes = NULL;
    size_t pixels;
    int y;
    int x;
    int status;
    int i;
    status = collect_unique(rgb, width, height, stride, &colors, &color_count);
    if (status != TCHIPNG_OK) {
        return status;
    }
    status = build_boxes(colors, color_count, &boxes, &box_count);
    if (status != TCHIPNG_OK) {
        free(colors);
        return status;
    }
    status = fill_palette(boxes, box_count, colors, color_count, palette, palette_count);
    for (i = 0; i < box_count; i++) {
        free(boxes[i].items);
    }
    free(boxes);
    if (status != TCHIPNG_OK) {
        free(colors);
        return status;
    }
    pixels = (size_t)width * (size_t)height;
    indexes = (uint8_t *)malloc(pixels);
    if (indexes == NULL) {
        free(colors);
        return TCHIPNG_ERR_NOMEM;
    }
    for (y = 0; y < height; y++) {
        const uint8_t *row = rgb + (size_t)y * (size_t)stride;
        for (x = 0; x < width; x++) {
            indexes[(size_t)y * (size_t)width + (size_t)x] = (uint8_t)nearest_index(row + x * 3, palette, *palette_count);
        }
    }
    free(colors);
    *out_indexes = indexes;
    return TCHIPNG_OK;
}

static int expand_indexed(const uint8_t *indexes, const uint8_t *palette, size_t pixels, uint8_t **out_rgb, size_t *out_size) {
    uint8_t *packed = (uint8_t *)malloc(pixels * 3);
    size_t index;
    if (packed == NULL) {
        return TCHIPNG_ERR_NOMEM;
    }
    for (index = 0; index < pixels; index++) {
        memcpy(packed + index * 3, palette + (size_t)indexes[index] * 3, 3);
    }
    *out_rgb = packed;
    *out_size = pixels * 3;
    return TCHIPNG_OK;
}

int tchipng_quantize_rgb(
    const uint8_t *rgb,
    int width,
    int height,
    int stride,
    uint8_t **out_rgb,
    size_t *out_size) {
    uint8_t *indexes = NULL;
    uint8_t palette[TCHIPNG_PALETTE * 3];
    int palette_count = 0;
    int status;
    if (rgb == NULL || out_rgb == NULL || out_size == NULL || width <= 0 || height <= 0 || stride < width * 3) {
        return TCHIPNG_ERR_ARG;
    }
    status = build_indexed(rgb, width, height, stride, &indexes, palette, &palette_count);
    if (status != TCHIPNG_OK) {
        return status;
    }
    status = expand_indexed(indexes, palette, (size_t)width * (size_t)height, out_rgb, out_size);
    free(indexes);
    return status;
}

int tchipng_compress_rgb(const uint8_t *rgb, int width, int height, int stride, uint8_t **out_png, size_t *out_size) {
    uint8_t *indexes = NULL;
    uint8_t palette[TCHIPNG_PALETTE * 3];
    int palette_count = 0;
    size_t pixels;
    int status;
    if (rgb == NULL || out_png == NULL || out_size == NULL || width <= 0 || height <= 0 || stride < width * 3) {
        return TCHIPNG_ERR_ARG;
    }
    status = build_indexed(rgb, width, height, stride, &indexes, palette, &palette_count);
    if (status != TCHIPNG_OK) {
        return status;
    }
    pixels = (size_t)width * (size_t)height;
    order_palette(palette, palette_count, indexes, pixels);
    status = encode_indexed_png(width, height, indexes, palette, palette_count, out_png, out_size);
    free(indexes);
    return status;
}

void tchipng_free(void *ptr) {
    free(ptr);
}
