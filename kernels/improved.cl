#ifdef DEPTH_16
    typedef ushort pixel_t;
    #define MAX_INTENSITY 65535u
    #define NUM_BUCKETS   65536u
    #define CHROMA_OFFSET 32768
#else
    typedef uchar  pixel_t;
    #define MAX_INTENSITY 255u
    #define NUM_BUCKETS   256u
    #define CHROMA_OFFSET 128
#endif

__kernel void Histogram(
    __global const pixel_t *data,
    __global       uint *histogram,
             const uint  data_length,
             const uint  num_buckets
) {
    uint i = get_global_id(0);
    if (i < data_length) {
        uint value = data[i];
        if (value < num_buckets) {
            atomic_inc(&histogram[value]);
        }
    }
}

__kernel void Scan(
    __global const uint *histogram,
    __global       uint *cumulative,
    const uint num_buckets
) {
    if (get_global_id(0) == 0) {
        uint sum = 0;
        for (uint i = 0; i < num_buckets; i++) {
            sum += histogram[i];
            cumulative[i] = sum;
        }
    }
}

__kernel void NormaliseAndScale(
    __global const uint *cumulative,
    __global       uint *lut,
    const uint total_pixels,
    const uint num_buckets,
    const uint max_intensity
) {
    if (get_global_id(0) == 0) {
        for (uint i = 0; i < num_buckets; i++) {
            lut[i] = (cumulative[i] * max_intensity) / total_pixels;
        }
    }
}

__kernel void Backproject(
    __global const pixel_t *data,
    __global const uint *lut,
    __global       pixel_t *output_image
) {
    int id = get_global_id(0);
    output_image[id] = (pixel_t)lut[data[id]];   
}

__kernel void YCbCr_to_RGB(
    __global const pixel_t *planeY,
    __global const pixel_t *planeCb,
    __global const pixel_t *planeCr,
    __global       pixel_t *output_rgb,
    const uint pixel_count
) {
    int id = get_global_id(0);
    if (id >= pixel_count) return;

    float Y  = (float)planeY[id];
    float Cb = (float)planeCb[id] - (float)CHROMA_OFFSET;
    float Cr = (float)planeCr[id] - (float)CHROMA_OFFSET;

    pixel_t R = (pixel_t)clamp(Y + 1.402f   * Cr,                        0.0f, (float)MAX_INTENSITY);
    pixel_t G = (pixel_t)clamp(Y - 0.344136f * Cb - 0.714136f * Cr,      0.0f, (float)MAX_INTENSITY);
    pixel_t B = (pixel_t)clamp(Y + 1.772f   * Cb,                        0.0f, (float)MAX_INTENSITY);

    output_rgb[id * 3 + 0] = R;
    output_rgb[id * 3 + 1] = G;
    output_rgb[id * 3 + 2] = B;
}