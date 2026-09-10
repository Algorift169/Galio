#include "../include/png.h"
#include "../../include/fs/vfs.h"
#include "../../include/mm/heap.h"
#include "../../include/lib/kprintf.h"
#include "../../include/lib/string.h"

#define PNG_MAX_DIMENSION 4096u
#define PNG_MAX_OUTPUT (64u * 1024u * 1024u)
#define PNG_MAX_IDAT (32u * 1024u * 1024u)

typedef struct {
    const u8 *data;
    u32 size;
    u32 bit;
} bit_reader_t;

typedef struct {
    u16 code;
    u8 length;
    u16 symbol;
} huffman_code_t;

typedef struct {
    huffman_code_t codes[288];
    u32 count;
} huffman_table_t;

static u32 png_be32(const u8 *data) {
    return ((u32)data[0] << 24) | ((u32)data[1] << 16) |
           ((u32)data[2] << 8) | (u32)data[3];
}

static u8 png_bytes_equal(const u8 *left, const u8 *right, u32 size) {
    for (u32 i = 0; i < size; i++) if (left[i] != right[i]) return 0;
    return 1;
}

static u8 bit_read(bit_reader_t *reader, u32 count, u32 *value) {
    u32 result = 0;
    if (!reader || !value || count > 24u || reader->bit > reader->size * 8u ||
        count > reader->size * 8u - reader->bit) return 0;
    for (u32 i = 0; i < count; i++) {
        if (reader->data[reader->bit >> 3] & (1u << (reader->bit & 7u))) result |= 1u << i;
        reader->bit++;
    }
    *value = result;
    return 1;
}

static u16 reverse_bits(u32 value, u32 count) {
    u16 result = 0;
    for (u32 i = 0; i < count; i++) result = (u16)((result << 1) | ((value >> i) & 1u));
    return result;
}

static u8 huffman_build(huffman_table_t *table, const u8 *lengths, u32 count) {
    u16 next_code[16] = {0};
    u16 bl_count[16] = {0};
    u32 code = 0;
    if (!table || !lengths || count > 288u) return 0;
    table->count = 0;
    for (u32 i = 0; i < count; i++) {
        if (lengths[i] > 15u) return 0;
        if (lengths[i]) bl_count[lengths[i]]++;
    }
    for (u32 bits = 1; bits <= 15u; bits++) {
        code = (code + bl_count[bits - 1u]) << 1;
        next_code[bits] = (u16)code;
    }
    for (u32 symbol = 0; symbol < count; symbol++) {
        u8 length = lengths[symbol];
        if (!length) continue;
        if (table->count >= 288u) return 0;
        table->codes[table->count].code = reverse_bits(next_code[length], length);
        table->codes[table->count].length = length;
        table->codes[table->count].symbol = (u16)symbol;
        table->count++;
        next_code[length]++;
    }
    return table->count != 0;
}

static int huffman_read(bit_reader_t *reader, const huffman_table_t *table) {
    u32 value;
    u16 code = 0;
    if (!reader || !table) return -1;
    for (u32 length = 1; length <= 15u; length++) {
        if (!bit_read(reader, 1u, &value)) return -1;
        code = (u16)(code | ((u16)value << (length - 1u)));
        for (u32 i = 0; i < table->count; i++) {
            if (table->codes[i].length == length && table->codes[i].code == code) return table->codes[i].symbol;
        }
    }
    return -1;
}

static u8 inflate_block(bit_reader_t *reader, u8 *output, u32 output_size, u32 *written,
                        const huffman_table_t *literal_table, const huffman_table_t *distance_table) {
    static const u16 length_base[] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const u8 length_extra[] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static const u16 distance_base[] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static const u8 distance_extra[] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    for (;;) {
        int symbol = huffman_read(reader, literal_table);
        if (symbol < 0) return 0;
        if (symbol < 256) {
            if (*written >= output_size) return 0;
            output[(*written)++] = (u8)symbol;
        } else if (symbol == 256) {
            return 1;
        } else if (symbol <= 285) {
            u32 extra = 0;
            u32 length_index = (u32)symbol - 257u;
            if (!bit_read(reader, length_extra[length_index], &extra)) return 0;
            u32 length = length_base[length_index] + extra;
            int distance_symbol = huffman_read(reader, distance_table);
            if (distance_symbol < 0 || distance_symbol >= 30) return 0;
            extra = 0;
            if (!bit_read(reader, distance_extra[distance_symbol], &extra)) return 0;
            u32 distance = distance_base[distance_symbol] + extra;
            if (distance > *written || length > output_size - *written) return 0;
            for (u32 i = 0; i < length; i++) output[*written + i] = output[*written - distance + i];
            *written += length;
        } else {
            return 0;
        }
    }
}

static u8 inflate_zlib(const u8 *data, u32 size, u8 *output, u32 output_size, u32 *written) {
    static const u8 fixed_lengths[288] = {
        [0 ... 143] = 8, [144 ... 255] = 9, [256 ... 279] = 7, [280 ... 287] = 8
    };
    static const u8 fixed_distance[32] = {[0 ... 31] = 5};
    bit_reader_t reader = {data, size, 16u};
    huffman_table_t literal_table, distance_table, code_table;
    u8 lengths[320], code_lengths[19];
    u32 final = 0;
    *written = 0;
    if (!data || size < 6u || (data[0] & 0x0Fu) != 8u || ((u16)data[0] << 8 | data[1]) % 31u != 0u) return 0;
    do {
        u32 type;
        if (!bit_read(&reader, 1u, &final) || !bit_read(&reader, 2u, &type) || type == 3u) return 0;
        if (type == 0u) {
            u32 length, inverse;
            reader.bit = (reader.bit + 7u) & ~7u;
            if (!bit_read(&reader, 16u, &length) || !bit_read(&reader, 16u, &inverse) ||
                ((length ^ inverse) & 0xFFFFu) != 0xFFFFu || length > output_size - *written ||
                reader.bit / 8u > reader.size || length > reader.size - reader.bit / 8u) return 0;
            memcpy(output + *written, data + reader.bit / 8u, length);
            *written += length;
            reader.bit += length * 8u;
        } else {
            if (type == 1u) {
                if (!huffman_build(&literal_table, fixed_lengths, 288u) || !huffman_build(&distance_table, fixed_distance, 32u)) return 0;
            } else {
                u32 hlit, hdist, hclen, value;
                static const u8 order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
                if (!bit_read(&reader, 5u, &hlit) || !bit_read(&reader, 5u, &hdist) || !bit_read(&reader, 4u, &hclen)) return 0;
                hlit += 257u; hdist += 1u; hclen += 4u;
                if (hlit > 286u || hdist > 32u) return 0;
                memset(code_lengths, 0, sizeof(code_lengths));
                for (u32 i = 0; i < hclen; i++) if (!bit_read(&reader, 3u, &value)) return 0; else code_lengths[order[i]] = (u8)value;
                if (!huffman_build(&code_table, code_lengths, 19u)) return 0;
                u32 total = hlit + hdist, index = 0;
                while (index < total) {
                    int symbol = huffman_read(&reader, &code_table);
                    if (symbol < 0) return 0;
                    if (symbol <= 15) {
                        lengths[index++] = (u8)symbol;
                    } else {
                        u32 repeat, extra = 0; u8 previous;
                        if (symbol == 16) { if (index == 0u || !bit_read(&reader, 2u, &extra)) return 0; repeat = extra + 3u; previous = lengths[index - 1u]; }
                        else if (symbol == 17) { if (!bit_read(&reader, 3u, &extra)) return 0; repeat = extra + 3u; previous = 0; }
                        else if (symbol == 18) { if (!bit_read(&reader, 7u, &extra)) return 0; repeat = extra + 11u; previous = 0; }
                        else return 0;
                        if (repeat > total - index) return 0;
                        while (repeat--) lengths[index++] = previous;
                    }
                }
                if (!huffman_build(&literal_table, lengths, hlit) || !huffman_build(&distance_table, lengths + hlit, hdist)) return 0;
            }
            if (!inflate_block(&reader, output, output_size, written, &literal_table, &distance_table)) return 0;
        }
    } while (!final);
    return *written == output_size;
}

static u8 png_unfilter(u8 *raw, u32 width, u32 height, u8 *pixels) {
    u32 stride = width * 3u;
    for (u32 y = 0; y < height; y++) {
        u8 filter = raw[y * (stride + 1u)];
        u8 *row = raw + y * (stride + 1u) + 1u;
        u8 *previous = y ? pixels + (y - 1u) * stride : NULL;
        for (u32 x = 0; x < stride; x++) {
            u8 left = x >= 3u ? pixels[y * stride + x - 3u] : 0u;
            u8 up = previous ? previous[x] : 0u;
            u8 upper_left = previous && x >= 3u ? previous[x - 3u] : 0u;
            u8 predictor = 0u;
            if (filter == 1u) predictor = left;
            else if (filter == 2u) predictor = up;
            else if (filter == 3u) predictor = (u8)(((u32)left + up) / 2u);
            else if (filter == 4u) {
                s32 p = (s32)left + up - upper_left;
                s32 pa = p > left ? p - left : left - p;
                s32 pb = p > up ? p - up : up - p;
                s32 pc = p > upper_left ? p - upper_left : upper_left - p;
                predictor = pa <= pb && pa <= pc ? left : (pb <= pc ? up : upper_left);
            } else if (filter != 0u) return 0;
            pixels[y * stride + x] = (u8)(row[x] + predictor);
        }
    }
    return 1;
}

u8 png_load_from_vfs(const char *path, png_image_t *image) {
    static const u8 signature[8] = {137,80,78,71,13,10,26,10};
    u8 *file = NULL, *compressed = NULL, *raw = NULL;
    u32 file_size, file_pos = 8u, compressed_size = 0u, width = 0, height = 0;
    u8 bit_depth = 0, color_type = 0, interlace = 0, *pixels = NULL;
    u32 fd, read_total = 0, expected_raw, expected_pixels;
    if (!image || !path) return 0;
    image->pixels = NULL; image->width = 0; image->height = 0;
    file_size = vfs_size(path);
    if (file_size < 33u || file_size > PNG_MAX_IDAT) return 0;
    file = kmalloc(file_size);
    if (!file) return 0;
    fd = vfs_open(path);
    if (fd == VFS_INVALID_FD) { kfree(file); return 0; }
    while (read_total < file_size) {
        u32 got = vfs_read_fd(fd, file + read_total, file_size - read_total);
        if (!got) break;
        read_total += got;
    }
    vfs_close(fd);
    kprintf("[GUI] Wallpaper read: size=%u read=%u\n", file_size, read_total);
    if (read_total != file_size || !png_bytes_equal(file, signature, 8u)) goto fail;
    while (file_pos + 12u <= file_size) {
        u32 length = png_be32(file + file_pos), data_pos = file_pos + 8u;
        if (length > file_size - data_pos - 4u) goto fail;
        if (png_bytes_equal(file + data_pos - 4u, (const u8 *)"IHDR", 4u)) {
            if (length != 13u) goto fail;
            width = png_be32(file + data_pos); height = png_be32(file + data_pos + 4u);
            bit_depth = file[data_pos + 8u]; color_type = file[data_pos + 9u]; interlace = file[data_pos + 12u];
            if (!width || !height || width > PNG_MAX_DIMENSION || height > PNG_MAX_DIMENSION ||
                bit_depth != 8u || color_type != 2u || interlace != 0u) goto fail;
        } else if (png_bytes_equal(file + data_pos - 4u, (const u8 *)"IDAT", 4u)) {
            if (length > PNG_MAX_IDAT - compressed_size) goto fail;
            if (!compressed) compressed = kmalloc(file_size);
            if (!compressed) goto fail;
            memcpy(compressed + compressed_size, file + data_pos, length); compressed_size += length;
        } else if (png_bytes_equal(file + data_pos - 4u, (const u8 *)"IEND", 4u)) break;
        file_pos = data_pos + length + 4u;
    }
    if (!width || !height || !compressed_size || width > 0xFFFFFFFFu / 3u ||
        height > 0xFFFFFFFFu / (width * 3u + 1u)) goto fail;
    expected_raw = height * (width * 3u + 1u); expected_pixels = width * height * 3u;
    if (expected_raw > PNG_MAX_OUTPUT || expected_pixels > PNG_MAX_OUTPUT) goto fail;
    raw = kmalloc(expected_raw); pixels = kmalloc(expected_pixels);
    if (!raw || !pixels) goto fail;
    u32 written = 0;
    kprintf("[GUI] Wallpaper PNG: %ux%u compressed=%u raw=%u\n", width, height, compressed_size, expected_raw);
    if (!inflate_zlib(compressed, compressed_size, raw, expected_raw, &written)) {
        kprintf("[GUI] Wallpaper inflate failed: wrote=%u expected=%u\n", written, expected_raw);
        goto fail;
    }
    if (!png_unfilter(raw, width, height, pixels)) goto fail;
    image->width = width; image->height = height; image->pixels = pixels;
    kprintf("[GUI] Wallpaper loaded: %s (%ux%u)\n", path, width, height);
    kfree(raw); kfree(compressed); kfree(file); return 1;
fail:
    if (pixels) kfree(pixels);
    if (raw) kfree(raw);
    if (compressed) kfree(compressed);
    if (file) kfree(file);
    kprintf("[GUI] Wallpaper PNG load failed: %s\n", path);
    return 0;
}

void png_free(png_image_t *image) {
    if (!image) return;
    if (image->pixels) kfree(image->pixels);
    image->pixels = NULL; image->width = 0; image->height = 0;
}