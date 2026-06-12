// clspv mp.cl --cl-std=CL2.0 --inline-entry-points --spv-version=1.6
// spirv-dis a.spv > mp.spvasm

__kernel void test(global atomic_uint* flag, global uint* data, global uint* r0, global uint* r1) {
    if (get_global_id(0) == 0) {
        *data = 1;
        atomic_store_explicit(flag, 1, memory_order_relaxed);
    } else {
        *r0 = atomic_load_explicit(flag, memory_order_relaxed);
        *r1 = *data;
    }
}