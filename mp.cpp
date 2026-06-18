#define CL_TARGET_OPENCL_VERSION 200
#define wls 256
#include <filesystem>
#include <iostream>
#include <fstream>
#include <CL/cl.h>
#include <vector>
#include <random>

struct {
    uint target_lines = 2;
    uint scratchpad_size = 1024;
    uint stress_line_size = 64;
    std::vector<uint> scratch_locations;
} stressparams;

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
        exit(EXIT_FAILURE);
    } else if (e != CL_SUCCESS) {
        std::cerr << "OpenCL build failed: " << e << std::endl;
        exit(EXIT_FAILURE);
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

void set_scratchlocations(std::mt19937 gen,
    std::uniform_int_distribution<uint> distrib_regions,
    std::uniform_int_distribution<uint> distrib_linesize,
    uint scratch_num_regions,
    uint workgroups) {
    std::vector<bool> used_regions(scratch_num_regions, 0);
    for (uint i=0; i < stressparams.target_lines; i ++) {
        uint region = distrib_regions(gen);
        while (used_regions[region]) region = distrib_regions(gen);
        used_regions[region] = 1;
        uint loc_in_region = distrib_linesize(gen);
        for (uint j = i; j < workgroups; j += stressparams.target_lines) {
            stressparams.scratch_locations[j] = region * stressparams.stress_line_size + loc_in_region;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: mp <path_to_source>" << std::endl;
        return 1;
    }
    std::string source_path = argv[1];
    std::string r_source_paht = argv[2];
    uint workgroups = argc > 3 ? atol(argv[3]) : 2;
    uint iterations = argc > 4 ? atol(argv[4]) : 1;
    stressparams.scratch_locations.resize(workgroups);

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

    std::vector<uint> zeros(wls * workgroups, 0);
    std::vector<uint> args_h[5] = {zeros, zeros, zeros, zeros, zeros};
    cl_mem args_d[5];
    args_d[4] = clCreateBuffer(context, CL_MEM_READ_WRITE, stressparams.scratchpad_size * sizeof(uint), NULL, &ret);
    check(clSetKernelArg(kernel, 4, sizeof(cl_mem), &args_d[4]), "Set arg main");
    for (int i=0; i < 4; i++) {
        args_d[i] = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, wls * workgroups * sizeof(uint), args_h[i].data(), &ret);
        check(ret, "Create buffer");
        check(clSetKernelArg(kernel, i, sizeof(cl_mem), &args_d[i]), "Set arg main");
    }
    cl_mem scratch_locations_d = clCreateBuffer(context, CL_MEM_READ_WRITE, workgroups * sizeof(uint), NULL, &ret);
    check(ret, "Create location buffer");
    check(clSetKernelArg(kernel, 5, sizeof(cl_mem), &scratch_locations_d));
    uint results_h[4] = {0};
    cl_mem results_d = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, 4 * sizeof(uint), results_h, &ret);
    check(ret, "Create resultd");
    check(clSetKernelArg(rkernel, 0, sizeof(cl_mem), &args_d[2]), "Set arg");
    check(clSetKernelArg(rkernel, 1, sizeof(cl_mem), &args_d[3]), "Set arg");
    check(clSetKernelArg(rkernel, 2, sizeof(cl_mem), &results_d), "set arg");

    cl_command_queue q = clCreateCommandQueueWithProperties(context, device_id, NULL, &ret);
    check(ret, "q");
    size_t wlsize[1] {wls};
    size_t wgsize[1] {wls * 3 * workgroups};
    size_t rwgsize[1] {wls * workgroups};
    std::random_device rd;
    std::mt19937 gen(rd());
    uint scratch_num_regions = stressparams.scratchpad_size / stressparams.stress_line_size;
    std::uniform_int_distribution<uint> distrib_reg(0,(scratch_num_regions - 1));
    std::uniform_int_distribution<uint> distrib_lz(0,stressparams.stress_line_size);
    for (int i=0; i<iterations; i++) {
        set_scratchlocations(gen, distrib_reg, distrib_lz, scratch_num_regions, workgroups);
        check(clEnqueueWriteBuffer(q, scratch_locations_d, true, 0, workgroups * sizeof(uint), stressparams.scratch_locations.data(), 0, 0, 0));
        check(clEnqueueNDRangeKernel(q, kernel, 1, 0, wgsize, wlsize, 0, 0, 0), "launch kernel");
        check(clEnqueueNDRangeKernel(q, rkernel, 1, 0, rwgsize, wlsize, 0, 0, 0), "launch result kernel");
        for (int i=0; i < 4; i++) {
            check(clEnqueueFillBuffer(q, args_d[i], args_h[0].data(), sizeof(uint), 0, wls * workgroups, 0, 0, 0), "write");
        }
    }
    check(clEnqueueReadBuffer(q, results_d, true, 0, 4 * sizeof(uint), results_h, 0, 0, 0), "Read");
    clFinish(q);
    for (int i=0; i<4; i++) {
        std::cout << results_h[i] << std::endl;
    }
}