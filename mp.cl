// clspv mp.cl --cl-std=CL2.0 --inline-entry-points --spv-version=1.6
// spirv-dis a.spv > mp.spvasm

static void do_stress(__global uint* scratchpad, uint group_id, __global uint* locations, uint iterations) {
    for (uint i=0; i<iterations; i++) {
        uint tmp1 = scratchpad[locations[group_id]];
        if (tmp1 > 100) {
            scratchpad[locations[group_id]] = i;
            break;
        }
        scratchpad[locations[group_id]] = tmp1 + 1;
        uint tmp2 = scratchpad[locations[group_id]];
        if (tmp2 > 100) {
            scratchpad[locations[group_id]] = i;
            break;
        }
    }
}

__kernel void test(global atomic_uint* flag, global uint* data, global uint* r0, global uint* r1, __global uint* scratchpad, __global uint* locations) {
    uint group_id = get_group_id(0);
    uint id = get_local_size(0) * (group_id / 3) + get_local_id(0);
    do_stress(scratchpad, group_id/3, locations, 128);
    if (group_id % 3 == 1) {
        data[id] = 1;
        atomic_store_explicit(&flag[id], 1, memory_order_relaxed);
    } else if (group_id % 3 == 2) {
        r0[id] = atomic_load_explicit(&flag[id], memory_order_relaxed);
        r1[id] = data[id];
    }
    else do_stress(scratchpad, (group_id/3), locations, id);
}
