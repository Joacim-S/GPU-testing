#include <cstddef>
#include <cstdlib>
#include <ostream>
#include <string>
#define CL_TARGET_OPENCL_VERSION 300

#include <string>
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

int main(int argc, char *argv[]) {
    bool info = false;
    if (argc < 2) {
        std::cout << "usage: atomic.cpp <filename>" << std::endl;
        exit(1);
    }

    std::string filename = argv[1];

    std::uintmax_t filesize = std::filesystem::file_size(filename);
    char* il = new char[filesize];
    std::ifstream fin(filename, std::ios::binary);
    fin.read(il, filesize);
    if(!fin) {
        std::cerr << "Error:only read " << fin.gcount() << " bytes" << std::endl;
    }
    fin.close();

    std::string asmfile = filename + "asm";
    std::string line;
    std::ifstream asmin(asmfile);
    if (!asmin.good()) {
        std::cerr << asmfile << " does not exist" << std::endl;
        exit(EXIT_FAILURE);
    }
    size_t asmi;
    while((asmi = line.find("@Config:")) == std::string::npos){
        std::getline(asmin, line);
    }
    asmin.close();

    while(line[asmi] != ' ')
        asmi++;
    asmi ++;
    size_t wlsize[3];
    size_t wgsize[3];
    
    for (int i=0; i<3; i++) {
        wlsize[i] = atoi(&line[asmi]);
        wgsize[i] = atoi(&line[asmi]);
        asmi+=3;
    }
    

    cl_platform_id platform_id;
    cl_uint num_platforms;
    cl_device_id device_id;
    cl_uint num_devices;
    cl_int ret;

    //Create platfrom and context build program
    check(clGetPlatformIDs(1, &platform_id,&num_platforms));
    check(clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, &num_devices));
    cl_context context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &ret);
    check(ret);
    cl_program program = clCreateProgramWithIL(context, il, filesize, &ret);
    check(ret);
    check(clBuildProgram(program, 1, &device_id, NULL, NULL, NULL));

    //Get kernel name
    size_t pname_size;
    clGetProgramInfo(program, CL_PROGRAM_KERNEL_NAMES, 0, NULL, &pname_size);
    char* pname = new char[pname_size];
    clGetProgramInfo(program, CL_PROGRAM_KERNEL_NAMES, pname_size, pname, NULL);

    //Create kernel
    cl_kernel kernel = clCreateKernel(program, pname, &ret);

    cl_uint num_args;
    size_t asize;
    char **args;
    char *arg;
    clGetKernelInfo(kernel, CL_KERNEL_NUM_ARGS, sizeof(cl_uint), &num_args, NULL);
    args = new char*[num_args];
    for (int i=0; i<num_args; i++) {
        check(clGetKernelArgInfo(kernel, i, CL_KERNEL_ARG_TYPE_NAME, 0, NULL, &asize));
        arg = new char[asize];
        args[i] = arg;
        check(clGetKernelArgInfo(kernel, i, CL_KERNEL_ARG_TYPE_NAME, asize, arg, NULL));
        std::cout << arg << std::endl;
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

    ret = clEnqueueNDRangeKernel(q, kernel, 3, NULL, wgsize, wlsize, 0, NULL, NULL);
    if (ret != CL_SUCCESS){
        printf("OpenCL error executing kernel: %d\n", ret);
        exit(1);
    }
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
}