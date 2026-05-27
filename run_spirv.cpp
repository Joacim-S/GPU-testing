#include<bits/stdc++.h>
#include <cctype>
#define CL_TARGET_OPENCL_VERSION 300

#include <cstring>
#include <vector>
#include <CL/cl.h>
#include <filesystem>
#include <fstream>
#include <iostream>

void check(cl_int e, const char* what = ""){
    if(e != CL_SUCCESS){
        printf("%s error: %d\n", what, e);
        
        exit(EXIT_FAILURE);
    }
}

//Parses parameters for running the kernel and saves them to given pointers.
//Inputs is an array of pointers to arrays of uints starting with the length of the array.
//0 Length implies the value is a constant.
void parse_input(std::string* filename, std::vector<char*> input_names, std::vector<uint*> inputs, size_t* wlsize, size_t* wgsize, std::string* output) {
    std::string line = "@";
    std::ifstream asmin(*filename);
    if (!asmin.good()) {
        std::cerr << "Failed to open file:" << filename << std::endl;
        exit(EXIT_FAILURE);
    }
    size_t li, s;
    size_t ic = 0; //Input count
    while(line.find("@") != std::string::npos){
        std::getline(asmin, line);
        if ((li = line.find("@Input:")) != std::string::npos) {
            size_t p = line.find("%");
            s = line.find(" ", p) - p;
            input_names.push_back(new char[s+1]);
            strncpy(input_names[ic], &line[p], s);

            size_t lb, rb, il = 1;
            li = line.find("=");
            bool is_array = ((lb = line.find("{", li)) != std::string::npos);
            if (is_array) {
                rb = line.find("}", lb);
                if (rb == std::string::npos) {
                    std::cerr << "Invalid input row in file:" << filename << std::endl << line << std::endl;
                }
                il += std::count(line.begin()+lb, line.begin()+rb, ','); //Input length
            }
            uint* input = new uint[il + 1];
            input[0] = is_array * il;
            for (int i=0; i<il; i++) {
                while (!std::isdigit(line[li])) 
                    li++;
                input[i] = atoi(&line[li]);
                while (std::isdigit(line[li]))
                    li++;
            }
        }
    }
    asmin.close();

    /*
    for (int i=0; i<3; i++) {
        wlsize[i] = atoi(&line[asmi]);
        wgsize[i] = atoi(&line[asmi]);
        asmi+=3;
    }
    */
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


    size_t wlsize[3];
    size_t wgsize[3];
    std::string asmfile = filename + "asm";
    std::vector<uint*> inputs;
    std::string output;
    std::vector<char*> args;


    parse_input(&asmfile, args, inputs, wlsize, wgsize, &output);
    exit(0);

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

    /*
    cl_uint num_args;
    size_t asize;
    char **args;
    char *arg;
    check(clGetKernelInfo(kernel, CL_KERNEL_NUM_ARGS, sizeof(cl_uint), &num_args, NULL));
    args = new char*[num_args];
    for (int i=0; i<num_args; i++) {
        std::cout << num_args << std::endl;
        check(clGetKernelArgInfo(kernel, i, CL_KERNEL_ARG_TYPE_NAME, 0, NULL, &asize), "argtypesize");
        arg = new char[asize];
        args[i] = arg;
        check(clGetKernelArgInfo(kernel, i, CL_KERNEL_ARG_TYPE_NAME, asize, arg, NULL), "argtypename");
        std::cout << arg << std::endl;
    }
    exit(0);
    */

    
    uint A_h = 0;
    const uint B = 1;
    cl_mem A_d = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(uint), &A_h, &ret);
    check(ret, "create buffer");
    ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), &A_d);
    check(ret, "arg0");
    ret = clSetKernelArg(kernel, 1, sizeof(uint), &B);
    check(ret, "arg1");
    cl_command_queue q = clCreateCommandQueueWithProperties(context, device_id, NULL, &ret); //CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE
    check(ret, "q");

    //Command buffers?
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
    for (int i=0; i<args.size(); i++)
        delete[] args[i];
}