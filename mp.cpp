#define CL_TARGET_OPENCL_VERSION 200
#include <filesystem>
#include <iostream>
#include <fstream>
#include <CL/cl.h>
#include <vector>

static inline void check(cl_int e, const char* what = ""){
    if(e != CL_SUCCESS){
        std::cerr << what << " error" << e << std::endl;
        exit(EXIT_FAILURE);
    }
}

static inline void check_build(cl_int e, cl_program program, cl_device_id device) {
    if (e == CL_BUILD_PROGRAM_FAILURE) {
        std::cerr << "OpenCL build failed:" << std::endl;
        size_t len;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &len);
        std::string log(len, '*');
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, len, log.data(), NULL);
        std::cout << log.data() << std::endl;
        std::exit(EXIT_FAILURE);
    } else if (e != CL_SUCCESS) {
        std::cerr << "OpenCL build failed: " << e << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

void read_source(std::string path, char* source, uint filesize) {
    std::ifstream fin(path, std::ios::binary);
    fin.read(source, filesize);
    if(!fin) {
        std::cerr << "Error:only read " << fin.gcount() << " bytes" << std::endl;
    }
    fin.close();
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: mp <path_to_source>" << std::endl;
        return 1;
    }
    std::string source_path = argv[1];
    std::string r_source_paht = argv[2];
    uint workgroups = argc > 3 ? atol(argv[3]) : 1;
    uint iterations = argc > 4 ? atol(argv[4]) : 1; //Will need to add argument initialization

    std::uintmax_t sourcesize = std::filesystem::file_size(source_path);
    std::string source(sourcesize, '*');
    read_source(source_path, source.data(), sourcesize);

    std::uintmax_t r_source_size = std::filesystem::file_size(r_source_paht);
    std::string r_source(r_source_size, '*');
    read_source(r_source_paht, r_source.data(), r_source_size);

    cl_platform_id platform_id;
    cl_uint num_platforms;
    cl_device_id device_id;
    cl_uint num_devices;
    cl_int ret;

    //Create platfrom and context build program
    check(clGetPlatformIDs(1, 
        &platform_id,
        &num_platforms), 
        "platformID");
    check(clGetDeviceIDs(platform_id,
        CL_DEVICE_TYPE_GPU,
        1,
        &device_id,
        &num_devices),
        "DeviceID");
    cl_context context = clCreateContext(NULL,
        1,
        &device_id,
        NULL,
        NULL,
        &ret);
    check(ret, "context");

    std::cout << "creating programs" << std::endl;
    const char* psource = source.c_str();
    cl_program program = clCreateProgramWithSource(context,
        1,
        (const char**) &psource,
        &sourcesize,
        &ret);
    check(ret, "program");
    check_build(clBuildProgram(program,
        1,
        &device_id, 
        "-cl-std=CL2.0",
        NULL,
        NULL),
        program, device_id);

    const char* prsource = r_source.c_str();
    cl_program result_program = clCreateProgramWithSource(context,
        1,
        (const char**) &prsource,
        &r_source_size,
        &ret);
    check(ret, "result program");
    check_build(clBuildProgram(result_program,
        1,
        &device_id, 
        "-cl-std=CL2.0",
        NULL,
        NULL),
        result_program, device_id);

    std::cout << "Created program" << std::endl;

    //Get kernel name 
    size_t pname_size;
    check(clGetProgramInfo(program, CL_PROGRAM_KERNEL_NAMES, 0, NULL, &pname_size), "Program info size");
    std::vector<char> pname(pname_size);
    check(clGetProgramInfo(program, CL_PROGRAM_KERNEL_NAMES, pname_size, pname.data(), NULL), "Program info");

    cl_kernel kernel = clCreateKernel(program, pname.data(), &ret);
    check(ret, "Create kernel");

    size_t rpname_size;
    check(clGetProgramInfo(result_program, CL_PROGRAM_KERNEL_NAMES, 0, NULL, &rpname_size), "Result program info size");
    std::vector<char> rpname(rpname_size);
    check(clGetProgramInfo(result_program, CL_PROGRAM_KERNEL_NAMES, rpname_size, rpname.data(), NULL), "Result program info");

    cl_kernel rkernel = clCreateKernel(result_program, rpname.data(), &ret);
    check(ret, "Create result kernel");

    std::vector<uint> zeros(256 * workgroups, 0);
    std::vector<uint> args_h[5] = {zeros, zeros, zeros, zeros, zeros};
    cl_mem args_d[5];
    for (int i=0; i < 5; i++) {
        args_d[i] = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, 256 * workgroups * sizeof(uint), args_h[i].data(), &ret);
        check(ret, "Create buffer");
        check(clSetKernelArg(kernel, i, sizeof(cl_mem), &args_d[i]), "Set arg main");
    }
    uint results_h[4] = {0};
    cl_mem results_d = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, 4 * sizeof(uint), results_h, &ret);
    check(ret, "Create resultd");
    check(clSetKernelArg(rkernel, 0, sizeof(cl_mem), &args_d[2]), "Set arg");
    check(clSetKernelArg(rkernel, 1, sizeof(cl_mem), &args_d[3]), "Set arg");
    check(clSetKernelArg(rkernel, 2, sizeof(cl_mem), &results_d), "set arg");

    cl_command_queue q = clCreateCommandQueueWithProperties(context, device_id, NULL, &ret);
    check(ret, "q");
    size_t wlsize[1] {256};
    size_t wgsize[1] {256 * 3 * workgroups};
    size_t rwgsize[1] {256 * workgroups};
    for (int i=0; i<iterations; i++) {
        check(clEnqueueNDRangeKernel(q, kernel, 1, 0, wgsize, wlsize, 0, 0, 0), "launch kernel");
        check(clEnqueueNDRangeKernel(q, rkernel, 1, 0, rwgsize, wlsize, 0, 0, 0), "launch result kernel");
        for (int i=0; i < 4; i++) {
            check(clEnqueueWriteBuffer(q, args_d[i], true, 0, 256 * workgroups * sizeof(uint), args_h[i].data(), 0, 0, 0), "write");
        }
    }
    check(clEnqueueReadBuffer(q, results_d, true, 0, 4 * sizeof(uint), results_h, 0, 0, 0), "Read");
    clFinish(q);
    for (int i=0; i<4; i++) {
        std::cout << results_h[i] << std::endl;
    }
}