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