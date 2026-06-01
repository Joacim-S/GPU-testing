#define CL_TARGET_OPENCL_VERSION 300
#include<bits/stdc++.h>
#include "fileutils.h"
#include <vector>
#include <CL/cl.h>

void check(cl_int e, const char* what = ""){
    if(e != CL_SUCCESS){
        printf("%s error: %d\n", what, e);
        
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    #define OUT "@Output:"
    if (argc < 2) {
        std::cout << "usage: atomic.cpp <spv filename>" << std::endl;
        exit(1);
    }

    std::string spvpath = argv[1];
    std::uintmax_t filesize = std::filesystem::file_size(spvpath);
    char* il = new char[filesize];
    std::ifstream fin(spvpath, std::ios::binary);
    fin.read(il, filesize);
    if(!fin) {
        std::cerr << "Error:only read " << fin.gcount() << " bytes" << std::endl;
    }
    fin.close();

    std::string asmpath;
    getAsmPath(spvpath, asmpath);
    size_t wlsize[1];
    size_t wgsize[1];
    std::vector<uint*> inputs;
    char* output;
    std::vector<char*> arg_names;

    parseInput(asmpath, arg_names, inputs, wlsize, wgsize, output);


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
    cl_program program = clCreateProgramWithIL(context, il, filesize, &ret);
    check(ret, "program");
    check(clBuildProgram(program, 1, &device_id, NULL, NULL, NULL), "Build program");

    //Get kernel name
    size_t pname_size;
    check(clGetProgramInfo(program, CL_PROGRAM_KERNEL_NAMES, 0, NULL, &pname_size), "Program info size");
    char* pname = new char[pname_size];
    check(clGetProgramInfo(program, CL_PROGRAM_KERNEL_NAMES, pname_size, pname, NULL), "Program info");

    //Create kernel
    cl_kernel kernel = clCreateKernel(program, pname, &ret);
    check(ret, "Create kernel");


    //Initialize memory and set kernel args.
    std::vector<std::vector<uint>> args_h;
    std::vector<cl_mem> args_d;
    std::vector<uint> constArgs_h;
    int ci = 0;
    for (int i=0; i<inputs.size(); i++) {
        int l = inputs[i][0];
        if (l) {
            std::vector<uint> arg;
            for (int j=0; j<l; j++) {
                arg.push_back(inputs[i][j+1]);
            }
            args_h.push_back(arg);
            cl_mem arg_d = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, l*sizeof(uint), arg.data(), &ret);
            check(ret, "Create buffer");
            args_d.push_back(arg_d);
            check(clSetKernelArg(kernel, i, sizeof(cl_mem), &arg_d), "Set arg");
        }
        else {
            constArgs_h.push_back(inputs[i][1]);
            check(clSetKernelArg(kernel, i, sizeof(uint), &constArgs_h[ci++]), "Set const arg");
        }
    }
    

    cl_command_queue q = clCreateCommandQueueWithProperties(context, device_id, NULL, &ret); //CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE
    check(ret, "q");

    //Command buffers?
    ret = clEnqueueNDRangeKernel(q, kernel, 1, NULL, wgsize, wlsize, 0, NULL, NULL);
    if (ret != CL_SUCCESS){
        printf("OpenCL error executing kernel: %d\n", ret);
        exit(1);
    }
    check(clFinish(q));
    std::cout << OUT  << " " << output << std::endl;
    std::cout << "Result:" << std::endl;
    for (int i=0; i<args_d.size(); i++) {
        check(clEnqueueReadBuffer(q, args_d[i], true, 0, sizeof(uint) * args_h[i].size(), args_h[i].data(), 0, NULL, NULL), "read");
        check(clFinish(q));
        std::cout << arg_names[i] << ": ";
        for (int j=0; j<args_h[i].size(); j++) {
            std::cout << args_h[i][j] << " ";
        }
        std::cout << std::endl;
    }
    for (int i=0; i<args_d.size(); i++) {
        check(clReleaseMemObject(args_d[i]), "memrelease");
    }
    
    check(clReleaseKernel(kernel), "kernelrelease");
    check(clReleaseProgram(program));
    check(clReleaseCommandQueue(q));
    check(clReleaseContext(context));

    delete[] il;
    delete[] pname;
    delete[] output;
    for (int i=0; i<arg_names.size(); i++) {
        delete[] arg_names[i];
        delete[] inputs[i];
    }
}