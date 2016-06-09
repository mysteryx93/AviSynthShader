#include <algorithm>
#include <map>
#include <tuple>
#include "ConvertShader.h"


template <bool STACK16>
static void __stdcall
shader_to_yuv_2_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float*) noexcept
{
    for (int p = 0; p < 3; ++p) {
        const uint8_t* s = srcp[p];
        uint8_t* d = dstp[p];
        uint8_t* lsb = d + height * dpitch;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (!STACK16) {
                    d[x] = std::min((s[2 * x] >> 7) + s[2 * x + 1], 255);
                } else {
                    lsb[x] = s[2 * x];
                    d[x] = s[2 * x + 1];
                }
            }
            s += spitch;
            d += dpitch;
            if (STACK16) {
                lsb += dpitch;
            }
        }
    }
}


template <bool STACK16>
static void __stdcall
shader_to_yuv_3_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float* buff) noexcept
{
    for (int p = 0; p < 3; ++p) {
        const uint8_t* s = srcp[p];
        uint8_t* d = dstp[p];
        uint8_t* lsb = d + height * dpitch;

        for (int y = 0; y < height; ++y) {
            convert_half_to_float<NO_SIMD>(buff, s, width);
            for (int x = 0; x < width; ++x) {
                if (!STACK16) {
                    d[x] = static_cast<uint8_t>(buff[x] * 255 + 0.5f);
                } else {
                    union {
                        uint16_t v16;
                        uint8_t v8[2];
                    } dst;

                    dst.v16 = static_cast<uint16_t>(buff[x] * 65535 + 0.5f);
                    lsb[x] = dst.v8[0];
                    d[x] = dst.v8[1];
                }
            }
            s += spitch;
            d += dpitch;
            if (STACK16) {
                lsb += dpitch;
            }
        }
    }
}


template <bool IS_RGB32>
static void __stdcall
shader_to_rgb_1_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float* buff) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;

    const uint8_t* sr = srcp[0];
    const uint8_t* sg = srcp[1];
    const uint8_t* sb = srcp[2];
    uint8_t* d = dstp[0] + (height - 1) * dpitch;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            d[step * x + 0] = sb[x];
            d[step * x + 1] = sg[x];
            d[step * x + 2] = sr[x];
            if (IS_RGB32) {
                d[4 * x + 3] = 0;
            }
        }
        sr += spitch;
        sg += spitch;
        sb += spitch;
        d -= dpitch;
    }
}


template <bool IS_RGB32>
static void __stdcall
shader_to_rgb_2_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float* buff) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;

    const uint8_t* sr = srcp[0];
    const uint8_t* sg = srcp[1];
    const uint8_t* sb = srcp[2];
    uint8_t* d = dstp[0] + (height - 1) * dpitch;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            d[step * x + 0] = std::min((sb[2 * x + 0] >> 7) + sb[2 * x + 1], 255);
            d[step * x + 1] = std::min((sg[2 * x + 0] >> 7) + sb[2 * x + 1], 255);
            d[step * x + 2] = std::min((sr[2 * x + 0] >> 7) + sb[2 * x + 1], 255);
            if (IS_RGB32) {
                d[4 * x + 3] = 0;
            }
        }
        sr += spitch;
        sg += spitch;
        sb += spitch;
        d -= dpitch;
    }
}


template <bool IS_RGB32>
static void __stdcall
shader_to_rgb_3_c(uint8_t** dstp, const uint8_t** srcp, const int dpitch,
    const int spitch, const int width, const int height, float* buff) noexcept
{
    constexpr size_t step = IS_RGB32 ? 4 : 3;

    const uint8_t* sr = srcp[0];
    const uint8_t* sg = srcp[1];
    const uint8_t* sb = srcp[2];
    uint8_t* d = dstp[0] + (height - 1) * dpitch;

    float* bb = buff;
    float* bg = bb + ((width + 7) & ~7); // must be aligned 32 bytes
    float* br = bg + ((width + 7) & ~7); // must be aligned 32 bytes

    for (int y = 0; y < height; ++y) {
        convert_half_to_float<NO_SIMD>(br, sr, width);
        convert_half_to_float<NO_SIMD>(bg, sg, width);
        convert_half_to_float<NO_SIMD>(bb, sb, width);
        for (int x = 0; x < width; ++x) {
            d[step * x + 0] = static_cast<uint8_t>(bb[x] * 255 + 0.5f);
            d[step * x + 1] = static_cast<uint8_t>(bg[x] * 255 + 0.5f);
            d[step * x + 2] = static_cast<uint8_t>(br[x] * 255 + 0.5f);
            if (IS_RGB32) {
                d[4 * x + 3] = 0;
            }
        }
        sr += spitch;
        sg += spitch;
        sb += spitch;
        d -= dpitch;
    }
}


convert_shader_t get_from_shader_planar(int precision, int pix_type, bool stack16, arch_t arch)
{
    using std::make_tuple;
    constexpr int rgb24 = VideoInfo::CS_BGR24;
    constexpr int rgb32 = VideoInfo::CS_BGR32;
    constexpr int yv24 = VideoInfo::CS_YV24;

    arch = NO_SIMD; // simd versions are not implemented yet

    std::map<std::tuple<int, int, bool, arch_t>, convert_shader_t> func;

    func[make_tuple(1, rgb24, false, NO_SIMD)] = shader_to_rgb_1_c<false>;
    func[make_tuple(1, rgb32, false, NO_SIMD)] = shader_to_rgb_1_c<true>;

    func[make_tuple(2, yv24, false, NO_SIMD)] = shader_to_yuv_2_c<false>;
    func[make_tuple(2, yv24, true, NO_SIMD)] = shader_to_yuv_2_c<true>;

    func[make_tuple(2, rgb24, false, NO_SIMD)] = shader_to_rgb_2_c<false>;
    func[make_tuple(2, rgb32, false, NO_SIMD)] = shader_to_rgb_2_c<true>;

    func[make_tuple(3, yv24, false, NO_SIMD)] = shader_to_yuv_3_c<false>;
    func[make_tuple(3, yv24, true, NO_SIMD)] = shader_to_yuv_3_c<true>;

    func[make_tuple(3, rgb24, false, NO_SIMD)] = shader_to_rgb_3_c<false>;
    func[make_tuple(3, rgb32, false, NO_SIMD)] = shader_to_rgb_3_c<true>;

    return func[make_tuple(precision, pix_type, stack16, arch)];


}

