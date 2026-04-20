#include <filesystem>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>

#include <png.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "CL/cl.h"
static const char *
GetOpenCLErrorString(int error)
{
    switch (error)
    {
    // run-time and JIT compiler errors
    case 0:
        return "CL_SUCCESS";
    case -1:
        return "CL_DEVICE_NOT_FOUND";
    case -2:
        return "CL_DEVICE_NOT_AVAILABLE";
    case -3:
        return "CL_COMPILER_NOT_AVAILABLE";
    case -4:
        return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
    case -5:
        return "CL_OUT_OF_RESOURCES";
    case -6:
        return "CL_OUT_OF_HOST_MEMORY";
    case -7:
        return "CL_PROFILING_INFO_NOT_AVAILABLE";
    case -8:
        return "CL_MEM_COPY_OVERLAP";
    case -9:
        return "CL_IMAGE_FORMAT_MISMATCH";
    case -10:
        return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
    case -11:
        return "CL_BUILD_PROGRAM_FAILURE";
    case -12:
        return "CL_MAP_FAILURE";
    case -13:
        return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
    case -14:
        return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
    case -15:
        return "CL_COMPILE_PROGRAM_FAILURE";
    case -16:
        return "CL_LINKER_NOT_AVAILABLE";
    case -17:
        return "CL_LINK_PROGRAM_FAILURE";
    case -18:
        return "CL_DEVICE_PARTITION_FAILED";
    case -19:
        return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";

    // compile-time errors
    case -30:
        return "CL_INVALID_VALUE";
    case -31:
        return "CL_INVALID_DEVICE_TYPE";
    case -32:
        return "CL_INVALID_PLATFORM";
    case -33:
        return "CL_INVALID_DEVICE";
    case -34:
        return "CL_INVALID_CONTEXT";
    case -35:
        return "CL_INVALID_QUEUE_PROPERTIES";
    case -36:
        return "CL_INVALID_COMMAND_QUEUE";
    case -37:
        return "CL_INVALID_HOST_PTR";
    case -38:
        return "CL_INVALID_MEM_OBJECT";
    case -39:
        return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
    case -40:
        return "CL_INVALID_IMAGE_SIZE";
    case -41:
        return "CL_INVALID_SAMPLER";
    case -42:
        return "CL_INVALID_BINARY";
    case -43:
        return "CL_INVALID_BUILD_OPTIONS";
    case -44:
        return "CL_INVALID_PROGRAM";
    case -45:
        return "CL_INVALID_PROGRAM_EXECUTABLE";
    case -46:
        return "CL_INVALID_KERNEL_NAME";
    case -47:
        return "CL_INVALID_KERNEL_DEFINITION";
    case -48:
        return "CL_INVALID_KERNEL";
    case -49:
        return "CL_INVALID_ARG_INDEX";
    case -50:
        return "CL_INVALID_ARG_VALUE";
    case -51:
        return "CL_INVALID_ARG_SIZE";
    case -52:
        return "CL_INVALID_KERNEL_ARGS";
    case -53:
        return "CL_INVALID_WORK_DIMENSION";
    case -54:
        return "CL_INVALID_WORK_GROUP_SIZE";
    case -55:
        return "CL_INVALID_WORK_ITEM_SIZE";
    case -56:
        return "CL_INVALID_GLOBAL_OFFSET";
    case -57:
        return "CL_INVALID_EVENT_WAIT_LIST";
    case -58:
        return "CL_INVALID_EVENT";
    case -59:
        return "CL_INVALID_OPERATION";
    case -60:
        return "CL_INVALID_GL_OBJECT";
    case -61:
        return "CL_INVALID_BUFFER_SIZE";
    case -62:
        return "CL_INVALID_MIP_LEVEL";
    case -63:
        return "CL_INVALID_GLOBAL_WORK_SIZE";
    case -64:
        return "CL_INVALID_PROPERTY";
    case -65:
        return "CL_INVALID_IMAGE_DESCRIPTOR";
    case -66:
        return "CL_INVALID_COMPILER_OPTIONS";
    case -67:
        return "CL_INVALID_LINKER_OPTIONS";
    case -68:
        return "CL_INVALID_DEVICE_PARTITION_COUNT";

    // extension errors
    case -1000:
        return "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR";
    case -1001:
        return "CL_PLATFORM_NOT_FOUND_KHR";
    case -1002:
        return "CL_INVALID_D3D10_DEVICE_KHR";
    case -1003:
        return "CL_INVALID_D3D10_RESOURCE_KHR";
    case -1004:
        return "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR";
    case -1005:
        return "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR";

    default:
        return "Unknown OpenCL error";
    }
}
// OpenCL error checking macro
#define CL_CHECK(err) if (err != CL_SUCCESS) { \
    std::cerr << "OpenCL error: " << GetOpenCLErrorString(err) << std::endl; exit(1); }

namespace fs = std::filesystem;

const bool DEBUG_MESSAGES = false;

bool isImageFile(const fs::path& path) {
    std::string ext = path.extension().string();
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".pgm";
}

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void debugPrintBuffer(cl_command_queue queue, cl_mem buffer, int size, const std::string& label) {
    std::vector<uint32_t> data(size);
    clEnqueueReadBuffer(queue, buffer, CL_TRUE, 0, size * sizeof(uint32_t), data.data(), 0, NULL, NULL);
    std::cout << "[DEBUG] " << label << ":\n";
    for (int i = 0; i < size; i++) {
        std::cout << i << ":" << data[i] << " ";
    }
    std::cout << "\n";
}

enum class BitDepth : uint8_t { U8 = 8, U16 = 16 };

struct ImageDescriptor {
    uint32_t width;
    uint32_t height;
    BitDepth bitDepth;
    bool     isGrayscale;  // if true, only planes[0] (Y) is valid
};

struct RawImageData {
    // For 8b images
    std::vector<uint8_t>  data8;
    // For 16b images
    std::vector<uint16_t> data16;
};

struct ImageBuffers {
    ImageDescriptor desc;
    cl_mem planes[3];  // [0]=Y, [1]=Cb, [2]=Cr
    uint32_t   planeCount; // 1 for grayscale, 3 for YCbCr
};

// HELPER: Allocate memory for a single image plane
cl_mem allocPlane(
    cl_context  context,
    size_t      width,
    size_t      height,
    BitDepth    bitDepth,
    cl_int*     err)
{
    size_t elementSize = (bitDepth == BitDepth::U16) ? sizeof(uint16_t) : sizeof(uint8_t);
    size_t bufferSize  = width * height * elementSize;

    cl_int localErr;
    cl_mem mem = clCreateBuffer(context, CL_MEM_READ_WRITE, bufferSize, nullptr, &localErr);

    if (localErr != CL_SUCCESS) {
        if (err) *err = localErr;
        return nullptr;
    }

    if (err) *err = CL_SUCCESS;
    return mem;
}

// HELPER: Free image buffers
void freeImageBuffers(ImageBuffers& buffers) {
    for (uint32_t i = 0; i < buffers.planeCount; i++) {
        if (buffers.planes[i]) {
            clReleaseMemObject(buffers.planes[i]);
            buffers.planes[i] = nullptr;
        }
    }
    buffers.planeCount = 0;
}

static cl_ulong eventDurationUs(const std::vector<cl_event>& events) {
    cl_ulong total = 0;
    for (cl_event e : events) {
        cl_ulong start, end;
        clGetEventProfilingInfo(e, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, nullptr);
        clGetEventProfilingInfo(e, CL_PROFILING_COMMAND_END,   sizeof(cl_ulong), &end,   nullptr);
        total += (end - start) / 1000;
    }
    return total;
}

struct OpenCLResources {
    std::string output_postfix;
    std::string program_source;
    char * histogram_kernel;
    char * scan_kernel;
    char * norm_scale_kernel;
    char * output_kernel;
    bool chunked_histogram = false;
};

OpenCLResources NAIVE_IMPLEMENTATION = OpenCLResources{
    .output_postfix = "_naive_equalized.png",
    .program_source = "naive.cl",
    .histogram_kernel = (char *)"Histogram",
    .scan_kernel = (char *)"Scan",
    .norm_scale_kernel = (char *)"NormaliseAndScale",
    .output_kernel = (char *)"Backproject",
    .chunked_histogram = false
};

OpenCLResources OPTIMIZED_IMPLEMENTATION = OpenCLResources{
    .output_postfix = "_optimized_equalized.png",
    .program_source = "optimized.cl",
    .histogram_kernel = (char *)"Histogram",
    .scan_kernel = (char *)"Scan",
    .norm_scale_kernel = (char *)"NormaliseAndScale",
    .output_kernel = (char *)"Backproject",
    .chunked_histogram = true
};

struct CSVDataRow {
    std::string buildID;
    std::string imageName;
    std::string implementation;
    ImageDescriptor imageDescriptor;
    
    // Kernel timings
    cl_ulong histogramTimeUs;
    cl_ulong scanTimeUs;
    cl_ulong normScaleTimeUs;

    // Transfer timings
    cl_ulong hostToDeviceTimeUs;
    cl_ulong deviceToHostTimeUs;
    size_t   hostToDeviceBytes;
    size_t   deviceToHostBytes;
};

static void writeCSV(const CSVDataRow& row) {
    const std::string csvPath = "results.csv";
    bool needsHeader = !fs::exists(csvPath);
 
    std::ofstream f(csvPath, std::ios::app);
    if (!f.is_open()) {
        std::cerr << "Failed to open " << csvPath << " for writing\n";
        return;
    }
 
    if (needsHeader) {
        f << "build_id,image_name,implementation,"
          << "width,height,bit_depth,is_grayscale,"
          << "histogram_us,scan_us,norm_scale_us,"
          << "h2d_us,h2d_bytes,d2h_us,d2h_bytes\n";
    }
 
    f << row.buildID                                           << ','
      << row.imageName                                       << ','
      << row.implementation                                  << ','
      << row.imageDescriptor.width                          << ','
      << row.imageDescriptor.height                         << ','
      << static_cast<int>(row.imageDescriptor.bitDepth)     << ','
      << (row.imageDescriptor.isGrayscale ? "true" : "false") << ','
      << row.histogramTimeUs                                 << ','
      << row.scanTimeUs                                      << ','
      << row.normScaleTimeUs                                 << ','
      << row.hostToDeviceTimeUs                              << ','
      << row.hostToDeviceBytes                               << ','
      << row.deviceToHostTimeUs                              << ','
      << row.deviceToHostBytes                               << '\n';
}

void write_png_16bit(const std::string& path, uint16_t* data, int width, int height, int channels) {
    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) { std::cerr << "Failed to open file for writing: " << path << "\n"; return; }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info  = png_create_info_struct(png);

    if (setjmp(png_jmpbuf(png))) {
        std::cerr << "PNG write error\n";
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return;
    }

    png_init_io(png, fp);

    int color_type = (channels == 1) ? PNG_COLOR_TYPE_GRAY : PNG_COLOR_TYPE_RGB;
    png_set_IHDR(png, info, width, height, 16, color_type,
        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    png_set_swap(png); // x86 is little-endian, PNG expects big-endian

    std::vector<uint16_t*> rows(height);
    for (int y = 0; y < height; y++)
        rows[y] = data + y * width * channels;

    png_write_image(png, (png_bytepp)rows.data());
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    fclose(fp);
}

CSVDataRow processOpenCL(
    OpenCLResources& resources, 
    ImageDescriptor& imageDescriptor, 
    RawImageData& rawData,
    cl_context context, 
    cl_command_queue queue, 
    cl_device_id device, 
    const std::string& path
) {
    std::cout << "Processing OpenCL for image: " << path << std::endl;
    cl_int err;
    
    std::string kernelSrc = readFile(resources.program_source);
    if (kernelSrc.empty()) {
        std::cerr << "Failed to read kernel source: " << resources.program_source << "\n";
        return CSVDataRow{};
    }
    const char* src = kernelSrc.c_str();

    cl_program program = clCreateProgramWithSource(context, 1, &src, NULL, &err);
    CL_CHECK(err);

    const char* build_opts = (imageDescriptor.bitDepth == BitDepth::U16)
        ? "-cl-fast-relaxed-math -cl-mad-enable -DDEPTH_16"
        : "-cl-fast-relaxed-math -cl-mad-enable -DDEPTH_8";

    err = clBuildProgram(program, 1, &device, build_opts, NULL, NULL);
    if (err != CL_SUCCESS) {
        size_t log_size;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);

        std::vector<char> log(log_size);
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, log.data(), NULL);

        std::cerr << "Build failed:\n" << log.data() << std::endl;
        return CSVDataRow{};
    }

    uint32_t numBuckets   = (imageDescriptor.bitDepth == BitDepth::U16) ? 65536 : 256;
    uint32_t maxIntensity = (imageDescriptor.bitDepth == BitDepth::U16) ? 65535 : 255;
    cl_uint pixelCount   = imageDescriptor.width * imageDescriptor.height;

    // Allocate image planes
    ImageBuffers imageBuffers = {};
    imageBuffers.desc = imageDescriptor;
    imageBuffers.planeCount = imageDescriptor.isGrayscale ? 1 : 3;

    for (uint32_t i = 0; i < imageBuffers.planeCount; i++) {
        imageBuffers.planes[i] = allocPlane(context, imageBuffers.desc.width, imageBuffers.desc.height, imageBuffers.desc.bitDepth, &err);
        CL_CHECK(err);
    }

    // Host to Device transfers
    std::vector<cl_event> writeEvents;
    size_t h2dBytes = 0;
 
    auto enqueueWrite = [&](cl_mem buf, const void* ptr, size_t bytes) {
        cl_event ev;
        CL_CHECK(clEnqueueWriteBuffer(queue, buf, CL_FALSE, 0, bytes, ptr, 0, NULL, &ev));
        writeEvents.push_back(ev);
        h2dBytes += bytes;
    };

    if (imageBuffers.desc.bitDepth == BitDepth::U8) {
        if (imageBuffers.desc.isGrayscale) {
            enqueueWrite(imageBuffers.planes[0], rawData.data8.data(), pixelCount * sizeof(uint8_t));
        } else {
            std::vector<uint8_t> planeY(pixelCount), planeCb(pixelCount), planeCr(pixelCount);
            for (size_t i = 0; i < pixelCount; i++) {
                float r = rawData.data8[i * 3 + 0];
                float g = rawData.data8[i * 3 + 1];
                float b = rawData.data8[i * 3 + 2];
                
                planeY [i] = (uint8_t)std::clamp( 0.299f*r + 0.587f*g + 0.114f*b,           0.f, 255.f);
                planeCb[i] = (uint8_t)std::clamp(-0.168736f*r - 0.331264f*g + 0.5f*b + 128.f, 0.f, 255.f);
                planeCr[i] = (uint8_t)std::clamp( 0.5f*r - 0.418688f*g - 0.081312f*b + 128.f, 0.f, 255.f);
            }

            enqueueWrite(imageBuffers.planes[0], planeY.data(),  pixelCount * sizeof(uint8_t));
            enqueueWrite(imageBuffers.planes[1], planeCb.data(), pixelCount * sizeof(uint8_t));
            enqueueWrite(imageBuffers.planes[2], planeCr.data(), pixelCount * sizeof(uint8_t));
        }
    } else {
        if (imageBuffers.desc.isGrayscale) {
            enqueueWrite(imageBuffers.planes[0], rawData.data16.data(), pixelCount * sizeof(uint16_t));
        } else {
            std::vector<uint16_t> planeY(pixelCount), planeCb(pixelCount), planeCr(pixelCount);
            for (size_t i = 0; i < pixelCount; i++) {
                float r = rawData.data16[i * 3 + 0];
                float g = rawData.data16[i * 3 + 1];
                float b = rawData.data16[i * 3 + 2];
                
                planeY [i] = (uint16_t)std::clamp( 0.299f*r + 0.587f*g + 0.114f*b,               0.f, 65535.f);
                planeCb[i] = (uint16_t)std::clamp(-0.168736f*r - 0.331264f*g + 0.5f*b + 32768.f,  0.f, 65535.f);
                planeCr[i] = (uint16_t)std::clamp( 0.5f*r - 0.418688f*g - 0.081312f*b + 32768.f,  0.f, 65535.f);
            }

            enqueueWrite(imageBuffers.planes[0], planeY.data(),  pixelCount * sizeof(uint16_t));
            enqueueWrite(imageBuffers.planes[1], planeCb.data(), pixelCount * sizeof(uint16_t));
            enqueueWrite(imageBuffers.planes[2], planeCr.data(), pixelCount * sizeof(uint16_t));
        }
    }
    // Wait for all writes to complete before kernels run
    clWaitForEvents((cl_uint)writeEvents.size(), writeEvents.data());
 


    // Load all kenels
    cl_kernel histogram_kernel = clCreateKernel(program, resources.histogram_kernel, &err);
    CL_CHECK(err);
    cl_kernel scan_kernel = clCreateKernel(program, resources.scan_kernel, &err);
    CL_CHECK(err);
    cl_kernel norm_scale_kernel = clCreateKernel(program, resources.norm_scale_kernel, &err);
    CL_CHECK(err);
    cl_kernel output_kernel = clCreateKernel(program, resources.output_kernel, &err);
    CL_CHECK(err);
    cl_kernel ycbcr_to_rgb_kernel = clCreateKernel(program, "YCbCr_to_RGB", &err);
    CL_CHECK(err);

    // Create needed buffers
    cl_mem histogram_buffer = clCreateBuffer(context, CL_MEM_READ_WRITE, numBuckets * sizeof(uint32_t), NULL, &err);
    CL_CHECK(err);
    cl_mem cumulative_hist_buffer = clCreateBuffer(context, CL_MEM_READ_WRITE, numBuckets * sizeof(uint32_t), NULL, &err);
    CL_CHECK(err);
    cl_mem lut_buffer = clCreateBuffer(context, CL_MEM_READ_WRITE, numBuckets * sizeof(uint32_t), NULL, &err);
    CL_CHECK(err);

    size_t elementSize = (imageBuffers.desc.bitDepth == BitDepth::U16) ? sizeof(uint16_t) : sizeof(uint8_t);
    size_t outputSizeGray = pixelCount * elementSize;
    size_t outputSize = (imageBuffers.desc.isGrayscale) ? outputSizeGray : outputSizeGray * 3;

    cl_mem equalized_y_buffer = clCreateBuffer(context, CL_MEM_READ_WRITE, outputSize, NULL, &err);
    CL_CHECK(err);
    cl_mem output_buffer = clCreateBuffer(context, CL_MEM_WRITE_ONLY, outputSize, NULL, &err);
    CL_CHECK(err);
    


    // Set kernel arguments
    // # HISTOGRAM
    clSetKernelArg(histogram_kernel, 0, sizeof(cl_mem), &imageBuffers.planes[0]);
    clSetKernelArg(histogram_kernel, 1, sizeof(cl_mem), &histogram_buffer);
    clSetKernelArg(histogram_kernel, 2, sizeof(cl_uint), &pixelCount);
    clSetKernelArg(histogram_kernel, 3, sizeof(uint), &numBuckets);
    // # SCAN
    clSetKernelArg(scan_kernel, 0, sizeof(cl_mem), &histogram_buffer);
    clSetKernelArg(scan_kernel, 1, sizeof(cl_mem), &cumulative_hist_buffer);
    clSetKernelArg(scan_kernel, 2, sizeof(uint), &numBuckets);
    // # NORMALISE_AND_SCALE
    clSetKernelArg(norm_scale_kernel, 0, sizeof(cl_mem), &cumulative_hist_buffer);
    clSetKernelArg(norm_scale_kernel, 1, sizeof(cl_mem), &lut_buffer);
    clSetKernelArg(norm_scale_kernel, 2, sizeof(cl_uint), &pixelCount);
    clSetKernelArg(norm_scale_kernel, 3, sizeof(uint), &numBuckets);
    clSetKernelArg(norm_scale_kernel, 4, sizeof(uint), &maxIntensity);
    // # BACKPROJECT
    clSetKernelArg(output_kernel, 0, sizeof(cl_mem), &imageBuffers.planes[0]);
    clSetKernelArg(output_kernel, 1, sizeof(cl_mem), &lut_buffer);
    clSetKernelArg(output_kernel, 2, sizeof(cl_mem), &equalized_y_buffer);
    // # YCbCr_to_RGB
    clSetKernelArg(ycbcr_to_rgb_kernel, 0, sizeof(cl_mem), &equalized_y_buffer);
    clSetKernelArg(ycbcr_to_rgb_kernel, 1, sizeof(cl_mem), &imageBuffers.planes[1]);
    clSetKernelArg(ycbcr_to_rgb_kernel, 2, sizeof(cl_mem), &imageBuffers.planes[2]);
    clSetKernelArg(ycbcr_to_rgb_kernel, 3, sizeof(cl_mem), &output_buffer);
    clSetKernelArg(ycbcr_to_rgb_kernel, 4, sizeof(cl_uint), &pixelCount);

    //
    // Histogram kernel, optimizations
    //
    cl_ulong device_local_mem;
    clGetDeviceInfo(device, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong), &device_local_mem, NULL);

    size_t kernel_local_usage;
    clGetKernelWorkGroupInfo(histogram_kernel, device, CL_KERNEL_LOCAL_MEM_SIZE, sizeof(size_t), &kernel_local_usage, NULL);

    size_t preferred_multiple;
    clGetKernelWorkGroupInfo(histogram_kernel, device, CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, sizeof(size_t), &preferred_multiple, NULL);

    size_t max_work_group_size;
    clGetKernelWorkGroupInfo(histogram_kernel, device, CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &max_work_group_size, NULL);

    size_t available = device_local_mem - kernel_local_usage;
    size_t max_bins  = available / sizeof(cl_uint);

    // 8-bit: ideal is 256 local (1:1 with bins), cap at device limits
    size_t local_8bit = 256;
    if (local_8bit > max_work_group_size) {
        // round down to largest power-of-2 multiple of preferred_multiple
        local_8bit = preferred_multiple;
        while (local_8bit * 2 <= max_work_group_size) local_8bit *= 2;
    }

    // 16-bit: chunk size = largest power-of-2 that fits in local mem, capped at 65536
    size_t chunk_16bit = 1;
    while (chunk_16bit * 2 <= max_bins && chunk_16bit * 2 <= 65536){
        chunk_16bit *= 2;
    }
    
    size_t local_16bit = 256; // same local size, chunks handle the bin range
    chunk_16bit = (chunk_16bit / local_16bit) * local_16bit;
    uint   num_chunks  = (65536 + chunk_16bit - 1) / chunk_16bit;

    //
    // Enqueue kernels
    //
    CSVDataRow dataRow;


    uint   numBucketsActual = (imageBuffers.desc.bitDepth == BitDepth::U8) ? 256 : 65536;
    size_t local  = (imageBuffers.desc.bitDepth == BitDepth::U8) ? local_8bit : local_16bit;
    size_t global = ((pixelCount + local - 1) / local) * local;

    std::cout << "Global Work Size: " << global << ", Local Work Size: " << local << "\n";
    std::cout << "local_size: " << device_local_mem << " max workgroup: " << max_work_group_size << "\n";

    cl_uint zero = 0;
    clEnqueueFillBuffer(queue, histogram_buffer, &zero, sizeof(cl_uint), 0, 
    numBuckets * sizeof(cl_uint), 0, NULL, NULL);
    clFinish(queue);

    cl_event histogram_event, scan_event, norm_event, out_event;

    cl_uint chunk_16bit_u = (cl_uint)chunk_16bit;
    std::cout << "Chunk Size (16-bit): " << chunk_16bit << "\n";
    bool histogram_timed = false;
    if (imageBuffers.desc.bitDepth == BitDepth::U8) {
        CL_CHECK(clEnqueueNDRangeKernel(queue, histogram_kernel, 1, NULL,
            &global, &local, 0, NULL, &histogram_event));
        clWaitForEvents(1, &histogram_event);

    } else if (resources.chunked_histogram) {
        // 16-bit optimized
        cl_uint bin_offset_zero = 0;
        clSetKernelArg(histogram_kernel, 4, sizeof(cl_uint), &chunk_16bit_u);
        clSetKernelArg(histogram_kernel, 5, sizeof(cl_uint), &bin_offset_zero);
        clSetKernelArg(histogram_kernel, 6, chunk_16bit * sizeof(cl_uint), NULL);

        cl_uint zero = 0;
        clEnqueueFillBuffer(queue, histogram_buffer, &zero, sizeof(cl_uint), 0,
            numBuckets * sizeof(cl_uint), 0, NULL, NULL);
        clFinish(queue);

        std::vector<cl_event> chunk_events(num_chunks);
        for (uint chunk = 0; chunk < num_chunks; chunk++) {
            cl_uint bin_offset = chunk * chunk_16bit_u;
            clSetKernelArg(histogram_kernel, 5, sizeof(cl_uint), &bin_offset);
            CL_CHECK(clEnqueueNDRangeKernel(queue, histogram_kernel, 1, NULL,
                &global, &local, 0, NULL, &chunk_events[chunk]));
        }
        clWaitForEvents(num_chunks, chunk_events.data());
        
        // but sum all chunks for accurate total time:
        dataRow.histogramTimeUs = eventDurationUs(chunk_events);
        histogram_timed = true;
        for (cl_event e : chunk_events) clReleaseEvent(e);

    } else {
        // 16-bit naive
        CL_CHECK(clEnqueueNDRangeKernel(queue, histogram_kernel, 1, NULL,
            &global, &local, 0, NULL, &histogram_event));
        clWaitForEvents(1, &histogram_event);
    }

    size_t scan_global = 1, scan_local  = 1;
    CL_CHECK(clEnqueueNDRangeKernel(queue, scan_kernel, 1, NULL, &scan_global, &scan_local, 0, NULL, &scan_event));
    clWaitForEvents(1, &scan_event);
    if (DEBUG_MESSAGES) debugPrintBuffer(queue, cumulative_hist_buffer, numBuckets, "Scan");

    size_t norm_global = numBuckets, norm_local  = 256;
    CL_CHECK(clEnqueueNDRangeKernel(queue, norm_scale_kernel, 1, NULL, &norm_global, &norm_local, 0, NULL, &norm_event));
    clWaitForEvents(1, &norm_event);
    if (DEBUG_MESSAGES) debugPrintBuffer(queue, lut_buffer, numBuckets, "NormalizeAndScan");

    CL_CHECK(clEnqueueNDRangeKernel(queue, output_kernel, 1, NULL, &global, &local, 0, NULL, &out_event));
    clWaitForEvents(1, &out_event);

    if (!imageBuffers.desc.isGrayscale) {// For color images, convert back to RGB
        cl_event ycbcr_event;
        CL_CHECK(clEnqueueNDRangeKernel(queue, ycbcr_to_rgb_kernel, 1, NULL, &global, &local, 0, NULL, &ycbcr_event));
        clWaitForEvents(1, &ycbcr_event);
        clReleaseEvent(ycbcr_event);
    }

    // Device to Host
    cl_mem read_buffer = imageBuffers.desc.isGrayscale ? equalized_y_buffer : output_buffer;

    std::vector<uint16_t> output16;
    std::vector<uint8_t>  output8;

    void* outputPtr;
    if (imageBuffers.desc.bitDepth == BitDepth::U16) {
        output16.resize(outputSize / sizeof(uint16_t));
        outputPtr = output16.data();
    } else {
        output8.resize(outputSize);
        outputPtr = output8.data();
    }
    cl_event read_event;
    CL_CHECK(clEnqueueReadBuffer(queue, read_buffer, CL_FALSE, 0, outputSize, outputPtr, 0, NULL, &read_event));
    clWaitForEvents(1, &read_event);

    // Write to file
    std::string out_path = path + resources.output_postfix;
    int stride_multiplier = imageBuffers.desc.isGrayscale ? 1 : 3;
    
    if (imageBuffers.desc.bitDepth == BitDepth::U16) {
        write_png_16bit(out_path, output16.data(), imageBuffers.desc.width, imageBuffers.desc.height, stride_multiplier);
    } else {
        stbi_write_png(out_path.c_str(), imageBuffers.desc.width, imageBuffers.desc.height,
            stride_multiplier, output8.data(), imageBuffers.desc.width * stride_multiplier);
    }

    dataRow.imageName       = fs::path(path).filename().string();
    dataRow.implementation  = resources.program_source;
    dataRow.imageDescriptor = imageBuffers.desc;

    if (!histogram_timed) { 
        dataRow.histogramTimeUs  = eventDurationUs({histogram_event}); 
    }
    dataRow.scanTimeUs       = eventDurationUs({scan_event});
    dataRow.normScaleTimeUs  = eventDurationUs({norm_event});
 
    dataRow.hostToDeviceTimeUs = eventDurationUs(writeEvents);
    dataRow.hostToDeviceBytes  = h2dBytes;
 
    dataRow.deviceToHostTimeUs = eventDurationUs({read_event});
    dataRow.deviceToHostBytes  = outputSize;

    for (cl_event e : writeEvents) clReleaseEvent(e);
    if (!histogram_timed) clReleaseEvent(histogram_event); // guarded
    clReleaseEvent(scan_event);
    clReleaseEvent(norm_event);
    clReleaseEvent(out_event);
    clReleaseEvent(read_event);
 
    clReleaseKernel(histogram_kernel);
    clReleaseKernel(scan_kernel);
    clReleaseKernel(norm_scale_kernel);
    clReleaseKernel(output_kernel);
    clReleaseKernel(ycbcr_to_rgb_kernel);
 
    clReleaseMemObject(histogram_buffer);
    clReleaseMemObject(cumulative_hist_buffer);
    clReleaseMemObject(lut_buffer);
    clReleaseMemObject(equalized_y_buffer);
    clReleaseMemObject(output_buffer);
    freeImageBuffers(imageBuffers);
 
    clReleaseProgram(program);
 
    return dataRow;
}

// Supported image configurations:
// Channels: 1, 3
// Bit Depth: 8, 16
int processImage(const std::string& path, const std::string& buildID) {
    std::cout << "Processing image: " << path << "\n";
    
    int width, height, channels;
    if (!stbi_info(path.c_str(), &width, &height, &channels)) {
        std::cerr << "Failed to get image info\n";
        return 1;
    }
    if (channels != 1 && channels != 3) {
        std::cerr << "Unsupported channel count: " << channels << "\n";
        return 1;
    }
 
    ImageDescriptor desc = {};
    desc.width       = (uint32_t)width;
    desc.height      = (uint32_t)height;
    desc.isGrayscale = (channels == 1);
    desc.bitDepth    = stbi_is_16_bit(path.c_str()) ? BitDepth::U16 : BitDepth::U8;
 
    size_t pixelCount = (size_t)width * height;
 
    // Load raw pixels into host memory
    RawImageData rawData;
    if (desc.bitDepth == BitDepth::U8) {
        unsigned char* img = stbi_load(path.c_str(), &width, &height, &channels, 0);
        if (!img) { 
            std::cerr << "Failed to load image\n"; 
            return 1; 
        }
        rawData.data8.assign(img, img + pixelCount * channels);
        stbi_image_free(img);
        std::cout << "Loaded " << (desc.isGrayscale ? "grayscale" : "color") << " 8-bit image: " << width << "x" << height << "\n";
    } else {
        unsigned short* img = (unsigned short*)stbi_load_16(path.c_str(), &width, &height, &channels, 0);
        if (!img) { 
            std::cerr << "Failed to load 16-bit image\n"; 
            return 1; 
        }
        rawData.data16.assign(img, img + pixelCount * channels);
        stbi_image_free(img);
        std::cout << "Loaded " << (desc.isGrayscale ? "grayscale" : "color") << " 16-bit image: " << width << "x" << height << "\n";
        std::cout << "First pixel RGB: " 
          << rawData.data16[0] << " "
          << rawData.data16[1] << " "
          << rawData.data16[2] << "\n";
    }

    // OpenCL setup
    cl_int err;
    cl_platform_id platform;
    CL_CHECK(clGetPlatformIDs(1, &platform, NULL));
    cl_device_id device;
    CL_CHECK(clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 1, &device, NULL));
 
    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    CL_CHECK(err);
 
    cl_queue_properties props[] = { CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0 };
    cl_command_queue queue = clCreateCommandQueueWithProperties(context, device, props, &err);
    CL_CHECK(err);

    OpenCLResources allResources[] = { NAIVE_IMPLEMENTATION, OPTIMIZED_IMPLEMENTATION };

    for (OpenCLResources& resource : allResources) {
        CSVDataRow row = processOpenCL(resource, desc, rawData, context, queue, device, path);
        row.buildID = buildID;
        writeCSV(row);
        std::cout << "[CSV] Wrote row for " << row.imageName << " (" << row.implementation << ")\n";
    }
 
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
    return 0;
}

// BuildID is just a unix timestamp, for the purposes of this assignment its good enough
static std::string makeBuildID() {
    auto now = std::chrono::system_clock::now();
    return std::to_string(
        std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count());
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: histeq <image_path_or_directory>\n";
        return 1;
    }

    fs::path input = argv[1];
    if (!fs::exists(input)) {
        std::cerr << "Path does not exist: " << input << std::endl;
        return 1;
    }

    std::vector<std::string> imagePaths;

    if (fs::is_regular_file(input)) {
        if (!isImageFile(input)) {
            std::cerr << "Not supported file format!\n";
            return 1;
        }

        imagePaths.push_back(input.string());
    } else if (fs::is_directory(input)) {
        for (const auto& entry : fs::directory_iterator(input)) {
            if (entry.is_regular_file() && isImageFile(entry.path())) {
                imagePaths.push_back(entry.path().string());
            }
        }
        if (imagePaths.empty()) {
            std::cerr << "No supported images found in directory\n";
            return 1;
        }
    } else {
        std::cerr << "Input is neither file or directory!\n";
        return 1;
    }

    std::string buildID = makeBuildID();

    for (const auto& path : imagePaths) {
        processImage(path, buildID);
    }

    return 0;
}

