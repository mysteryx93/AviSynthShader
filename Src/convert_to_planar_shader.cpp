#include <map>
#include <tuple>
#include "ConvertShader.h"



template <bool STACK16>
static void __stdcall
yuv_to_shader_2_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float*) noexcept
{
    for (int p = 0; p < 3; ++p) {
        const uint8_t* s = srcp[p];
        const uint8_t* lsb = s + height * spitch;
        uint8_t* d = dstp[p];

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                d[2 * x + 0] = STACK16 ? lsb[x] : 0;
                d[2 * x + 1] = s[x];
            }
            s += spitch;
            d += dpitch;
            if (STACK16) {
                lsb += spitch;
            }
        }
    }
}


template <bool STACK16>
static void __stdcall
yuv_to_shader_3_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float* buff) noexcept
{
    constexpr float rcp = 1.0f / (STACK16 ? 65535 : 255);

    for (int p = 0; p < 3; ++p) {
        const uint8_t* s = srcp[p];
        const uint8_t* lsb = s + height * spitch;
        uint8_t* d = dstp[p];

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                buff[x] = rcp * (STACK16 ? ((s[x] << 8) | lsb[x]) : s[x]);
            }
            convert_float_to_half<NO_SIMD>(d, buff, width);
            s += spitch;
            d += dpitch;
            if (STACK16) {
                lsb += spitch;
            }
        }
    }
}


template <bool IS_RGB32>
static void __stdcall
rgb_to_shader_1_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float*) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;

    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    uint8_t* dr = dstp[0];
    uint8_t* dg = dstp[1];
    uint8_t* db = dstp[2];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            db[x] = s[step * x + 0];
            dg[x] = s[step * x + 1];
            dr[x] = s[step * x + 2];
        }
        s -= spitch;
        dr += dpitch;
        dg += dpitch;
        db += dpitch;
    }
}


template <bool IS_RGB32>
static void __stdcall
rgb_to_shader_2_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float*) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;

    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    uint8_t* dr = dstp[0];
    uint8_t* dg = dstp[1];
    uint8_t* db = dstp[2];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            db[2 * x + 0] = 0;
            db[2 * x + 1] = s[step * x + 0];
            dg[2 * x + 0] = 0;
            dg[2 * x + 1] = s[step * x + 1];
            dr[2 * x + 0] = 0;
            dr[2 * x + 1] = s[step * x + 2];
        }
        s -= spitch;
        dr += dpitch;
        dg += dpitch;
        db += dpitch;
    }
}


template <bool IS_RGB32>
static void __stdcall
rgb_to_shader_3_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float* buff) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;
    constexpr float rcp = 1.0f / 255;

    const uint8_t* s = srcp[0] + (height - 1) * spitch;
    uint8_t* dr = dstp[0];
    uint8_t* dg = dstp[1];
    uint8_t* db = dstp[2];

    float* bb = buff;
    float* bg = bb + ((width + 7) & ~7); // must be aligned 32 bytes
    float* br = bg + ((width + 7) & ~7); // must be aligned 32 bytes

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            bb[x] = s[step * x + 0] * rcp;
            bg[x] = s[step * x + 1] * rcp;
            br[x] = s[step * x + 2] * rcp;
        }
        convert_float_to_half<NO_SIMD>(dr, br, width);
        convert_float_to_half<NO_SIMD>(dg, bg, width);
        convert_float_to_half<NO_SIMD>(db, bb, width);
        s -= spitch;
        dr += dpitch;
        dg += dpitch;
        db += dpitch;
    }
}


convert_shader_t get_to_shader_planar(int precision, int pix_type, bool stack16, arch_t arch)
{
    using std::make_tuple;
    constexpr int rgb24 = VideoInfo::CS_BGR24;
    constexpr int rgb32 = VideoInfo::CS_BGR32;
    constexpr int yv24 = VideoInfo::CS_YV24;

    arch = NO_SIMD; // simd versions are not implemented yet

    std::map<std::tuple<int, int, bool, arch_t>, convert_shader_t> func;

    func[make_tuple(2, yv24, false, NO_SIMD)] = yuv_to_shader_2_c<false>;
    func[make_tuple(2, yv24, true, NO_SIMD)] = yuv_to_shader_2_c<true>;

    func[make_tuple(3, yv24, false, NO_SIMD)] = yuv_to_shader_3_c<false>;
    func[make_tuple(3, yv24, true, NO_SIMD)] = yuv_to_shader_3_c<true>;

    func[make_tuple(1, rgb24, false, NO_SIMD)] = rgb_to_shader_1_c<false>;
    func[make_tuple(1, rgb32, false, NO_SIMD)] = rgb_to_shader_1_c<true>;
    func[make_tuple(2, rgb24, false, NO_SIMD)] = rgb_to_shader_2_c<false>;
    func[make_tuple(2, rgb32, false, NO_SIMD)] = rgb_to_shader_2_c<true>;
    func[make_tuple(3, rgb24, false, NO_SIMD)] = rgb_to_shader_3_c<false>;
    func[make_tuple(3, rgb32, false, NO_SIMD)] = rgb_to_shader_3_c<true>;

    return func[make_tuple(precision, pix_type, stack16, arch)];
}

