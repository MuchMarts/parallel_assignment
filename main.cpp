#include <filesystem>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

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

const int NUM_BUCKETS = 256;

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

int processImage(const std::string& path) {
    std::cout << "Processing image" << path << std::endl;

    int width, height, channels;
    int ok = stbi_info(path.c_str(), &width, &height, &channels);
    if (ok) {
        std::cout << "w" << width << " h" << height << " c" << channels << "\n"; 
    } else {
        std::cout << "stbi_info not ok\n";
    }

    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 1); // force grayscale with last arg = 1
    if (!pixels) {
        std::cerr << "Failed to load image\n";
        return 1;
    }
    int DATA_LENGTH = width * height;

    // --- Data ---
    std::vector<uint32_t> histogram(NUM_BUCKETS, 0);

    // --- OpenCL setup ---
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

    // Load opencl file
    std::string kernelNaiveSrc = readFile("naive.cl");
    const char* src = kernelNaiveSrc.c_str();

    cl_program program = clCreateProgramWithSource(context, 1, &src, NULL, &err);
    CL_CHECK(err);

    err = clBuildProgram(program, 1, &device, "-cl-fast-relaxed-math -cl-mad-enable", NULL, NULL);

    if (err != CL_SUCCESS) {
        size_t log_size;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);

        std::vector<char> log(log_size);
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, log.data(), NULL);

        std::cerr << "Build failed:\n" << log.data() << std::endl;
        return 1;
    }

    // Load all kenels
    cl_kernel histogram_kernel = clCreateKernel(program, "HistogramNaive", &err);
    CL_CHECK(err);
    cl_kernel scan_kernel = clCreateKernel(program, "ScanNaive", &err);
    CL_CHECK(err);
    cl_kernel norm_scale_kernel = clCreateKernel(program, "NormaliseAndScale", &err);
    CL_CHECK(err);
    cl_kernel output_kernel = clCreateKernel(program, "Backproject", &err);
    CL_CHECK(err);

    // Buffers
    cl_mem data_buffer = clCreateBuffer(
        context,
        CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        DATA_LENGTH * sizeof(unsigned char),
        pixels,
        &err);
    CL_CHECK(err);
    stbi_image_free(pixels);
    
    cl_mem histogram_buffer = clCreateBuffer(
        context,
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        NUM_BUCKETS * sizeof(uint32_t),
        histogram.data(),
        &err);
    CL_CHECK(err);

    cl_mem cumulative_hist_buffer = clCreateBuffer(
        context,
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        NUM_BUCKETS * sizeof(uint32_t),
        histogram.data(),
        &err);
    CL_CHECK(err);

    cl_mem lut_buffer = clCreateBuffer(
        context,
        CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        NUM_BUCKETS * sizeof(uint32_t),
        histogram.data(),
        &err);
    CL_CHECK(err);

    cl_mem output_buffer = clCreateBuffer(
        context,
        CL_MEM_WRITE_ONLY,
        DATA_LENGTH * sizeof(unsigned char),
        NULL,
        &err);
    CL_CHECK(err);

    // Histogram kernel args
    CL_CHECK(clSetKernelArg(histogram_kernel, 0, sizeof(cl_mem), &data_buffer));
    CL_CHECK(clSetKernelArg(histogram_kernel, 1, sizeof(cl_mem), &histogram_buffer));
    CL_CHECK(clSetKernelArg(histogram_kernel, 2, sizeof(uint32_t), &DATA_LENGTH));
    CL_CHECK(clSetKernelArg(histogram_kernel, 3, sizeof(uint32_t), &NUM_BUCKETS));

    // Scan kernel args
    CL_CHECK(clSetKernelArg(scan_kernel, 0, sizeof(cl_mem), &histogram_buffer));
    CL_CHECK(clSetKernelArg(scan_kernel, 1, sizeof(cl_mem), &cumulative_hist_buffer));
    CL_CHECK(clSetKernelArg(scan_kernel, 2, sizeof(uint32_t), &NUM_BUCKETS));
    
    const uint32_t MAX_INTENSITY = 255;

    // Scan kernel args
    CL_CHECK(clSetKernelArg(norm_scale_kernel, 0, sizeof(cl_mem), &cumulative_hist_buffer));
    CL_CHECK(clSetKernelArg(norm_scale_kernel, 1, sizeof(cl_mem), &lut_buffer));
    CL_CHECK(clSetKernelArg(norm_scale_kernel, 2, sizeof(uint32_t), &DATA_LENGTH));
    CL_CHECK(clSetKernelArg(norm_scale_kernel, 3, sizeof(uint32_t), &NUM_BUCKETS));
    CL_CHECK(clSetKernelArg(norm_scale_kernel, 4, sizeof(uint32_t), &MAX_INTENSITY));
    
    // Backproject
    CL_CHECK(clSetKernelArg(output_kernel, 0, sizeof(cl_mem), &data_buffer));
    CL_CHECK(clSetKernelArg(output_kernel, 1, sizeof(cl_mem), &lut_buffer));
    CL_CHECK(clSetKernelArg(output_kernel, 2, sizeof(cl_mem), &output_buffer));
    
    // Launch histogram kernel
    size_t local  = 64;
    size_t global = ((DATA_LENGTH + local - 1) / local) * local;
    cl_event histogram_event;
    CL_CHECK(clEnqueueNDRangeKernel(queue, histogram_kernel, 1, NULL, &global, &local, 0, NULL, &histogram_event));
    clWaitForEvents(1, &histogram_event);
    //debugPrintBuffer(queue, histogram_buffer, NUM_BUCKETS, "Histogram");

    // Launch scan kernel
    size_t scan_global = 1;
    size_t scan_local  = 1;
    cl_event scan_event;
    CL_CHECK(clEnqueueNDRangeKernel(queue, scan_kernel, 1, NULL, &scan_global, &scan_local, 0, NULL, &scan_event));
    clWaitForEvents(1, &scan_event);
    //debugPrintBuffer(queue, cumulative_hist_buffer, NUM_BUCKETS, "Scan");
    
    // Launch normalize and scale kernel
    size_t norm_global = 1;
    size_t norm_local  = 1;
    cl_event norm_event;
    CL_CHECK(clEnqueueNDRangeKernel(queue, norm_scale_kernel, 1, NULL, &norm_global, &norm_local, 0, NULL, &norm_event));
    clWaitForEvents(1, &norm_event);
    //debugPrintBuffer(queue, lut_buffer, NUM_BUCKETS, "NormalizeAndScan");
    
    // Launch normalize and scale kernel
    size_t out_global = ((DATA_LENGTH + local - 1) / local) * local;
    size_t out_local  = 64;
    cl_event out_event;
    CL_CHECK(clEnqueueNDRangeKernel(queue, output_kernel, 1, NULL, &out_global, &out_local, 0, NULL, &out_event));
    clWaitForEvents(1, &out_event);
    
    // read back
    std::vector<unsigned char> output(DATA_LENGTH);
    CL_CHECK(clEnqueueReadBuffer(
        queue,
        output_buffer,
        CL_TRUE,
        0,
        DATA_LENGTH * sizeof(unsigned char),
        output.data(),
        0, NULL, NULL));

    // write to file
    std::string out_path = path + "_equalized.png";
    int write_ok = stbi_write_png(
        out_path.c_str(),
        width,
        height,
        1,           // 1 channel = grayscale
        output.data(),
        width        // stride = width * channels, for grayscale just width
    );

    if (!write_ok) {
        std::cerr << "Failed to write output image\n";
    }
    std::cout << "Saved: " << out_path << "\n";

    // --- Timing ---
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


    // --- Cleanup ---
    clReleaseKernel(histogram_kernel);
    clReleaseKernel(scan_kernel);
    clReleaseKernel(norm_scale_kernel);
    
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

