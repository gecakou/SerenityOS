#include <AK/FileSystemPath.h>
#include <AK/MappedFile.h>
#include <AK/NetworkOrdered.h>
#include <LibDraw/PNGLoader.h>
#include <LibDraw/puff.c>
#include <fcntl.h>
#include <serenity.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

//#define PNG_STOPWATCH_DEBUG

struct PNG_IHDR {
    NetworkOrdered<u32> width;
    NetworkOrdered<u32> height;
    u8 bit_depth { 0 };
    u8 color_type { 0 };
    u8 compression_method { 0 };
    u8 filter_method { 0 };
    u8 interlace_method { 0 };
};

static_assert(sizeof(PNG_IHDR) == 13);

struct Scanline {
    u8 filter { 0 };
    ByteBuffer data {};
};

struct [[gnu::packed]] PaletteEntry
{
    u8 r;
    u8 g;
    u8 b;
    //u8 a;
};

struct [[gnu::packed]] Triplet
{
    u8 r;
    u8 g;
    u8 b;
};

struct [[gnu::packed]] Triplet16
{
    u16 r;
    u16 g;
    u16 b;
};

struct [[gnu::packed]] Quad16
{
    u16 r;
    u16 g;
    u16 b;
    u16 a;
};

struct PNGLoadingContext {
    int width { -1 };
    int height { -1 };
    u8 bit_depth { 0 };
    u8 color_type { 0 };
    u8 compression_method { 0 };
    u8 filter_method { 0 };
    u8 interlace_method { 0 };
    u8 bytes_per_pixel { 0 };
    bool has_seen_zlib_header { false };
    bool has_alpha() const { return color_type & 4 || palette_transparency_data.size() > 0; }
    Vector<Scanline> scanlines;
    RefPtr<GraphicsBitmap> bitmap;
    u8* decompression_buffer { nullptr };
    int decompression_buffer_size { 0 };
    Vector<u8> compressed_data;
    Vector<PaletteEntry> palette_data;
    Vector<u8> palette_transparency_data;
};

class Streamer {
public:
    Streamer(const u8* data, int size)
        : m_original_data(data)
        , m_original_size(size)
        , m_data_ptr(data)
        , m_size_remaining(size)
    {
    }

    template<typename T>
    bool read(T& value)
    {
        if (m_size_remaining < (int)sizeof(T))
            return false;
        value = *((const NetworkOrdered<T>*)m_data_ptr);
        m_data_ptr += sizeof(T);
        m_size_remaining -= sizeof(T);
        return true;
    }

    bool read_bytes(u8* buffer, int count)
    {
        if (m_size_remaining < count)
            return false;
        memcpy(buffer, m_data_ptr, count);
        m_data_ptr += count;
        m_size_remaining -= count;
        return true;
    }

    bool wrap_bytes(ByteBuffer& buffer, int count)
    {
        if (m_size_remaining < count)
            return false;
        buffer = ByteBuffer::wrap(m_data_ptr, count);
        m_data_ptr += count;
        m_size_remaining -= count;
        return true;
    }

    bool at_end() const { return !m_size_remaining; }

private:
    const u8* m_original_data;
    int m_original_size;
    const u8* m_data_ptr;
    int m_size_remaining;
};

static RefPtr<GraphicsBitmap> load_png_impl(const u8*, int);
static bool process_chunk(Streamer&, PNGLoadingContext& context);

RefPtr<GraphicsBitmap> load_png(const StringView& path)
{
    MappedFile mapped_file(path);
    if (!mapped_file.is_valid())
        return nullptr;
    auto bitmap = load_png_impl((const u8*)mapped_file.data(), mapped_file.size());
    if (bitmap)
        bitmap->set_mmap_name(String::format("GraphicsBitmap [%dx%d] - Decoded PNG: %s", bitmap->width(), bitmap->height(), canonicalized_path(path).characters()));
    return bitmap;
}

RefPtr<GraphicsBitmap> load_png_from_memory(const u8* data, size_t length)
{
    auto bitmap = load_png_impl(data, length);
    if (bitmap)
        bitmap->set_mmap_name(String::format("GraphicsBitmap [%dx%d] - Decoded PNG: <memory>", bitmap->width(), bitmap->height()));
    return bitmap;
}

[[gnu::always_inline]] static inline u8 paeth_predictor(int a, int b, int c)
{
    int p = a + b - c;
    int pa = abs(p - a);
    int pb = abs(p - b);
    int pc = abs(p - c);
    if (pa <= pb && pa <= pc)
        return a;
    if (pb <= pc)
        return b;
    return c;
}

union [[gnu::packed]] Pixel
{
    RGBA32 rgba { 0 };
    u8 v[4];
    struct {
        u8 r;
        u8 g;
        u8 b;
        u8 a;
    };
};
static_assert(sizeof(Pixel) == 4);

template<bool has_alpha, u8 filter_type>
[[gnu::always_inline]] static inline void unfilter_impl(GraphicsBitmap& bitmap, int y, const void* dummy_scanline_data)
{
    auto* dummy_scanline = (const Pixel*)dummy_scanline_data;
    if constexpr (filter_type == 0) {
        auto* pixels = (Pixel*)bitmap.scanline(y);
        for (int i = 0; i < bitmap.width(); ++i) {
            auto& x = pixels[i];
            swap(x.r, x.b);
        }
    }

    if constexpr (filter_type == 1) {
        auto* pixels = (Pixel*)bitmap.scanline(y);
        swap(pixels[0].r, pixels[0].b);
        for (int i = 1; i < bitmap.width(); ++i) {
            auto& x = pixels[i];
            swap(x.r, x.b);
            auto& a = (const Pixel&)pixels[i - 1];
            x.v[0] += a.v[0];
            x.v[1] += a.v[1];
            x.v[2] += a.v[2];
            if constexpr (has_alpha)
                x.v[3] += a.v[3];
        }
        return;
    }
    if constexpr (filter_type == 2) {
        auto* pixels = (Pixel*)bitmap.scanline(y);
        auto* pixels_y_minus_1 = y == 0 ? dummy_scanline : (Pixel*)bitmap.scanline(y - 1);
        for (int i = 0; i < bitmap.width(); ++i) {
            auto& x = pixels[i];
            swap(x.r, x.b);
            const Pixel& b = pixels_y_minus_1[i];
            x.v[0] += b.v[0];
            x.v[1] += b.v[1];
            x.v[2] += b.v[2];
            if constexpr (has_alpha)
                x.v[3] += b.v[3];
        }
        return;
    }
    if constexpr (filter_type == 3) {
        auto* pixels = (Pixel*)bitmap.scanline(y);
        auto* pixels_y_minus_1 = y == 0 ? dummy_scanline : (Pixel*)bitmap.scanline(y - 1);
        for (int i = 0; i < bitmap.width(); ++i) {
            auto& x = pixels[i];
            swap(x.r, x.b);
            Pixel a;
            if (i != 0)
                a = pixels[i - 1];
            const Pixel& b = pixels_y_minus_1[i];
            x.v[0] = x.v[0] + ((a.v[0] + b.v[0]) / 2);
            x.v[1] = x.v[1] + ((a.v[1] + b.v[1]) / 2);
            x.v[2] = x.v[2] + ((a.v[2] + b.v[2]) / 2);
            if constexpr (has_alpha)
                x.v[3] = x.v[3] + ((a.v[3] + b.v[3]) / 2);
        }
        return;
    }
    if constexpr (filter_type == 4) {
        auto* pixels = (Pixel*)bitmap.scanline(y);
        auto* pixels_y_minus_1 = y == 0 ? dummy_scanline : (Pixel*)bitmap.scanline(y - 1);
        for (int i = 0; i < bitmap.width(); ++i) {
            auto& x = pixels[i];
            swap(x.r, x.b);
            Pixel a;
            const Pixel& b = pixels_y_minus_1[i];
            Pixel c;
            if (i != 0) {
                a = pixels[i - 1];
                c = pixels_y_minus_1[i - 1];
            }
            x.v[0] += paeth_predictor(a.v[0], b.v[0], c.v[0]);
            x.v[1] += paeth_predictor(a.v[1], b.v[1], c.v[1]);
            x.v[2] += paeth_predictor(a.v[2], b.v[2], c.v[2]);
            if constexpr (has_alpha)
                x.v[3] += paeth_predictor(a.v[3], b.v[3], c.v[3]);
        }
    }
}

[[gnu::noinline]] static void unfilter(PNGLoadingContext& context)
{
    {
#ifdef PNG_STOPWATCH_DEBUG
        Stopwatch sw("load_png_impl: unfilter: unpack");
#endif
        // First unpack the scanlines to RGBA:
        switch (context.color_type) {
        case 2:
            if (context.bit_depth == 8) {
                for (int y = 0; y < context.height; ++y) {
                    auto* triplets = (Triplet*)context.scanlines[y].data.data();
                    for (int i = 0; i < context.width; ++i) {
                        auto& pixel = (Pixel&)context.bitmap->scanline(y)[i];
                        pixel.r = triplets[i].r;
                        pixel.g = triplets[i].g;
                        pixel.b = triplets[i].b;
                        pixel.a = 0xff;
                    }
                }
            } else if (context.bit_depth == 16) {
                for (int y = 0; y < context.height; ++y) {
                    auto* triplets = (Triplet16*)context.scanlines[y].data.data();
                    for (int i = 0; i < context.width; ++i) {
                        auto& pixel = (Pixel&)context.bitmap->scanline(y)[i];
                        pixel.r = triplets[i].r & 0xFF;
                        pixel.g = triplets[i].g & 0xFF;
                        pixel.b = triplets[i].b & 0xFF;
                        pixel.a = 0xff;
                    }
                }
            } else {
                ASSERT_NOT_REACHED();
            }
            break;
        case 6:
            if (context.bit_depth == 8) {
                for (int y = 0; y < context.height; ++y) {
                    memcpy(context.bitmap->scanline(y), context.scanlines[y].data.data(), context.scanlines[y].data.size());
                }
            } else if (context.bit_depth == 16) {
                for (int y = 0; y < context.height; ++y) {
                    auto* triplets = (Quad16*)context.scanlines[y].data.data();
                    for (int i = 0; i < context.width; ++i) {
                        auto& pixel = (Pixel&)context.bitmap->scanline(y)[i];
                        pixel.r = triplets[i].r & 0xFF;
                        pixel.g = triplets[i].g & 0xFF;
                        pixel.b = triplets[i].b & 0xFF;
                        pixel.a = triplets[i].a & 0xFF;
                    }
                }
            } else {
                ASSERT_NOT_REACHED();
            }
            break;
        case 3:
            for (int y = 0; y < context.height; ++y) {
                auto* palette_index = (u8*)context.scanlines[y].data.data();
                for (int i = 0; i < context.width; ++i) {
                    auto& pixel = (Pixel&)context.bitmap->scanline(y)[i];
                    auto& color = context.palette_data.at((int)palette_index[i]);
                    auto transparency = context.palette_transparency_data.size() >= palette_index[i] + 1
                        ? (int)context.palette_transparency_data.data()[palette_index[i]]
                        : 0xFF;
                    pixel.r = color.r;
                    pixel.g = color.g;
                    pixel.b = color.b;
                    pixel.a = transparency;
                }
            }
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

    auto dummy_scanline = ByteBuffer::create_zeroed(context.width * sizeof(RGBA32));

#ifdef PNG_STOPWATCH_DEBUG
    Stopwatch sw("load_png_impl: unfilter: process");
#endif
    for (int y = 0; y < context.height; ++y) {
        auto filter = context.scanlines[y].filter;
        if (filter == 0) {
            if (context.has_alpha())
                unfilter_impl<true, 0>(*context.bitmap, y, dummy_scanline.data());
            else
                unfilter_impl<false, 0>(*context.bitmap, y, dummy_scanline.data());
            continue;
        }
        if (filter == 1) {
            if (context.has_alpha())
                unfilter_impl<true, 1>(*context.bitmap, y, dummy_scanline.data());
            else
                unfilter_impl<false, 1>(*context.bitmap, y, dummy_scanline.data());
            continue;
        }
        if (filter == 2) {
            if (context.has_alpha())
                unfilter_impl<true, 2>(*context.bitmap, y, dummy_scanline.data());
            else
                unfilter_impl<false, 2>(*context.bitmap, y, dummy_scanline.data());
            continue;
        }
        if (filter == 3) {
            if (context.has_alpha())
                unfilter_impl<true, 3>(*context.bitmap, y, dummy_scanline.data());
            else
                unfilter_impl<false, 3>(*context.bitmap, y, dummy_scanline.data());
            continue;
        }
        if (filter == 4) {
            if (context.has_alpha())
                unfilter_impl<true, 4>(*context.bitmap, y, dummy_scanline.data());
            else
                unfilter_impl<false, 4>(*context.bitmap, y, dummy_scanline.data());
            continue;
        }
    }
}

static RefPtr<GraphicsBitmap> load_png_impl(const u8* data, int data_size)
{
#ifdef PNG_STOPWATCH_DEBUG
    Stopwatch sw("load_png_impl: total");
#endif
    const u8* data_ptr = data;
    int data_remaining = data_size;

    const u8 png_header[8] = { 0x89, 'P', 'N', 'G', 13, 10, 26, 10 };
    if (memcmp(data, png_header, sizeof(png_header))) {
        dbgprintf("Invalid PNG header\n");
        return nullptr;
    }

    PNGLoadingContext context;

    context.compressed_data.ensure_capacity(data_size);

    data_ptr += sizeof(png_header);
    data_remaining -= sizeof(png_header);

    {
#ifdef PNG_STOPWATCH_DEBUG
        Stopwatch sw("load_png_impl: read chunks");
#endif
        Streamer streamer(data_ptr, data_remaining);
        while (!streamer.at_end()) {
            if (!process_chunk(streamer, context)) {
                return nullptr;
            }
        }
    }

    {
#ifdef PNG_STOPWATCH_DEBUG
        Stopwatch sw("load_png_impl: uncompress");
#endif
        unsigned long srclen = context.compressed_data.size() - 6;
        unsigned long destlen = context.decompression_buffer_size;
        int ret = puff(context.decompression_buffer, &destlen, context.compressed_data.data() + 2, &srclen);
        if (ret < 0)
            return nullptr;
        context.compressed_data.clear();
    }

    {
#ifdef PNG_STOPWATCH_DEBUG
        Stopwatch sw("load_png_impl: extract scanlines");
#endif
        context.scanlines.ensure_capacity(context.height);
        Streamer streamer(context.decompression_buffer, context.decompression_buffer_size);
        for (int y = 0; y < context.height; ++y) {
            u8 filter;
            if (!streamer.read(filter))
                return nullptr;

            context.scanlines.append({ filter });
            auto& scanline_buffer = context.scanlines.last().data;
            if (!streamer.wrap_bytes(scanline_buffer, context.width * context.bytes_per_pixel))
                return nullptr;
        }
    }

    {
#ifdef PNG_STOPWATCH_DEBUG
        Stopwatch sw("load_png_impl: create bitmap");
#endif
        context.bitmap = GraphicsBitmap::create(context.has_alpha() ? GraphicsBitmap::Format::RGBA32 : GraphicsBitmap::Format::RGB32, { context.width, context.height });
    }

    unfilter(context);

    munmap(context.decompression_buffer, context.decompression_buffer_size);
    context.decompression_buffer = nullptr;
    context.decompression_buffer_size = 0;

    return context.bitmap;
}

static bool process_IHDR(const ByteBuffer& data, PNGLoadingContext& context)
{
    if (data.size() < (int)sizeof(PNG_IHDR))
        return false;
    auto& ihdr = *(const PNG_IHDR*)data.data();
    context.width = ihdr.width;
    context.height = ihdr.height;
    context.bit_depth = ihdr.bit_depth;
    context.color_type = ihdr.color_type;
    context.compression_method = ihdr.compression_method;
    context.filter_method = ihdr.filter_method;
    context.interlace_method = ihdr.interlace_method;

#ifdef PNG_DEBUG
    printf("PNG: %dx%d (%d bpp)\n", context.width, context.height, context.bit_depth);
    printf("     Color type: %d\n", context.color_type);
    printf("Compress Method: %d\n", context.compression_method);
    printf("  Filter Method: %d\n", context.filter_method);
    printf(" Interlace type: %d\n", context.interlace_method);
#endif

    // FIXME: Implement Adam7 deinterlacing
    if (context.interlace_method != 0) {
        dbgprintf("PNGLoader::process_IHDR: Interlaced PNGs not currently supported.\n");
        return false;
    }

    switch (context.color_type) {
    case 0: // Each pixel is a grayscale sample.
    case 4: // Each pixel is a grayscale sample, followed by an alpha sample.
        // FIXME: Implement grayscale PNG support.
        dbgprintf("PNGLoader::process_IHDR: Unsupported grayscale format.\n");
        return false;
    case 2:
        context.bytes_per_pixel = 3 * (ihdr.bit_depth / 8);
        break;
    case 3: // Each pixel is a palette index; a PLTE chunk must appear.
        // FIXME: Implement support for 1/2/4 bit palette based images.
        if (ihdr.bit_depth != 8) {
            dbgprintf("PNGLoader::process_IHDR: Unsupported index-based format (%d bpp).\n", context.bit_depth);
            return false;
        }
        context.bytes_per_pixel = 1;
        break;
    case 6:
        context.bytes_per_pixel = 4 * (ihdr.bit_depth / 8);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    context.decompression_buffer_size = (context.width * context.height * context.bytes_per_pixel + context.height);
    context.decompression_buffer = (u8*)mmap_with_name(nullptr, context.decompression_buffer_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0, "PNG decompression buffer");
    return true;
}

static bool process_IDAT(const ByteBuffer& data, PNGLoadingContext& context)
{
    context.compressed_data.append(data.data(), data.size());
    return true;
}

static bool process_PLTE(const ByteBuffer& data, PNGLoadingContext& context)
{
    context.palette_data.append((const PaletteEntry*)data.data(), data.size() / 3);
    return true;
}

static bool process_tRNS(const ByteBuffer& data, PNGLoadingContext& context)
{
    switch (context.color_type) {
    case 3:
        context.palette_transparency_data.append(data.data(), data.size());
        break;
    }
    return true;
}

static bool process_chunk(Streamer& streamer, PNGLoadingContext& context)
{
    u32 chunk_size;
    if (!streamer.read(chunk_size)) {
        printf("Bail at chunk_size\n");
        return false;
    }
    u8 chunk_type[5];
    chunk_type[4] = '\0';
    if (!streamer.read_bytes(chunk_type, 4)) {
        printf("Bail at chunk_type\n");
        return false;
    }
    ByteBuffer chunk_data;
    if (!streamer.wrap_bytes(chunk_data, chunk_size)) {
        printf("Bail at chunk_data\n");
        return false;
    }
    u32 chunk_crc;
    if (!streamer.read(chunk_crc)) {
        printf("Bail at chunk_crc\n");
        return false;
    }
#ifdef PNG_DEBUG
    printf("Chunk type: '%s', size: %u, crc: %x\n", chunk_type, chunk_size, chunk_crc);
#endif

    if (!strcmp((const char*)chunk_type, "IHDR"))
        return process_IHDR(chunk_data, context);
    if (!strcmp((const char*)chunk_type, "IDAT"))
        return process_IDAT(chunk_data, context);
    if (!strcmp((const char*)chunk_type, "PLTE"))
        return process_PLTE(chunk_data, context);
    if (!strcmp((const char*)chunk_type, "tRNS"))
        return process_tRNS(chunk_data, context);
    return true;
}
