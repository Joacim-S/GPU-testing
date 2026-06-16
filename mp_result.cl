__kernel void results(global uint* r0, global uint* r1, __global atomic_uint* res) {
    uint id = get_global_id(0);
    uint res_id = 2 * r1[id] + r0[id];
    atomic_fetch_add(&res[res_id], 1);
}