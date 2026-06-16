// clspv mp.cl --cl-std=CL2.0 --inline-entry-points --spv-version=1.6
// spirv-dis a.spv > mp.spvasm
// Get groupid and define target workgroup so threads don't write to own wg
// atom uint vs uint? assigning vs store_explicit?

static void do_stress(__global uint* scratchpad, uint id) {
    for (int i=0; i<10000; i++) {
        scratchpad[id] = i;
        scratchpad[id] = i + 1;
    }
}

__kernel void test(global atomic_uint* flag, global uint* data, global uint* r0, global uint* r1, __global uint* scratchpad) {
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
    else do_stress(scratchpad, id);
}
