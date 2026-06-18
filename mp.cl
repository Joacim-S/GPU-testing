// clspv mp.cl --cl-std=CL2.0 --inline-entry-points --spv-version=1.6
// spirv-dis a.spv > mp.spvasm

static void do_stress(__global uint* scratchpad, uint group_id, uint* locations) {
    for (uint i=0; i<1000; i++) {
        scratchpad[locations[group_id]] = i;
        scratchpad[locations[group_id]] = i + 1;
    }
}

__kernel void test(global atomic_uint* flag, global uint* data, global uint* r0, global uint* r1, __global uint* scratchpad, __global uint* locations) {
    uint group_id = get_group_id(0);
    uint id = get_local_size(0) * (group_id / 3) + get_local_id(0);
    if (group_id % 3 == 1) {
        data[id] = 1;
        atomic_store_explicit(&flag[id], 1, memory_order_relaxed);
    } else if (group_id % 3 == 2) {
        r0[id] = atomic_load_explicit(&flag[id], memory_order_relaxed);
        r1[id] = data[id];
    }
    else do_stress(scratchpad, (group_id/3), locations);
}   
