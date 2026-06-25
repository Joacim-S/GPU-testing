# Input = .cl file for dartagnan
# Output = file with stress for parallel execution. Result kernel in seperate file.
# Gather memory locations from kernel inputs.
# Replace id == x with id % max(x) == y?
# Use id = get_local_size(0) * (group_id / max(x)) + get_local_id(0) to have duplicate ids
# Have x[id] r0[id] and so on
# Else == prev(x) + 1 = max(x)?

import argparse

class KernelArg():
    def __init__(self, glob: bool, typ: str, name: str):
        self.glob = glob
        self.type = typ
        self.name = name

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument('filename')
    parser.add_argument('-o', '--output')
    args = parser.parse_args()
    return args

def main():
    output = """
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

    """
    args = parse_args()
    arg_objs = set()
    with open(args.filename, 'r') as f:
        for row in f:
            r = row.strip()
            if r.startswith('__kernel'):
                output += row
                i = 0
                while r[i] != '(':
                    i += 1
                i += 1
                kernel_args = row[i:row.find(')')].split(', ')
                for a in kernel_args:
                    arg = a.split(' ')
                    if arg[0] == 'global':
                        arg_obj = KernelArg(True, arg[1], arg[2])
                        arg_objs.add(arg_obj)
    print(output)


if __name__ == '__main__':
    main()