#define ANKERL_NANOBENCH_IMPLEMENT
#include <nanobench.h>

#include <cstddef> // for: size_t
#include <cstdint> // for: uint8_t

#include <vector>  // for: std::vector<T>
#include <cstdlib> // for: rand()
#include <ctime>   // for: seeding rand()

#if defined(__AVX2__)
    #include <immintrin.h>
#endif // defined(__AVX2__)

// -----------------------------------------------------------------------------
// NOTE: before benchmarking make sure, that CPU 'min_freq' and 'max_freq' is
// the same maximum number, and 'governor' is'performance'.
//
// Get state:
//   $ cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq
//   $ cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq
//   $ cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
//
// Set state:
//   - GUI-way (https://github.com/vagnum08/cpupower-gui):
//     1. Install GUI: $ sudo apt install cpupower-gui
//     2. ./cpupower-gui
//   - CLI-way:
//     1. TODO: describe
// -----------------------------------------------------------------------------

// NOTE: to print cpu info we use 'lscpu' instead `/proc/cpuinfo`, since it
// contains CPU model names.
void print_lscpu()
{
    FILE* fp = popen("lscpu", "r");
    if (fp == nullptr)
    {
        fputs("Failed to run lscpu command!\n", stderr);
        fflush(stderr);
    }
    else
    {
        fputs("lscpu:\n", stdout);

        char buffer[256] { '\0' };
        while( fgets(buffer, sizeof(buffer), fp) != nullptr )
        {
            fputs(buffer, stdout);
        }
        fflush(stdout);

        fclose(fp);
    }
}

// -----------------------------------------------------------------------------

std::vector<uint8_t> make_ascending_data(size_t size)
{
    std::vector<uint8_t> data(size);
    for(size_t i = 0; i < size; ++i)
    {
        data[i] = static_cast<uint8_t>( (i+1) % 256 );
    }
    return data;
}

std::vector<uint8_t> make_random_data(size_t size)
{
    std::vector<uint8_t> data(size);
    std::srand(static_cast<unsigned>(std::time(nullptr))); // Seed random number generator

    for(size_t i = 0; i < size; ++i)
    {
        data[i] = static_cast<uint8_t>(std::rand() % 256); // Random value in range [0, 255]
    }

    return data;
}

// Compares an RGBA buffer to an RGB buffer (ignoring alpha channel)
bool compare_rgba_to_rgb(const uint8_t* rgba, const uint8_t* rgb, size_t num_pixels)
{
    uint8_t rgba_r, rgba_g, rgba_b, rgba_a;
    uint8_t rgb_r, rgb_g, rgb_b;
    for(size_t i = 0; i < num_pixels; ++i)
    {
        rgba_r = rgba[(i * 4)    ];
        rgba_g = rgba[(i * 4) + 1];
        rgba_b = rgba[(i * 4) + 2];
        rgba_a = rgba[(i * 4) + 3];

        rgb_r  = rgb[(i * 3)    ];
        rgb_g  = rgb[(i * 3) + 1];
        rgb_b  = rgb[(i * 3) + 2];

        if( (rgba_r != rgb_r) || (rgba_g != rgb_g) || (rgba_b != rgb_b) )
        {
            // -----------------------------------------------------------------
            fputs("rgba: [", stdout);
            for(size_t r = 0; r < num_pixels * 4; ++r)
            {
                fprintf(stdout, "%d ", rgba[r]);
                if(((r+1) % 4) == 0) { fputs("| ", stdout); }
            }
            fputs("]\nrgb: [", stdout);
            for(size_t r = 0; r < num_pixels * 3; ++r)
            {
                fprintf(stdout, "%d ", rgb[r]);
                if(((r+1) % 3) == 0) { fputs("| ", stdout); }
            }
            fputs("]\n", stdout);
            // -----------------------------------------------------------------

            fprintf(
                stdout,
                "Mismatch at i=%zu: RGBA(%d, %d, %d, %d) but got RGB(%d, %d, %d)\n",
                i, rgba_r, rgba_g, rgba_g, rgba_a, rgb_r, rgb_g, rgb_b
            );
            fflush(stdout);

            return false; // Stop as soon as we detect an error
        }
    }
    return true;
}

// -----------------------------------------------------------------------------

void copy_rgba_to_rgb__memcpy(const uint8_t* rgba, uint8_t* rgb, size_t num_pixels)
{
    size_t i = 0;
    for(; i < num_pixels; ++i)
    {
        // Copy RGB components from rgba-buffer into rgb-buffer
        memcpy(
            rgb  + (i * (sizeof(uint8_t) * 3)),
            rgba + (i * (sizeof(uint8_t) * 4)),
            sizeof(uint8_t) * 3
        );
    }
}

/*
    Faster then `convert_rgba_to_rgb__memcpy()`, since using `memcpy()` in a
    loop introduces unnecessary function-call overhead. Instead we directly
    copy the values using simple raw-pointers arithmetic, which is optimized by
    the compilers.
*/
void copy_rgba_to_rgb__raw_ptr(const uint8_t* rgba, uint8_t* rgb, size_t num_pixels)
{
    size_t i = 0;
    for(; i < num_pixels; ++i)
    {
        rgb[0] = rgba[0]; // Copy R
        rgb[1] = rgba[1]; // Copy G
        rgb[2] = rgba[2]; // Copy B
        rgba += 4;
        rgb  += 3;
    }
}

/*
    Faster then `convert_rgba_to_rgb__raw_ptr()`, since unrolling the loop can
    improve performance by reducing loop-control overhead.
    - Why?
        - Less branching   --> CPU predicts better
        - Fewer iterations --> Saves loop overhead
        - More instructions per loop iteration --> Boosts throughput
*/
void copy_rgba_to_rgb__raw_ptr__4pixels(const uint8_t* rgba, uint8_t* rgb, size_t num_pixels)
{
    const size_t num_pixels_unrolled = num_pixels / 4; // Process 4 pixels per iteration

    size_t i = 0;
    for(; i < num_pixels_unrolled; ++i)
    {
        // Pixel 1
        rgb[0] = rgba[0];
        rgb[1] = rgba[1];
        rgb[2] = rgba[2];
        rgba += 4;
        rgb  += 3;

        // Pixel 2
        rgb[0] = rgba[0];
        rgb[1] = rgba[1];
        rgb[2] = rgba[2];
        rgba += 4;
        rgb  += 3;

        // Pixel 3
        rgb[0] = rgba[0];
        rgb[1] = rgba[1];
        rgb[2] = rgba[2];
        rgba += 4;
        rgb  += 3;

        // Pixel 4
        rgb[0] = rgba[0];
        rgb[1] = rgba[1];
        rgb[2] = rgba[2];
        rgba += 4;
        rgb  += 3;
    }

    // Handle the remaining pixels
    i = num_pixels_unrolled * 4; // Number of processed pixels
    for(; i < num_pixels; ++i)
    {
        rgb[0] = rgba[0];
        rgb[1] = rgba[1];
        rgb[2] = rgba[2];
        rgba += 4;
        rgb  += 3;
    }
}

// -----------------------------------------------------------------------------

#if defined(__AVX2__)

    #if !defined(COPY_RGBA_TO_RGB__AVX2__DO_PREFETCH)
        #define COPY_RGBA_TO_RGB__AVX2__DO_PREFETCH 0
    #endif

void copy_rgba_to_rgb__avx2__8pixels(const uint8_t* rgba, uint8_t* rgb, size_t num_pixels)
{
    // Shuffle mask to extract RGB bytes while discarding the Alpha byte
    //
    // The shuffle mask defines how bytes are rearranged in `__m256i` (32-byte) AVX2 registers.
    // Each pixel is stored as [R,G,B,A], but we want only [R,G,B]
    const __m256i shuffle_mask = _mm256_set_epi8(
        -1, -1, -1, -1, // `-1` means 'skipped bytes'
        14,13,12,  10,9,8,  6,5,4,  2,1,0, // Extract 4 RGB from second half

        -1, -1, -1, -1, // `-1` means 'skipped bytes'
        14,13,12,  10,9,8,  6,5,4,  2,1,0  // Extract 4 RGB from first half
    );

    // Reusable
    __m256i v;
    __m128i part_128;
    size_t i = 0;

    const size_t num_8pixel_blocks = num_pixels / 8; //  Process 8 pixels per iteration
    if(num_8pixel_blocks > 0)
    {
        /*
            Run the main loop for all but the last block

            Its important, since `_mm_storeu_si128()` calls stores memory by
            16 bytes (128 bits / 8 = 16 bytes). So, even we store memory in
            overlapped manner:

                                  |00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15|16 17 18 19 20 21 22 23 24 25 26 27|28 29 30 ..
            _mm_storeu_si128() -> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |
                                  +-----------------------------------------------+                                   |
                                               _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|
                                                                      +-----------------------------------------------+

            them writes total 28 bytes into `rgb` buffer on each iteration.

            It may cause write out-of-bound, if rgb buffer too small, for
            example - if rgb buffer store exactly 8 pixels --> 8 * 3 = 24 bytes,
            but we write 28 bytes into it.

            That's why we write 'not-precise' but 'faster' (due to storage by
            larger chunks of memory) only if we have anough space for it.
        */
        for(; i < (num_8pixel_blocks - 1); ++i)
        {
            #if COPY_RGBA_TO_RGB__AVX2__DO_PREFETCH == 1
            {
                // (Optional) Prefetching: Load data into (L1) cache before it's needed (improves performance)
                _mm_prefetch((const char*)(rgba + 32), _MM_HINT_T0); // 32 bytes <-- 8 RGBA pixels * 4 bytes each
                _mm_prefetch((const char*)(rgb  + 28), _MM_HINT_T0); // 28 bytes of rgb buffer, to write into
            }
            #endif

            // -----------------------------------------------------------------

            // Load (from unaligned memory) 8 RGBA pixels into AVX2 register
            //   256 bits / 8 = 32 bytes; 32 bytes / 4 bytes-in-rgba = 8 RGBA pixels
            v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba));

            // -----------------------------------------------------------------

            // Shuffle to discard the Alpha channel, keeping only RGB
            v = _mm256_shuffle_epi8(v, shuffle_mask);

            // -----------------------------------------------------------------
            // Store the extracted RGB values (28 bytes will be stored)

            part_128 = _mm256_extracti128_si256(v, 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb     ), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v, 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 12), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // -----------------------------------------------------------------

            rgba += 32; // Move forward by 8 pixels in RGBA (8 * 4 = 32)
            rgb  += 24; // Move forward by 8 pixels in RGB  (8 * 3 = 24)
        }

        /*
            Last block - precise

            More precise block is may be 'slower' (in comparison to non-precise)
            since there are more store operations, but its not write
            out-of-bounds: 8 rgb pixels stored as 24 bytes.

                                  |00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15|16 17 18 19|20 21 22 23|24 25 26 ..
            _mm_storeu_si128() -> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|           |           |
                                  +-----------------------------------------------+           |           |
                                                _mm_storeu_si64() --> |RR GG BB|RR GG BB|RR GG|           |
                                                                      +-----------------------+           |
                                                                        _mm_storeu_si32() --> |BB|RR GG BB|
                                                                                              +-----------+
        */
        {
            #if COPY_RGBA_TO_RGB__AVX2__DO_PREFETCH == 1
            {
                // (Optional) Prefetching: Load data into (L1) cache before it's needed (improves performance)
                _mm_prefetch((const char*)(rgba + 32), _MM_HINT_T0); // 32 bytes <-- 8 RGBA pixels * 4 bytes each
                _mm_prefetch((const char*)(rgb  + 24), _MM_HINT_T0); // 24 bytes of rgb buffer, to write into
            }
            #endif

            // -----------------------------------------------------------------

            // Load (from unaligned memory) 8 RGBA pixels into AVX2 register
            //   256 bits / 8 bits-in-byte = 32 bytes; 32 bytes / 4 bytes-in-rgba = 8 RGBA pixels
            v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba));

            // -----------------------------------------------------------------

            // Shuffle to discard the Alpha channel, keeping only RGB
            v = _mm256_shuffle_epi8(v, shuffle_mask);

            // -----------------------------------------------------------------
            // Store the extracted RGB values (24 bytes will be stored)

            part_128 = _mm256_extracti128_si256(v, 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v, 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si64(rgb + 12, part_128); // Store 8 bytes in (unaligned) rgb

            part_128 = _mm_srli_si128(part_128, 8); // Right-shift by 8 bytes
            _mm_storeu_si32(rgb + 20, part_128); // Store 4 bytes in (unaligned) rgb

            // -----------------------------------------------------------------

            rgba += 32; // Move forward by 8 pixels in RGBA (8 * 4 = 32)
            rgb  += 24; // Move forward by 8 pixels in RGB  (8 * 3 = 24)
        }
    }

    // Handle the remaining pixels (fallback to scalar loop)
    i = num_8pixel_blocks * 8; // Number of processed pixels
    for(; i < num_pixels; ++i)
    {
        rgb[0] = rgba[0]; // Copy R
        rgb[1] = rgba[1]; // Copy G
        rgb[2] = rgba[2]; // Copy B
        rgba += 4;
        rgb  += 3;
    }
}

void copy_rgba_to_rgb__avx2__16pixels(const uint8_t* rgba, uint8_t* rgb, size_t num_pixels)
{
    // Shuffle mask to extract RGB bytes while discarding the Alpha byte
    //
    // The shuffle mask defines how bytes are rearranged in `__m256i` (32-byte) AVX2 registers.
    // Each pixel is stored as [R,G,B,A], but we want only [R,G,B]
    const __m256i shuffle_mask = _mm256_set_epi8(
        -1, -1, -1, -1, // `-1` means 'skipped bytes'
        14,13,12,  10,9,8,  6,5,4,  2,1,0, // Extract 4 RGB from second half

        -1, -1, -1, -1, // `-1` means 'skipped bytes'
        14,13,12,  10,9,8,  6,5,4,  2,1,0  // Extract 4 RGB from first half
    );

    // Reusable
    __m256i v[2];
    __m128i part_128;
    size_t i = 0;

    const size_t num_16pixel_blocks = num_pixels / 16; //  Process 16 pixels per iteration
    if(num_16pixel_blocks > 0)
    {
        /*
            Run the main loop for all but the last block

            Its important, since `_mm_storeu_si128()` calls stores memory by
            16 bytes (128 bits / 8 = 16 bytes). So, even we store memory in
            overlapped manner:

                                  |00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15|16 17 18 19 20 21 22 23 24 25 26 27|28 29 30 31 32 33 34 35 36 37 38 39|40 41 42 43 44 45 46 47 48 49 50 51|52 53 54 ..
            _mm_storeu_si128() -> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |
                                  +-----------------------------------------------+                                   |                                   |                                   |
                                               _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |
                                                                      +-----------------------------------------------+                                   |                                   |
                                                                                   _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |
                                                                                                          +-----------------------------------------------+                                   |
                                                                                                                       _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|
                                                                                                                                              +-----------------------------------------------+

            them writes total 52 bytes into `rgb` buffer on each iteration.

            It may cause write out-of-bound, if rgb buffer too small, for
            example - if rgb buffer store exactly 16 pixels --> 16 * 3 = 48 bytes,
            but we write 52 bytes into it.

            That's why we write 'not-precise' but 'faster' (due to storage by
            larger chunks of memory) only if we have anough space for it.
        */
        for(; i < (num_16pixel_blocks - 1); ++i)
        {
            #if COPY_RGBA_TO_RGB__AVX2__DO_PREFETCH == 1
            {
                // (Optional) Prefetching: Load data into (L1) cache before it's needed (improves performance)
                _mm_prefetch((const char*)(rgba + 64), _MM_HINT_T0); // 64 bytes <-- 16 RGBA pixels * 4 bytes each
                _mm_prefetch((const char*)(rgb  + 52), _MM_HINT_T0); // 52 bytes of rgb buffer, to write into
            }
            #endif

            // -----------------------------------------------------------------

            // Load (from unaligned memory) 16 RGBA pixels into AVX2 registers
            //   256 bits / 8 = 32 bytes; 32 bytes / 4 bytes-in-rgba = 8 RGBA pixels in each
            v[0] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba     ));
            v[1] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba + 32));

            // -----------------------------------------------------------------

            // Shuffle to discard the Alpha channel, keeping only RGB
            v[0] = _mm256_shuffle_epi8(v[0], shuffle_mask);
            v[1] = _mm256_shuffle_epi8(v[1], shuffle_mask);

            // -----------------------------------------------------------------
            // Store the extracted RGB values (52 bytes will be stored)

            part_128 = _mm256_extracti128_si256(v[0], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb     ), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[0], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 12), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[1], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 24), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[1], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 36), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // -----------------------------------------------------------------

            rgba += 64; // Move forward by 16 pixels in RGBA (16 * 4 = 64)
            rgb  += 48; // Move forward by 16 pixels in RGB  (16 * 3 = 48)
        }

        /*
            Last block - precise

            More precise block is may be 'slower' (in comparison to non-precise)
            since there are more store operations, but its not write
            out-of-bounds: 16 rgb pixels stored as 48 bytes.

                                  |00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15|16 17 18 19 20 21 22 23 24 25 26 27|28 29 30 31 32 33 34 35 36 37 38 39|40 41 42 43|44 45 46 47|48 49 50 ..
            _mm_storeu_si128() -> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |           |           |
                                  +-----------------------------------------------+                                   |                                   |           |           |
                                               _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |           |           |
                                                                      +-----------------------------------------------+                                   |           |           |
                                                                                   _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|           |           |
                                                                                                          +-----------------------------------------------+           |           |
                                                                                                                        _mm_storeu_si64() --> |RR GG BB|RR GG BB|RR GG|           |
                                                                                                                                              +-----------------------+           |
                                                                                                                                                _mm_storeu_si32() --> |BB|RR GG BB|
                                                                                                                                                                      +-----------+
        */
        {
            #if COPY_RGBA_TO_RGB__AVX2__DO_PREFETCH == 1
            {
                // (Optional) Prefetching: Load data into (L1) cache before it's needed (improves performance)
                _mm_prefetch((const char*)(rgba + 64), _MM_HINT_T0); // 64 bytes <-- 16 RGBA pixels * 4 bytes each
                _mm_prefetch((const char*)(rgb  + 48), _MM_HINT_T0); // 48 bytes of rgb buffer, to write into
            }
            #endif

            // -----------------------------------------------------------------

            // Load (from unaligned memory) 16 RGBA pixels into AVX2 registers
            //   256 bits / 8 = 32 bytes; 32 bytes / 4 bytes-in-rgba = 8 RGBA pixels in each
            v[0] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba     ));
            v[1] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba + 32));

            // -----------------------------------------------------------------

            // Shuffle to discard the Alpha channel, keeping only RGB
            v[0] = _mm256_shuffle_epi8(v[0], shuffle_mask);
            v[1] = _mm256_shuffle_epi8(v[1], shuffle_mask);

            // -----------------------------------------------------------------
            // Store the extracted RGB values (48 bytes will be stored)

            part_128 = _mm256_extracti128_si256(v[0], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb     ), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[0], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 12), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[1], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 24), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[1], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si64(rgb + 36, part_128); // Store 8 bytes in (unaligned) rgb

            part_128 = _mm_srli_si128(part_128, 8); // Right-shift by 8 bytes
            _mm_storeu_si32(rgb + 44, part_128); // Store 4 bytes in (unaligned) rgb

            // -----------------------------------------------------------------

            rgba += 64; // Move forward by 16 pixels in RGBA (16 * 4 = 64)
            rgb  += 48; // Move forward by 16 pixels in RGB  (16 * 3 = 48)
        }
    }

    // Handle the remaining pixels (fallback to scalar loop)
    i = num_16pixel_blocks * 16; // Number of processed pixels
    for(; i < num_pixels; ++i)
    {
        rgb[0] = rgba[0]; // Copy R
        rgb[1] = rgba[1]; // Copy G
        rgb[2] = rgba[2]; // Copy B
        rgba += 4;
        rgb  += 3;
    }
}

void copy_rgba_to_rgb__avx2__32pixels(const uint8_t* rgba, uint8_t* rgb, size_t num_pixels)
{
    // Shuffle mask to extract RGB bytes while discarding the Alpha byte
    //
    // The shuffle mask defines how bytes are rearranged in `__m256i` (32-byte) AVX2 registers.
    // Each pixel is stored as [R,G,B,A], but we want only [R,G,B]
    const __m256i shuffle_mask = _mm256_set_epi8(
        -1, -1, -1, -1, // `-1` means 'skipped bytes'
        14,13,12,  10,9,8,  6,5,4,  2,1,0, // Extract 4 RGB from second half

        -1, -1, -1, -1, // `-1` means 'skipped bytes'
        14,13,12,  10,9,8,  6,5,4,  2,1,0  // Extract 4 RGB from first half
    );

    // Reusable
    __m256i v[4];
    __m128i part_128;
    size_t i = 0;

    const size_t num_32pixel_blocks = num_pixels / 32; //  Process 32 pixels per iteration
    if(num_32pixel_blocks > 0)
    {
        /*
            Run the main loop for all but the last block

            Its important, since `_mm_storeu_si128()` calls stores memory by
            16 bytes (128 bits / 8 = 16 bytes). So, even we store memory in
            overlapped manner:

                                  |00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15|16 17 18 19 20 21 22 23 24 25 26 27|28 29 30 31 32 33 34 35 36 37 38 39|40 41 42 43 44 45 46 47 48 49 50 51|52 53 54 55 56 57 58 59 60 61 62 63|64 65 66 67 68 69 70 71 72 73 74 75|76 77 78 79 80 81 82 83 84 85 86 87|88 89 90 91 92 93 94 95 96 97 98 99|00 01 02 ..
            _mm_storeu_si128() -> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |                                   |
                                  +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |                                   |
                                               _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |
                                                                      +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |
                                                                                   _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |
                                                                                                          +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |
                                                                                                                       _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |
                                                                                                                                              +-----------------------------------------------+                                   |                                   |                                   |                                   |
                                                                                                                                                            _mm_storeu_si128() -> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |
                                                                                                                                                                                  +-----------------------------------------------+                                   |                                   |                                   |
                                                                                                                                                                                               _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |
                                                                                                                                                                                                                      +-----------------------------------------------+                                   |                                   |
                                                                                                                                                                                                                                   _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |
                                                                                                                                                                                                                                                          +-----------------------------------------------+                                   |
                                                                                                                                                                                                                                                                       _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|
                                                                                                                                                                                                                                                                                              +-----------------------------------------------+

            them writes total 100 bytes into `rgb` buffer on each iteration.

            It may cause write out-of-bound, if rgb buffer too small, for
            example - if rgb buffer store exactly 32 pixels --> 32 * 3 = 96 bytes,
            but we write 100 bytes into it.

            That's why we write 'not-precise' but 'faster' (due to storage by
            larger chunks of memory) only if we have anough space for it.
        */
        for(; i < (num_32pixel_blocks - 1); ++i)
        {
            #if COPY_RGBA_TO_RGB__AVX2__DO_PREFETCH == 1
            {
                // (Optional) Prefetching: Load data into (L1) cache before it's needed (improves performance)
                _mm_prefetch((const char*)(rgba + 128), _MM_HINT_T0); // 128 bytes <-- 32 RGBA pixels * 4 bytes each
                _mm_prefetch((const char*)(rgb  + 100), _MM_HINT_T0); // 100 bytes of rgb buffer, to write into
            }
            #endif

            // -----------------------------------------------------------------

            // Load (from unaligned memory) 32 RGBA pixels into AVX2 registers
            //   256 bits / 8 = 32 bytes; 32 bytes / 4 bytes-in-rgba = 8 RGBA pixels in each
            v[0] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba     ));
            v[1] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba + 32));
            v[2] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba + 64));
            v[3] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba + 96));

            // -----------------------------------------------------------------

            // Shuffle to discard the Alpha channel, keeping only RGB
            v[0] = _mm256_shuffle_epi8(v[0], shuffle_mask);
            v[1] = _mm256_shuffle_epi8(v[1], shuffle_mask);
            v[2] = _mm256_shuffle_epi8(v[2], shuffle_mask);
            v[3] = _mm256_shuffle_epi8(v[3], shuffle_mask);

            // -----------------------------------------------------------------
            // Store the extracted RGB values (100 bytes will be stored)

            part_128 = _mm256_extracti128_si256(v[0], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb     ), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[0], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 12), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[1], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 24), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[1], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 36), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[2], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 48), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[2], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 60), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[3], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 72), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[3], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 84), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // -----------------------------------------------------------------

            rgba += 128; // Move forward by 32 pixels in RGBA (32 * 4 = 128)
            rgb  +=  96; // Move forward by 32 pixels in RGB  (32 * 3 =  96)
        }

        /*
            Last block - precise

            More precise block is may be 'slower' (in comparison to non-precise)
            since there are more store operations, but its not write
            out-of-bounds: 32 rgb pixels stored as 96 bytes.

                                  |00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15|16 17 18 19 20 21 22 23 24 25 26 27|28 29 30 31 32 33 34 35 36 37 38 39|40 41 42 43 44 45 46 47 48 49 50 51|52 53 54 55 56 57 58 59 60 61 62 63|64 65 66 67 68 69 70 71 72 73 74 75|76 77 78 79 80 81 82 83 84 85 86 87|88 89 90 91 92 93 94 95|96 97 98 ..
            _mm_storeu_si128() -> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |                       |
                                  +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |                       |
                                               _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                       |
                                                                      +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                       |
                                                                                   _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                       |
                                                                                                          +-----------------------------------------------+                                   |                                   |                                   |                                   |                       |
                                                                                                                       _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                       |
                                                                                                                                              +-----------------------------------------------+                                   |                                   |                                   |                       |
                                                                                                                                                            _mm_storeu_si128() -> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                       |
                                                                                                                                                                                  +-----------------------------------------------+                                   |                                   |                       |
                                                                                                                                                                                               _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                       |
                                                                                                                                                                                                                      +-----------------------------------------------+                                   |                       |
                                                                                                                                                                                                                                   _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                       |
                                                                                                                                                                                                                                                          +-----------------------------------------------+                       |
                                                                                                                                                                                                                                                                        _mm_storeu_si64() --> |RR GG BB|RR GG BB|RR GG|           |
                                                                                                                                                                                                                                                                                              +-----------------------+           |
                                                                                                                                                                                                                                                                                                _mm_storeu_si32() --> |BB|RR GG BB|
                                                                                                                                                                                                                                                                                                                      +-----------+
        */
        {
            #if COPY_RGBA_TO_RGB__AVX2__DO_PREFETCH == 1
            {
                // (Optional) Prefetching: Load data into (L1) cache before it's needed (improves performance)
                _mm_prefetch((const char*)(rgba + 128), _MM_HINT_T0); // 128 bytes <-- 32 RGBA pixels * 4 bytes each
                _mm_prefetch((const char*)(rgb  +  96), _MM_HINT_T0); //  96 bytes of rgb buffer, to write into
            }
            #endif

            // -----------------------------------------------------------------

            // Load (from unaligned memory) 32 RGBA pixels into AVX2 registers
            //   256 bits / 8 = 32 bytes; 32 bytes / 4 bytes-in-rgba = 8 RGBA pixels in each
            v[0] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba     ));
            v[1] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba + 32));
            v[2] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba + 64));
            v[3] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba + 96));

            // -----------------------------------------------------------------

            // Shuffle to discard the Alpha channel, keeping only RGB
            v[0] = _mm256_shuffle_epi8(v[0], shuffle_mask);
            v[1] = _mm256_shuffle_epi8(v[1], shuffle_mask);
            v[2] = _mm256_shuffle_epi8(v[2], shuffle_mask);
            v[3] = _mm256_shuffle_epi8(v[3], shuffle_mask);

            // -----------------------------------------------------------------
            // Store the extracted RGB values (96 bytes will be stored)

            part_128 = _mm256_extracti128_si256(v[0], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb     ), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[0], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 12), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[1], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 24), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[1], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 36), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[2], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 48), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[2], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 60), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[3], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 72), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[3], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si64(rgb + 84, part_128); // Store 8 bytes in (unaligned) rgb

            part_128 = _mm_srli_si128(part_128, 8); // Right-shift by 8 bytes
            _mm_storeu_si32(rgb + 92, part_128); // Store 4 bytes in (unaligned) rgb

            // -----------------------------------------------------------------

            rgba += 128; // Move forward by 32 pixels in RGBA (32 * 4 = 128)
            rgb  +=  96; // Move forward by 32 pixels in RGB  (32 * 3 =  96)
        }
    }

    // Handle the remaining pixels (fallback to scalar loop)
    i = num_32pixel_blocks * 32; // Number of processed pixels
    for(; i < num_pixels; ++i)
    {
        rgb[0] = rgba[0]; // Copy R
        rgb[1] = rgba[1]; // Copy G
        rgb[2] = rgba[2]; // Copy B
        rgba += 4;
        rgb  += 3;
    }
}

void copy_rgba_to_rgb__avx2__64pixels(const uint8_t* rgba, uint8_t* rgb, size_t num_pixels)
{
    // Shuffle mask to extract RGB bytes while discarding the Alpha byte
    //
    // The shuffle mask defines how bytes are rearranged in `__m256i` (32-byte) AVX2 registers.
    // Each pixel is stored as [R,G,B,A], but we want only [R,G,B]
    const __m256i shuffle_mask = _mm256_set_epi8(
        -1, -1, -1, -1, // `-1` means 'skipped bytes'
        14,13,12,  10,9,8,  6,5,4,  2,1,0, // Extract 4 RGB from second half

        -1, -1, -1, -1, // `-1` means 'skipped bytes'
        14,13,12,  10,9,8,  6,5,4,  2,1,0  // Extract 4 RGB from first half
    );

    // Reusable
    __m256i v[8];
    __m128i part_128;
    size_t i = 0;

    const size_t num_64pixel_blocks = num_pixels / 64; //  Process 64 pixels per iteration
    if(num_64pixel_blocks > 0)
    {
        /*
            Run the main loop for all but the last block

            Its important, since `_mm_storeu_si128()` calls stores memory by
            16 bytes (128 bits / 8 = 16 bytes). So, even we store memory in
            overlapped manner:

                                  |00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15|16 17 18 19 20 21 22 23 24 25 26 27|28 29 30 31 32 33 34 35 36 37 38 39|40 41 42 43 44 45 46 47 48 49 50 51|52 53 54 55 56 57 58 59 60 61 62 63|64 65 66 67 68 69 70 71 72 73 74 75|76 77 78 79 80 81 82 83 84 85 86 87|88 89 90 91 92 93 94 95 96 97 98 99|00 01 02 03 04 05 06 07 08 09 10 11|12 13 14 15 16 17 18 19 20 21 22 23|24 25 26 27 28 29 30 31 32 33 34 35|36 37 38 39 40 41 42 43 44 45 46 47|48 49 50 51 52 53 54 55 56 57 58 59|60 61 62 63 64 65 66 67 68 69 70 71|72 73 74 75 76 77 78 79 80 81 82 83|84 85 86 87 88 89 90 91 92 93 94 95|96 97 98 ..
            _mm_storeu_si128() -> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |
                                  +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |
                                               _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |
                                                                      +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |
                                                                                   _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |
                                                                                                          +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |
                                                                                                                       _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |
                                                                                                                                              +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |
                                                                                                                                                            _mm_storeu_si128() -> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |
                                                                                                                                                                                  +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |
                                                                                                                                                                                               _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |
                                                                                                                                                                                                                      +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |
                                                                                                                                                                                                                                   _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |
                                                                                                                                                                                                                                                          +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |
                                                                                                                                                                                                                                                                       _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |
                                                                                                                                                                                                                                                                                              +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |
                                                                                                                                                                                                                                                                                                            _mm_storeu_si128() -> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |                                   |
                                                                                                                                                                                                                                                                                                                                  +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |                                   |
                                                                                                                                                                                                                                                                                                                                               _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |
                                                                                                                                                                                                                                                                                                                                                                      +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |
                                                                                                                                                                                                                                                                                                                                                                                   _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |
                                                                                                                                                                                                                                                                                                                                                                                                          +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |
                                                                                                                                                                                                                                                                                                                                                                                                                       _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |
                                                                                                                                                                                                                                                                                                                                                                                                                                              +-----------------------------------------------+                                   |                                   |                                   |                                   |
                                                                                                                                                                                                                                                                                                                                                                                                                                                            _mm_storeu_si128() -> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  +-----------------------------------------------+                                   |                                   |                                   |
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      +-----------------------------------------------+                                   |                                   |
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          +-----------------------------------------------+                                   |
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              +-----------------------------------------------+

            them writes total 196 bytes into `rgb` buffer on each iteration.

            It may cause write out-of-bound, if rgb buffer too small, for
            example - if rgb buffer store exactly 64 pixels --> 64 * 3 = 192 bytes,
            but we write 196 bytes into it.

            That's why we write 'not-precise' but 'faster' (due to storage by
            larger chunks of memory) only if we have anough space for it.
        */
        for(; i < (num_64pixel_blocks - 1); ++i)
        {
            #if COPY_RGBA_TO_RGB__AVX2__DO_PREFETCH == 1
            {
                // (Optional) Prefetching: Load data into (L1) cache before it's needed (improves performance)
                _mm_prefetch((const char*)(rgba + 256), _MM_HINT_T0); // 256 bytes <-- 64 RGBA pixels * 4 bytes each
                _mm_prefetch((const char*)(rgb  + 196), _MM_HINT_T0); // 196 bytes of rgb buffer, to write into
            }
            #endif

            // -----------------------------------------------------------------

            // Load (from unaligned memory) 64 RGBA pixels into AVX2 registers
            //   256 bits / 8 = 32 bytes; 32 bytes / 4 bytes-in-rgba = 8 RGBA pixels in each
            v[0] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba     ));
            v[1] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba +  32));
            v[2] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba +  64));
            v[3] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba +  96));
            v[4] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba + 128));
            v[5] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba + 160));
            v[6] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba + 192));
            v[7] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba + 224));

            // -----------------------------------------------------------------

            // Shuffle to discard the Alpha channel, keeping only RGB
            v[0] = _mm256_shuffle_epi8(v[0], shuffle_mask);
            v[1] = _mm256_shuffle_epi8(v[1], shuffle_mask);
            v[2] = _mm256_shuffle_epi8(v[2], shuffle_mask);
            v[3] = _mm256_shuffle_epi8(v[3], shuffle_mask);
            v[4] = _mm256_shuffle_epi8(v[4], shuffle_mask);
            v[5] = _mm256_shuffle_epi8(v[5], shuffle_mask);
            v[6] = _mm256_shuffle_epi8(v[6], shuffle_mask);
            v[7] = _mm256_shuffle_epi8(v[7], shuffle_mask);

            // -----------------------------------------------------------------
            // Store the extracted RGB values (196 bytes will be stored)

            part_128 = _mm256_extracti128_si256(v[0], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb      ), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[0], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb +  12), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[1], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb +  24), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[1], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb +  36), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[2], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb +  48), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[2], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb +  60), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[3], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb +  72), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[3], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb +  84), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[4], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb +  96), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[4], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 108), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[5], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 120), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[5], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 132), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[6], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 144), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[6], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 156), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[7], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 168), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[7], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 180), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // -----------------------------------------------------------------

            rgba += 256; // Move forward by 64 pixels in RGBA (64 * 4 = 256)
            rgb  += 192; // Move forward by 64 pixels in RGB  (64 * 3 = 192)
        }

        /*
            Last block - precise

            More precise block is may be 'slower' (in comparison to non-precise)
            since there are more store operations, but its not write
            out-of-bounds: 64 rgb pixels stored as 192 bytes.

                                  |00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15|16 17 18 19 20 21 22 23 24 25 26 27|28 29 30 31 32 33 34 35 36 37 38 39|40 41 42 43 44 45 46 47 48 49 50 51|52 53 54 55 56 57 58 59 60 61 62 63|64 65 66 67 68 69 70 71 72 73 74 75|76 77 78 79 80 81 82 83 84 85 86 87|88 89 90 91 92 93 94 95 96 97 98 99|00 01 02 03 04 05 06 07 08 09 10 11|12 13 14 15 16 17 18 19 20 21 22 23|24 25 26 27 28 29 30 31 32 33 34 35|36 37 38 39 40 41 42 43 44 45 46 47|48 49 50 51 52 53 54 55 56 57 58 59|60 61 62 63 64 65 66 67 68 69 70 71|72 73 74 75 76 77 78 79 80 81 82 83|84 85 86 87|88 89 90 91|92 93 94 ..
            _mm_storeu_si128() -> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |           |           |
                                  +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |           |           |
                                               _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |           |           |
                                                                      +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |           |           |
                                                                                   _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |           |           |
                                                                                                          +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |           |           |
                                                                                                                       _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |           |           |
                                                                                                                                              +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |           |           |
                                                                                                                                                            _mm_storeu_si128() -> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |           |           |
                                                                                                                                                                                  +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |           |           |
                                                                                                                                                                                               _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |           |           |
                                                                                                                                                                                                                      +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |           |           |
                                                                                                                                                                                                                                   _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |           |           |
                                                                                                                                                                                                                                                          +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |                                   |                                   |           |           |
                                                                                                                                                                                                                                                                       _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |                                   |           |           |
                                                                                                                                                                                                                                                                                              +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |                                   |           |           |
                                                                                                                                                                                                                                                                                                            _mm_storeu_si128() -> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |                                   |           |           |
                                                                                                                                                                                                                                                                                                                                  +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |                                   |           |           |
                                                                                                                                                                                                                                                                                                                                               _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |                                   |           |           |
                                                                                                                                                                                                                                                                                                                                                                      +-----------------------------------------------+                                   |                                   |                                   |                                   |                                   |           |           |
                                                                                                                                                                                                                                                                                                                                                                                   _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |                                   |           |           |
                                                                                                                                                                                                                                                                                                                                                                                                          +-----------------------------------------------+                                   |                                   |                                   |                                   |           |           |
                                                                                                                                                                                                                                                                                                                                                                                                                       _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |                                   |           |           |
                                                                                                                                                                                                                                                                                                                                                                                                                                              +-----------------------------------------------+                                   |                                   |                                   |           |           |
                                                                                                                                                                                                                                                                                                                                                                                                                                                            _mm_storeu_si128() -> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |                                   |           |           |
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  +-----------------------------------------------+                                   |                                   |           |           |
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|                                   |           |           |
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      +-----------------------------------------------+                                   |           |           |
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   _mm_storeu_si128() --> |RR GG BB|RR GG BB|RR GG BB|RR GG BB|xx xx xx xx|           |           |
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          +-----------------------------------------------+           |           |
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        _mm_storeu_si64() --> |RR GG BB|RR GG BB|RR GG|           |
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              +-----------------------+           |
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                _mm_storeu_si32() --> |BB|RR GG BB|
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      +-----------+
        */
        {
            #if COPY_RGBA_TO_RGB__AVX2__DO_PREFETCH == 1
            {
                // (Optional) Prefetching: Load data into (L1) cache before it's needed (improves performance)
                _mm_prefetch((const char*)(rgba + 256), _MM_HINT_T0); // 256 bytes <-- 64 RGBA pixels * 4 bytes each
                _mm_prefetch((const char*)(rgb  + 192), _MM_HINT_T0); // 192 bytes of rgb buffer, to write into
            }
            #endif

            // -----------------------------------------------------------------

            // Load (from unaligned memory) 64 RGBA pixels into AVX2 registers
            //   256 bits / 8 = 32 bytes; 32 bytes / 4 bytes-in-rgba = 8 RGBA pixels in each
            v[0] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba     ));
            v[1] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba +  32));
            v[2] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba +  64));
            v[3] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba +  96));
            v[4] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba + 128));
            v[5] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba + 160));
            v[6] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba + 192));
            v[7] = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rgba + 224));

            // -----------------------------------------------------------------

            // Shuffle to discard the Alpha channel, keeping only RGB
            v[0] = _mm256_shuffle_epi8(v[0], shuffle_mask);
            v[1] = _mm256_shuffle_epi8(v[1], shuffle_mask);
            v[2] = _mm256_shuffle_epi8(v[2], shuffle_mask);
            v[3] = _mm256_shuffle_epi8(v[3], shuffle_mask);
            v[4] = _mm256_shuffle_epi8(v[4], shuffle_mask);
            v[5] = _mm256_shuffle_epi8(v[5], shuffle_mask);
            v[6] = _mm256_shuffle_epi8(v[6], shuffle_mask);
            v[7] = _mm256_shuffle_epi8(v[7], shuffle_mask);

            // -----------------------------------------------------------------
            // Store the extracted RGB values (192 bytes will be stored)

            part_128 = _mm256_extracti128_si256(v[0], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb      ), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[0], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb +  12), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[1], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb +  24), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[1], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb +  36), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[2], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb +  48), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[2], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb +  60), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[3], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb +  72), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[3], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb +  84), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[4], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb +  96), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[4], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 108), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[5], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 120), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[5], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 132), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[6], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 144), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[6], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 156), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[7], 0); // Extract low  128-bit part from the 256-bit register
            _mm_storeu_si128(reinterpret_cast<__m128i*>(rgb + 168), part_128); // Store 16 bytes in (unaligned) rgb (useful - first 12 bytes, 4 RGB pixels)

            // - - - - -

            part_128 = _mm256_extracti128_si256(v[7], 1); // Extract high 128-bit part from the 256-bit register
            _mm_storeu_si64(rgb + 180, part_128); // Store 8 bytes in (unaligned) rgb

            part_128 = _mm_srli_si128(part_128, 8); // Right-shift by 8 bytes
            _mm_storeu_si32(rgb + 188, part_128); // Store 4 bytes in (unaligned) rgb

            // -----------------------------------------------------------------

            rgba += 256; // Move forward by 64 pixels in RGBA (64 * 4 = 256)
            rgb  += 192; // Move forward by 64 pixels in RGB  (64 * 3 = 192)
        }
    }

    // Handle the remaining pixels (fallback to scalar loop)
    i = num_64pixel_blocks * 64; // Number of processed pixels
    for(; i < num_pixels; ++i)
    {
        rgb[0] = rgba[0]; // Copy R
        rgb[1] = rgba[1]; // Copy G
        rgb[2] = rgba[2]; // Copy B
        rgba += 4;
        rgb  += 3;
    }
}

#endif // defined(__AVX2__)

// -----------------------------------------------------------------------------

int main()
{
    print_lscpu();

    // Validation
    if(1)
    {
        using test_func_t = void (*) (const uint8_t*, uint8_t*, size_t);
        using test_name_and_func_t = std::pair< const char*, test_func_t >;
        const std::vector< test_name_and_func_t > registry
        {
              test_name_and_func_t{"memcpy (1 pixel)",       copy_rgba_to_rgb__memcpy}
            , test_name_and_func_t{"raw_pointers (1 pixel)", copy_rgba_to_rgb__raw_ptr}
            , test_name_and_func_t{"raw_pointers (4 pixel)", copy_rgba_to_rgb__raw_ptr__4pixels}

            #if defined(__AVX2__)
            , test_name_and_func_t{"avx2 (8 pixels)",  copy_rgba_to_rgb__avx2__8pixels}
            , test_name_and_func_t{"avx2 (16 pixels)", copy_rgba_to_rgb__avx2__16pixels}
            , test_name_and_func_t{"avx2 (32 pixels)", copy_rgba_to_rgb__avx2__32pixels}
            , test_name_and_func_t{"avx2 (64 pixels)", copy_rgba_to_rgb__avx2__64pixels}
            #endif
        };

        std::vector<size_t> num_pixels_cases;
        for(size_t i = 0; i <= 512; ++i)
        {
            num_pixels_cases.push_back(i);
        }
        num_pixels_cases.push_back(800 * 600);
        num_pixels_cases.push_back(1920 * 1080);

        for(size_t i = 0; i < num_pixels_cases.size(); ++i)
        {
            const size_t num_pixels = num_pixels_cases[i];
            fprintf(stdout, "%zu. Validation case: %zu pixels\n", i, num_pixels);
            fflush(stdout);

            for(const test_name_and_func_t& t : registry)
            {
                const char*        name = t.first;
                const test_func_t& func = t.second;

                const std::vector<uint8_t> rgba = make_ascending_data(num_pixels * 4);
                std::vector<uint8_t> rgb(num_pixels * 3, 0);

                func(rgba.data(), rgb.data(), num_pixels);

                if( compare_rgba_to_rgb(rgba.data(), rgb.data(), num_pixels) == false )
                {
                    fprintf(stdout, "%s failed for %zu pixels\n", name, num_pixels);
                    fflush(stdout);
                }
            }
        };
    }

    // Benchmarking
    if(1)
    {
        static constexpr size_t WIDTH  = 1920;
        static constexpr size_t HEIGHT = 1080;
        static constexpr size_t NUM_PIXELS = WIDTH * HEIGHT;
        static constexpr size_t NUM_ITERATIONS = 5000;

        fprintf(stdout, "\nIterations count: %zu\n", NUM_ITERATIONS);
        fflush(stdout);

        std::vector<uint8_t> rgba(NUM_PIXELS * 4, 255); // Input  RGBA buffer
        std::vector<uint8_t> rgb (NUM_PIXELS * 3,   0); // Output RGB  buffer

        ankerl::nanobench::Bench b;
        b.title("RGBA to RGB");
        b.warmup(100); // iters
        b.relative(true);
        b.performanceCounters(true);
        b.minEpochIterations(NUM_ITERATIONS);

        // ---------------------------------------------------------------------

        b.run("memcpy (1 pixel)", [&]() {
            copy_rgba_to_rgb__memcpy(rgba.data(), rgb.data(), NUM_PIXELS);
        });

        b.run("raw_pointers (1 pixel)", [&]() {
            copy_rgba_to_rgb__raw_ptr(rgba.data(), rgb.data(), NUM_PIXELS);
        });

        b.run("raw_pointers (4 pixels)", [&]() {
            copy_rgba_to_rgb__raw_ptr__4pixels(rgba.data(), rgb.data(), NUM_PIXELS);
        });

        #if defined(__AVX2__)
            b.run("avx2 (8 pixels)", [&]() {
                copy_rgba_to_rgb__avx2__8pixels(rgba.data(), rgb.data(), NUM_PIXELS);
            });

            b.run("avx2 (16 pixels)", [&]() {
                copy_rgba_to_rgb__avx2__16pixels(rgba.data(), rgb.data(), NUM_PIXELS);
            });

            b.run("avx2 (32 pixels)", [&]() {
                copy_rgba_to_rgb__avx2__32pixels(rgba.data(), rgb.data(), NUM_PIXELS);
            });

            b.run("avx2 (64 pixels)", [&]() {
                copy_rgba_to_rgb__avx2__64pixels(rgba.data(), rgb.data(), NUM_PIXELS);
            });
        #endif // defined(__AVX2__)
    }

    return 0;
}
