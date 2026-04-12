#include <filesystem>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>

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

bool isImageFile(const fs::path& path) {
    std::string ext = path.extension().string();
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp";
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

struct OpenCLResources {
    std::string output_postfix;
    std::string program_source;
    char * histogram_kernel;
    char * scan_kernel;
    char * norm_scale_kernel;
    char * output_kernel;
};

OpenCLResources NAIVE_IMPLEMENTATION = OpenCLResources{
    .output_postfix = "_naive_equalized.png",
    .program_source = "naive.cl",
    .histogram_kernel = (char *)"Histogram",
    .scan_kernel = (char *)"Scan",
    .norm_scale_kernel = (char *)"NormaliseAndScale",
    .output_kernel = (char *)"Backproject"
};

int processOpenCL(OpenCLResources& resources, ImageBuffers& imageBuffers, cl_context context, cl_command_queue queue, cl_device_id device, const std::string& path) {
    std::cout << "Processing OpenCL for image: " << path << std::endl;
    cl_int err;
    
    std::string kernelSrc = readFile(resources.program_source);
    const char* src = kernelSrc.c_str();

    cl_program program = clCreateProgramWithSource(context, 1, &src, NULL, &err);
    CL_CHECK(err);

    const char* build_opts = imageBuffers.desc.bitDepth == BitDepth::U16
    ? "-cl-fast-relaxed-math -cl-mad-enable -DDEPTH_16"
    : "-cl-fast-relaxed-math -cl-mad-enable -DDEPTH_8";

    err = clBuildProgram(program, 1, &device, build_opts, NULL, NULL);

    if (err != CL_SUCCESS) {
        size_t log_size;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);

        std::vector<char> log(log_size);
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, log.data(), NULL);

        std::cerr << "Build failed:\n" << log.data() << std::endl;
        return 1;
    }

    uint32_t numBuckets   = (imageBuffers.desc.bitDepth == BitDepth::U16) ? 65536 : 256;
    uint32_t maxIntensity = (imageBuffers.desc.bitDepth == BitDepth::U16) ? 65535 : 255;
    size_t   pixelCount   = imageBuffers.desc.width * imageBuffers.desc.height;

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
    cl_mem histogram_buffer = clCreateBuffer(
        context,
        CL_MEM_READ_WRITE,
        numBuckets * sizeof(uint32_t),
        NULL,
        &err);
    CL_CHECK(err);

    cl_mem cumulative_hist_buffer = clCreateBuffer(
        context,
        CL_MEM_READ_WRITE,
        numBuckets * sizeof(uint32_t),
        NULL,
        &err);
    CL_CHECK(err);

    cl_mem lut_buffer = clCreateBuffer(
        context,
        CL_MEM_READ_WRITE,
        numBuckets * sizeof(uint32_t),
        NULL,
        &err);
    CL_CHECK(err);

    size_t elementSize = (imageBuffers.desc.bitDepth == BitDepth::U16) ? sizeof(uint16_t) : sizeof(uint8_t);
    size_t outputSize = imageBuffers.desc.isGrayscale
    ? pixelCount * elementSize
    : pixelCount * elementSize * 3;

    cl_mem equalized_y_buffer = clCreateBuffer(
        context, 
        CL_MEM_READ_WRITE, 
        outputSize,
        NULL, 
        &err);
    CL_CHECK(err);

    cl_mem output_buffer = clCreateBuffer(
        context,
        CL_MEM_WRITE_ONLY,
        outputSize,
        NULL,
        &err);
    CL_CHECK(err);
    


    // Set kernel arguments
    // # HISTOGRAM
    clSetKernelArg(histogram_kernel, 0, sizeof(cl_mem), &imageBuffers.planes[0]);
    clSetKernelArg(histogram_kernel, 1, sizeof(cl_mem), &histogram_buffer);
    clSetKernelArg(histogram_kernel, 2, sizeof(uint), &pixelCount);
    clSetKernelArg(histogram_kernel, 3, sizeof(uint), &numBuckets);
    // # SCAN
    clSetKernelArg(scan_kernel, 0, sizeof(cl_mem), &histogram_buffer);
    clSetKernelArg(scan_kernel, 1, sizeof(cl_mem), &cumulative_hist_buffer);
    clSetKernelArg(scan_kernel, 2, sizeof(uint), &numBuckets);
    // # NORMALISE_AND_SCALE
    clSetKernelArg(norm_scale_kernel, 0, sizeof(cl_mem), &cumulative_hist_buffer);
    clSetKernelArg(norm_scale_kernel, 1, sizeof(cl_mem), &lut_buffer);
    clSetKernelArg(norm_scale_kernel, 2, sizeof(uint), &pixelCount);
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
    clSetKernelArg(ycbcr_to_rgb_kernel, 4, sizeof(uint), &pixelCount);

    // Enqueue kernels
    
    // Launch histogram kernel
    size_t local  = 64;
    size_t global = ((pixelCount + local - 1) / local) * local;
    cl_event histogram_event;
    CL_CHECK(clEnqueueNDRangeKernel(queue, histogram_kernel, 1, NULL, &global, &local, 0, NULL, &histogram_event));
    clWaitForEvents(1, &histogram_event);
    //debugPrintBuffer(queue, histogram_buffer, numBuckets, "Histogram");

    // Launch scan kernel
    size_t scan_global = 1;
    size_t scan_local  = 1;
    cl_event scan_event;
    CL_CHECK(clEnqueueNDRangeKernel(queue, scan_kernel, 1, NULL, &scan_global, &scan_local, 0, NULL, &scan_event));
    clWaitForEvents(1, &scan_event);
    //debugPrintBuffer(queue, cumulative_hist_buffer, numBuckets, "Scan");

    // Launch normalize and scale kernel
    size_t norm_global = 1;
    size_t norm_local  = 1;
    cl_event norm_event;
    CL_CHECK(clEnqueueNDRangeKernel(queue, norm_scale_kernel, 1, NULL, &norm_global, &norm_local, 0, NULL, &norm_event));
    clWaitForEvents(1, &norm_event);
    //debugPrintBuffer(queue, lut_buffer, numBuckets, "NormalizeAndScan");

    // Launch Backproject kernel
    size_t out_global = ((pixelCount + local - 1) / local) * local;
    size_t out_local  = 64;
    cl_event out_event;

    CL_CHECK(clEnqueueNDRangeKernel(queue, output_kernel, 1, NULL, &out_global, &out_local, 0, NULL, &out_event));
    clWaitForEvents(1, &out_event);

    // For color images, convert back to RGB
    if (!imageBuffers.desc.isGrayscale) {
        cl_event ycbcr_event;
        CL_CHECK(clEnqueueNDRangeKernel(queue, ycbcr_to_rgb_kernel, 1, NULL, &out_global, &out_local, 0, NULL, &ycbcr_event));
        clWaitForEvents(1, &ycbcr_event);
    }

    cl_mem read_buffer = imageBuffers.desc.isGrayscale ? equalized_y_buffer : output_buffer;
    // OUTPUT
    std::vector<unsigned char> output(outputSize);
    CL_CHECK(clEnqueueReadBuffer(
        queue,
        read_buffer,
        CL_TRUE,
        0,
        outputSize,
        output.data(),
        0, NULL, NULL));

    // write to file
    std::string out_path = path + resources.output_postfix;
    int stride_multiplier = imageBuffers.desc.isGrayscale ? 1 : 3;
    int write_ok = stbi_write_png(
        out_path.c_str(),
        imageBuffers.desc.width,
        imageBuffers.desc.height,
        stride_multiplier,           
        output.data(),
        imageBuffers.desc.width * stride_multiplier
    );

    if (!write_ok) {
        std::cerr << "Failed to write output image\n";
    }
    std::cout << "Saved: " << out_path << "\n";

    // --- Timing ---
    std::cout << resources.program_source << "\n";

    // histogram
    cl_ulong h_start, h_end;
    clGetEventProfilingInfo(histogram_event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &h_start, NULL);
    clGetEventProfilingInfo(histogram_event, CL_PROFILING_COMMAND_END,   sizeof(cl_ulong), &h_end, NULL);
    std::cout << "[GPU] Histogram kernel: " << (h_end - h_start) / 1000 << " us\n";

    // scan
    cl_ulong s_start, s_end;
    clGetEventProfilingInfo(scan_event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &s_start, NULL);
    clGetEventProfilingInfo(scan_event, CL_PROFILING_COMMAND_END,   sizeof(cl_ulong), &s_end, NULL);
    std::cout << "[GPU] Scan kernel:      " << (s_end - s_start) / 1000 << " us\n";
    
    // scan
    cl_ulong n_start, n_end;
    clGetEventProfilingInfo(norm_event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &n_start, NULL);
    clGetEventProfilingInfo(norm_event, CL_PROFILING_COMMAND_END,   sizeof(cl_ulong), &n_end, NULL);
    std::cout << "[GPU] NormalizeAndScan kernel:      " << (n_end - n_start) / 1000 << " us\n";


    clReleaseKernel(histogram_kernel);
    clReleaseKernel(scan_kernel);
    clReleaseKernel(norm_scale_kernel);
    clReleaseKernel(output_kernel);
    clReleaseKernel(ycbcr_to_rgb_kernel);


    clReleaseMemObject(histogram_buffer);
    clReleaseMemObject(cumulative_hist_buffer);
    clReleaseMemObject(lut_buffer);
    clReleaseMemObject(output_buffer);
    clReleaseMemObject(equalized_y_buffer);
    return 0;

}


// Supported image configurations:
// Channels: 1, 3
// Bit Depth: 8, 16
int processImage(const std::string& path) {
    std::cout << "Processing image" << path << std::endl;
    
    // Initialize structs
    int width, height, channels;
    ImageDescriptor imageDesc = {};
    ImageBuffers imageBuffers = {};
    imageBuffers.desc = imageDesc;

    int ok = stbi_info(path.c_str(), &width, &height, &channels);
    if (!ok) {
        std::cerr << "Failed to get image info\n";
        return 1;
    }

    imageBuffers.desc.width = width;
    imageBuffers.desc.height = height;
    imageBuffers.desc.isGrayscale = (channels == 1);
    imageBuffers.desc.bitDepth = stbi_is_16_bit(path.c_str()) ? BitDepth::U16 : BitDepth::U8;

    if (channels != 3 && channels != 1) {
        std::cerr << "Unsupported number of channels: " << channels << "\n";
        return 1;
    }
    imageBuffers.planeCount = channels;
    size_t PIXEL_COUNT = width * height;

    // OpenCL setup
    cl_int err;

    cl_platform_id platform;
    CL_CHECK(clGetPlatformIDs(1, &platform, NULL));

    cl_device_id device;
    CL_CHECK(clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 1, &device, NULL));

    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    CL_CHECK(err);

    cl_queue_properties props[] = {
        CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE,
        0
    };
    cl_command_queue queue = clCreateCommandQueueWithProperties(
        context, device, props, &err);
    CL_CHECK(err);

    // Load image data and create buffers
    if (imageBuffers.desc.bitDepth == BitDepth::U8){
        unsigned char* image_8b = stbi_load(path.c_str(), &width, &height, &channels, 0);
        if (!image_8b) {
            std::cerr << "Failed to load image\n";
            return 1;
        }

        if (imageBuffers.desc.isGrayscale) {
            std::cout << "Loaded grayscale image: w=" << width << " h=" << height << " c=" << channels << "\n";
            // Allocate memory for the grayscale plane
            imageBuffers.planes[0] = allocPlane(context, width, height, imageBuffers.desc.bitDepth, &err);
            CL_CHECK(err);
            err = clEnqueueWriteBuffer(queue, imageBuffers.planes[0], CL_TRUE, 0, PIXEL_COUNT * sizeof(uint8_t), image_8b, 0, NULL, NULL);
            CL_CHECK(err);
        } else {
            std::cout << "Loaded color image: w=" << width << " h=" << height << " c=" << channels << "\n";
            // Convert to YCbCr
            std::vector<uint8_t> planeY (PIXEL_COUNT);
            std::vector<uint8_t> planeCb(PIXEL_COUNT);
            std::vector<uint8_t> planeCr(PIXEL_COUNT);

            for (size_t i = 0; i < PIXEL_COUNT; i++) {
                float r = (float)image_8b[i * 3 + 0];
                float g = (float)image_8b[i * 3 + 1];
                float b = (float)image_8b[i * 3 + 2];

                // Convert RGB to YCbCr
                planeY [i] = (uint8_t)std::clamp((0.299f * r + 0.587f * g + 0.114f * b), 0.0f, 255.0f);
                planeCb[i] = (uint8_t)std::clamp((-0.168736f * r - 0.331264f * g + 0.5f * b + 128.0f), 0.0f, 255.0f);
                planeCr[i] = (uint8_t)std::clamp((0.5f * r - 0.418688f * g - 0.081312f * b + 128.0f), 0.0f, 255.0f);
            }

            for (uint32_t i = 0; i < 3; i++) {
                imageBuffers.planes[i] = allocPlane(context, width, height, imageBuffers.desc.bitDepth, &err);
                if (err != CL_SUCCESS) {
                    freeImageBuffers(imageBuffers);
                    std::cout << "Failed to allocate memory for image planes\n"; 
                    return 1;
                }
            }
            err = clEnqueueWriteBuffer(queue, imageBuffers.planes[0], CL_TRUE, 0, PIXEL_COUNT * sizeof(uint8_t), planeY.data(), 0, NULL, NULL);
            CL_CHECK(err);
            err = clEnqueueWriteBuffer(queue, imageBuffers.planes[1], CL_TRUE, 0, PIXEL_COUNT * sizeof(uint8_t), planeCb.data(), 0, NULL, NULL);
            CL_CHECK(err);
            err = clEnqueueWriteBuffer(queue, imageBuffers.planes[2], CL_TRUE, 0, PIXEL_COUNT * sizeof(uint8_t), planeCr.data(), 0, NULL, NULL);
            CL_CHECK(err);
        }
        stbi_image_free(image_8b);  
    } else {
        // 16-bit image loading 
        unsigned short* image_16b = (unsigned short*)stbi_load_16(path.c_str(), &width, &height, &channels, 0);
        if (!image_16b) {
            std::cerr << "Failed to load 16-bit image\n";
            return 1;
        }

        if (imageBuffers.desc.isGrayscale) {
            std::cout << "Loaded grayscale 16-bit image: w=" << width << " h=" << height << " c=" << channels << "\n";
            imageBuffers.planes[0] = allocPlane(context, width, height, imageBuffers.desc.bitDepth, &err);
            CL_CHECK(err);
            err = clEnqueueWriteBuffer(queue, imageBuffers.planes[0], CL_TRUE, 0, PIXEL_COUNT * sizeof(uint16_t), image_16b, 0, NULL, NULL);
            CL_CHECK(err);
        } else {
            std::cout << "Loaded color 16-bit image: w=" << width << " h=" << height << " c=" << channels << "\n";
            // Convert to YCbCr
            std::vector<uint16_t> planeY (PIXEL_COUNT);
            std::vector<uint16_t> planeCb(PIXEL_COUNT);
            std::vector<uint16_t> planeCr(PIXEL_COUNT);

            for (size_t i = 0; i < PIXEL_COUNT; i++) {
                float r = (float)image_16b[i * 3 + 0];
                float g = (float)image_16b[i * 3 + 1];
                float b = (float)image_16b[i * 3 + 2];

                // Convert RGB to YCbCr
                planeY [i] = (uint16_t)std::clamp((0.299f * r + 0.587f * g + 0.114f * b), 0.0f, 65535.0f);
                planeCb[i] = (uint16_t)std::clamp((-0.168736f * r - 0.331264f * g + 0.5f * b + 32768.0f), 0.0f, 65535.0f);
                planeCr[i] = (uint16_t)std::clamp((0.5f * r - 0.418688f * g - 0.081312f * b + 32768.0f), 0.0f, 65535.0f);
            }

            for (uint32_t i = 0; i < 3; i++) {
                imageBuffers.planes[i] = allocPlane(context, width, height, imageBuffers.desc.bitDepth, &err);
                if (err != CL_SUCCESS) {
                    freeImageBuffers(imageBuffers);
                    std::cout << "Failed to allocate memory for image planes\n"; 
                    return 1;
                }
            }
            err = clEnqueueWriteBuffer(queue, imageBuffers.planes[0], CL_TRUE, 0, PIXEL_COUNT * sizeof(uint16_t), planeY.data(), 0, NULL, NULL);
            CL_CHECK(err);
            err = clEnqueueWriteBuffer(queue, imageBuffers.planes[1], CL_TRUE, 0, PIXEL_COUNT * sizeof(uint16_t), planeCb.data(), 0, NULL, NULL);
            CL_CHECK(err);
            err = clEnqueueWriteBuffer(queue, imageBuffers.planes[2], CL_TRUE, 0, PIXEL_COUNT * sizeof(uint16_t), planeCr.data(), 0, NULL, NULL);
            CL_CHECK(err);
        }
        stbi_image_free(image_16b);
    }

    if (imageBuffers.desc.bitDepth == BitDepth::U8) {
        std::cout << "Image bit depth: 8-bit\n";
    } else {
        std::cout << "Image bit depth: 16-bit\n";
    }

    OpenCLResources allResources[] = {
        NAIVE_IMPLEMENTATION
    };

    for (OpenCLResources& resource : allResources) {
        processOpenCL(resource, imageBuffers, context, queue, device, path);
    }

    // Release stuff at end
    freeImageBuffers(imageBuffers);
    //clReleaseKernel(kernel);
    //clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
    
    return 0;
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

    for (const auto& path : imagePaths) {
        processImage(path);
    }

    return 0;
}

