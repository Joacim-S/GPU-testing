#include <ostream>
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
    if (argc < 2 || argc > 4) {
        std::cout << "usage: atomic.cpp <strind: spv file path (required)> <int: concurrent kernels> <int: iterations>" << std::endl;
        exit(1);
    }
    std::string spvpath = argv[1];

    size_t kernel_count = argc > 2 ? atol(argv[2]): 1;
    size_t iterations = argc == 4 ? atol(argv[3]) : 1;

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
    char* output = new char[1];
    output[0] = '\0';
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

    //Create kernels
    cl_kernel* kernels = new cl_kernel[kernel_count];
    for (int i = 0; i < kernel_count; i++) {
        kernels[i] = clCreateKernel(program, pname, &ret);
        check(ret, "Create kernel");
    }


    //Initialize memory and set kernel args.
    //TODO: Reserve one big memory area. This takes forever.
    std::vector<uint> constArgs_h;
    size_t arg_count = 0;
    for (int i=0; i<inputs.size(); i++) {
        if (inputs[i][0]) arg_count += 1;
        else constArgs_h.push_back(inputs[i][1]);
    }
    std::vector<std::vector<uint>> args_h(kernel_count * arg_count);
    std::vector<cl_mem> args_d(kernel_count * arg_count);
    for (int k=0; k<kernel_count; k++) {
        int ci = 0;
        for (int i=0; i<inputs.size(); i++) {
            int l = inputs[i][0];
            if (l) {
                std::vector<uint> arg(l);
                for (int j=0; j<l; j++) {
                    arg[j] = inputs[i][j+1];
                }
                args_h[i + k * arg_count] = arg;
                cl_mem arg_d = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, l*sizeof(uint), arg.data(), &ret);
                check(ret, "Create buffer");
                args_d[i + k * arg_count] = arg_d;
                check(clSetKernelArg(kernels[k], i, sizeof(cl_mem), &arg_d), "Set arg");
            }
            else {
                check(clSetKernelArg(kernels[k], i, sizeof(uint), &constArgs_h[ci++]), "Set const arg");
            }
        }
    }
    
    cl_queue_properties properties[] = { CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE, 0 };
    cl_command_queue *queues = new cl_command_queue[kernel_count]; //Need several queus to get parallel execution. 1k works, 10k is too much.
    for (int i = 0; i < kernel_count; ++i) {
        queues[i] = clCreateCommandQueueWithProperties(context, device_id, properties, &ret);
    }
    check(ret, "q");

    cl_event user_event = clCreateUserEvent(context, &ret);
    cl_event* kernel_events = new cl_event[kernel_count];
    check(ret, "Create event");
    for (int k=0; k<kernel_count; k++) {
        ret = clEnqueueNDRangeKernel(queues[k], kernels[k], 1, NULL, wgsize, wlsize, 1, &user_event, &kernel_events[k]);
        check(ret, "Enqueue");
    }
    std::cout << "Enqueued" << std::endl;
    clSetUserEventStatus(user_event, CL_COMPLETE);
    std::cout << "Finished" << std::endl;
    for (int k=0; k<kernel_count; k++) {
        int ko = k * arg_count;
        std::cout << OUT << output << '\n';
        std::cout << "Result:" << '\n';
        for (int i=0; i<arg_count; i++) {
            check(clEnqueueReadBuffer(queues[k], args_d[i + ko], true, 0, sizeof(uint) * args_h[i + ko].size(), args_h[i + ko].data(), 0, NULL, NULL), "read");
            std::cout << arg_names[i] << ": ";
            for (int j=0; j<args_h[i + ko].size(); j++) {
                std::cout << args_h[i + ko][j] << " ";
            }
            std::cout << '\n';
        }
    }

    for (int i=0; i<args_d.size(); i++) check(clReleaseMemObject(args_d[i]), "memrelease");
    for (int i=0; i<kernel_count; i++) {
        check(clReleaseKernel(kernels[i]), "kernelrelease");
    }
    check(clReleaseProgram(program));
    for (int i=0; i<kernel_count; i++) check(clReleaseCommandQueue(queues[i]));
    check(clReleaseContext(context));

    delete[] il;
    delete[] pname;
    delete[] output;
    delete [] kernels;
    delete [] kernel_events;
    delete [] queues;
    for (int i=0; i<arg_names.size(); i++) delete[] arg_names[i];
    for (int i=0; i<inputs.size(); i++) delete[] inputs[i];
}