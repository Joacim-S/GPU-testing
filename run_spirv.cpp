#include<bits/stdc++.h>
#define CL_TARGET_OPENCL_VERSION 300

#include <vector>
#include <CL/cl.h>

void check(cl_int e, const char* what = ""){
    if(e != CL_SUCCESS){
        printf("%s error: %d\n", what, e);
        
        exit(EXIT_FAILURE);
    }
}

//Parses parameters for running the kernel and saves them to given pointers.
//Inputs is an array of pointers to arrays of uints starting with the length of the array.
//0 Length implies the value is a constant.
//Output is just a 
int parseInput(std::string& filename, std::vector<char*>& input_names, std::vector<uint*>& inputs, size_t* wlsize, size_t* wgsize, char*& output) {
    #define IN "@Input:"
    #define OUT "@Output:"
    #define CONFIG "@Config:"

    std::ifstream asmin(filename);
    if (!asmin.good()) {
        std::cerr << "Failed to open file:" << filename << std::endl;
        exit(EXIT_FAILURE);
    }
    size_t li, s, ic = 0;
    std::string line;
    std::getline(asmin, line);
    while(line.find("@") != std::string::npos){
        if ((li = line.find(IN)) != std::string::npos) {
            size_t p = line.find("%");
            s = line.find(" ", p) - p;
            input_names.push_back(new char[s+1]);
            strncpy(input_names[ic++], &line[p], s);

            size_t lb, rb, il = 1;
            li = line.find("=");
            bool is_array = (lb = line.find("{", li) != std::string::npos);

            if (is_array) {
                if ((rb = line.find("}", lb)) == std::string::npos) {
                    std::cerr << "Invalid input row in file:" << filename << std::endl << line << std::endl;
                    return -1;
                }
                il += std::count(line.begin()+lb, line.begin()+rb, ','); //Input length
            }

            uint* input = new uint[il + 1];
            input[0] = is_array * il;
            for (int i=1; i < il+1; i++) {
                while (!std::isdigit(line[li])) 
                    li++;
                input[i] = atoi(&line[li]);
                while (std::isdigit(line[li]))
                    li++;
            }
            inputs.push_back(input);
        }

        else if ((li = line.find(OUT)) != std::string::npos) {
            li += sizeof(OUT);
            s = 1 + line.length() - li;
            output = new char[s];
            strncpy(output, &line[li], s);
            output[s] = '\0';
        }

        else if ((li = line.find(CONFIG)) != std::string::npos) {
            li += sizeof(CONFIG);
            for (int i=0; i < 3; i++) {
                while (!std::isdigit(line[li]))
                    li++;
                size_t d = atol(&line[li]);
                wlsize[i] = d;
                wgsize[i] = d;
                
                while (std::isdigit(line[li]))
                    li++;
            }
        }
        else
            std::cerr << "Invalid input row in file:" << filename << std::endl << line << std::endl;
        std::getline(asmin, line);
    }
    asmin.close();
    return 0;
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
    char* output;
    std::vector<char*> arg_names;

    parseInput(asmfile, arg_names, inputs, wlsize, wgsize, output);

    /* Code for checking parsing
    for (int i=0; i<inputs.size(); i++) {
        std::cout << arg_names[i] << std::endl;
        for (int j=0; j<=inputs[i][0]; j++) {
            std::cout << inputs[i][j];
        } std::cout << std::endl;
    }
    for (int i=0; i<3; i++) {
        std::cout << wlsize[i] << " ";
        std::cout << wgsize[i] << std::endl;
    }
    */

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

    /* Code for getting kernel arg info from the kernel object.
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
    */

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
    ret = clEnqueueNDRangeKernel(q, kernel, 3, NULL, wgsize, wlsize, 0, NULL, NULL);
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
    for (int i=0; i<arg_names.size(); i++) {
        delete[] arg_names[i];
        delete[] inputs[i];
    }
}