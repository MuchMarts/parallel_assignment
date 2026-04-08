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
    // only one thread does all the work
    if (get_global_id(0) == 0) {
        uint sum = 0;
        for (uint i = 0; i < num_buckets; i++) {
            sum += histogram[i];
            cumulative[i] = sum;
        }
    }
}