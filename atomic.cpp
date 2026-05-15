#define CL_TARGET_OPENCL_VERSION 300

#include <CL/cl.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>

void check(cl_int e, const char* what = ""){
    if(e != CL_SUCCESS){
        printf("%s error: %d\n", what, e);
        
        exit(EXIT_FAILURE);
    }
}

void get_args() {
    //Ask user for kernel arguments and workgroups
}

int main(int argc, char *argv[]) {
    bool info = false;
    if (argc < 2) {
        std::cout << "usage: atomic.cpp <filename>" << std::endl;
        exit(1);
    }
    if (argc > 2) {
        if (std::string(argv[2]) == "info") {
            info = true;
        }
    }
    char* filename = argv[1];
    std::uintmax_t filesize = std::filesystem::file_size(filename);
    char* il = new char[filesize];
    std::ifstream fin(filename, std::ios::binary);
    fin.read(il, filesize);
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
    check(clGetPlatformIDs(1, &platform_id,&num_platforms));
    check(clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, &num_devices));
    cl_context context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &ret);
    cl_program program = clCreateProgramWithIL(context, il, filesize, &ret);
    ret = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);

    //Get kernel name
    size_t pname_size;
    clGetProgramInfo(program, CL_PROGRAM_KERNEL_NAMES, 0, NULL, &pname_size);
    char* pname = new char[pname_size];
    clGetProgramInfo(program, CL_PROGRAM_KERNEL_NAMES, pname_size, pname, NULL);

    //Create kernel
    cl_kernel kernel = clCreateKernel(program, pname, &ret);
    
    //If the info flag was given, print kernel argument info
    struct {
        char* type;

    } arginfo;

    cl_uint num_args;
    size_t psize;
    char **args;
    char* arg;
    if (info) {
        clGetKernelInfo(kernel, CL_KERNEL_NUM_ARGS, sizeof(cl_uint), &num_args, NULL);
        printf("number of args:%d\n", num_args);
        args = new char*[num_args];
        for (int i=0; i<num_args; i++) {
            clGetKernelArgInfo(kernel, i, CL_KERNEL_ARG_TYPE_NAME, 0, NULL, &psize);
            arg = new char[psize];
            args[i] = arg;
            clGetKernelArgInfo(kernel, i, CL_KERNEL_ARG_TYPE_NAME, psize, arg, NULL);
            printf("Arg %d: %s\n", i, arg);
        }
    }

    uint A_h = 0;
    const uint B = 1;
    cl_mem A_d = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(uint), &A_h, &ret);
    check(ret, "create buffer");
    ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), &A_d);
    check(ret, "arg0");
    ret = clSetKernelArg(kernel, 1, sizeof(uint), &B);
    check(ret, "arg1");
    cl_command_queue q = clCreateCommandQueueWithProperties(context, device_id, NULL, &ret);
    check(ret, "q");

    const size_t wlsize[1] = {256};
    const size_t wgsize[1] = {1000*wlsize[0]};

    ret = clEnqueueNDRangeKernel(q, kernel, 1, NULL, wgsize, wlsize, 0, NULL, NULL);
    if (ret != CL_SUCCESS)
        printf("OpenCL error executing kernel: %d\n", ret);
    check(clFinish(q));
    check(clEnqueueReadBuffer(q, A_d, true, 0, sizeof(uint), &A_h, 0, NULL, NULL), "read");
    check(clFinish(q));
    std::cout << A_h << std::endl;
    check(clReleaseMemObject(A_d), "memrelease");
    check(clReleaseKernel(kernel), "kernelrelease");
    check(clReleaseProgram(program));
    check(clReleaseCommandQueue(q));
    check(clReleaseContext(context));
    delete[] il;
    delete[] pname;
    for (int i=0; i<num_args; i++){
        delete[] args[i];
    }
    delete[] args;
}