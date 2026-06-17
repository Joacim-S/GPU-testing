// clspv mp.cl --cl-std=CL2.0 --inline-entry-points --spv-version=1.6
// spirv-dis a.spv > mp.spvasm

static void do_stress(__global uint* scratchpad, uint id, uint* locations) {
    for (uint i=0; i<1000; i++) {
        scratchpad[locations[get_global_id(0)]] = i;
        scratchpad[locations[get_global_id(0)]] = i + 1;
    }
}

__kernel void test(global atomic_uint* flag, global uint* data, global uint* r0, global uint* r1, __global uint* scratchpad, __global uint* locations) {
    uint gid = get_group_id(0);
    uint id = gid / 3 + get_local_id(0);
    if (gid % 3 == 1) {
        data[id] = 1;
        atomic_store_explicit(&flag[id], 1, memory_order_relaxed);
    }
    if (gid % 3 == 2) {
        r0[id] = atomic_load_explicit(&flag[id], memory_order_relaxed);
        r1[id] = *data;
    }
    else do_stress(scratchpad, id, locations);
}
