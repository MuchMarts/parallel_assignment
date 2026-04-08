__kernel void HistogramNaive(
    __global const uchar *data,
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

__kernel void ScanNaive(
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
    __global const uchar *data,
    __global const uint *lut,
    __global       uchar *output_image
) {
    int id = get_global_id(0);
    output_image[id] = lut[data[id]];   
}
