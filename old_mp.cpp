#define CL_TARGET_OPENCL_VERSION 200
#include <filesystem>
#include <iostream>
#include <fstream>
#include <CL/cl.h>

void check(cl_int e, const char* what = ""){
    if(e != CL_SUCCESS){
        printf("%s error: %d\n", what, e);
        
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: mp <path_to_source>" << std::endl;
        return 1;
    }
    std::string source_path = argv[1];
    std::uintmax_t filesize = std::filesystem::file_size(source_path);
    char* source = new char[filesize];
    std::ifstream fin(source_path, std::ios::binary);
    fin.read(source, filesize);
    if(!fin) {
        std::cerr << "Error:only read " << fin.gcount() << " bytes" << std::endl;
    }
    fin.close();

    cl_platform_id platform_id;
    cl_uint num_platforms;
    cl_device_id device_id;
    cl_uint num_devices;
    cl_int ret;

    //Create platfrom and context build program
    check(clGetPlatformIDs(1, &platform_id,&num_platforms), "platformID");
    check(clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, &num_devices), "DeviceID");
    cl_context context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &ret);
    check(ret, "context");
    cl_program program = clCreateProgramWithSource(context, 1, (const char**) &source, &filesize, &ret);
    check(ret, "program");
    check(clBuildProgram(program, 1, &device_id,  "-cl-std=CL2.0", NULL, NULL), "Build program");

    //Get kernel name 
    size_t pname_size;
    check(clGetProgramInfo(program, CL_PROGRAM_KERNEL_NAMES, 0, NULL, &pname_size), "Program info size");
    char* pname = new char[pname_size];
    check(clGetProgramInfo(program, CL_PROGRAM_KERNEL_NAMES, pname_size, pname, NULL), "Program info");

    cl_kernel kernel = clCreateKernel(program, pname, &ret);
    check(ret, "Create kernel");
    //flag, data, r0, r1
    uint args_h[4] = {0};
    cl_mem args_d[4];
    for (int i=0; i < 4; i++) {
        args_d[i] = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(uint), &args_h[i], &ret);
        check(ret, "Create buffer");
        check(clSetKernelArg(kernel, i, sizeof(cl_mem), &args_d[i]), "Set arg");
    }
    
    cl_command_queue q = clCreateCommandQueueWithProperties(context, device_id, NULL, &ret);
    check(ret, "q");

    size_t wlsize[1] {1};
    size_t wgsize[1] {2};

    ret = clEnqueueNDRangeKernel(q, kernel, 1, NULL, wgsize, wlsize, 0, NULL, NULL);
    check(ret, "Enqueue");

    clFinish(q);
    for (int i=0; i<4; i++) {
        check(clEnqueueReadBuffer(q, args_d[i], true, 0, sizeof(uint), &args_h[i], 0, NULL, NULL));
        std::cout << args_h[i] << " ";
    }
    std::cout << std::endl;

    for (int i=0; i<4; i++) check(clReleaseMemObject(args_d[i]), "memrelease");
    check(clReleaseKernel(kernel), "kernelrelease");
    check(clReleaseProgram(program));
    check(clReleaseCommandQueue(q));
    check(clReleaseContext(context));
}