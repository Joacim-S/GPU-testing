# Input = .cl file for dartagnan
# Output = file with stress for parallel execution. Result kernel in seperate file.
# Gather memory locations from kernel inputs.
# Replace id == x with id % max(x) == y?
# Use id = get_local_size(0) * (group_id / max(x)) + get_local_id(0) to have duplicate ids
# Have x[id] r0[id] and so on
# Else == prev(x) + 1 = max(x)?

import argparse

class KernelArg():
    def __init__(self, glob: bool, typ: str = '', name: str = ''):
        self.glob = glob
        self.type = typ
        self.name = name
    
    def __str__(self):
            return f'{self.name} global: {self.glob} type: {self.type}'
    
class Kernel():
    def __init__(self, name: str, args: list[KernelArg] = []):
        self.name = name
        self.args = args

    def __str__(self):
        return f'{self.name}\n{'\n'.join([str(arg) for arg in self.args])}'

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument('filename')
    parser.add_argument('-o', '--output')
    args = parser.parse_args()
    return args

def parse_kernel_row(row: str) -> Kernel:
    arg_objs = []
    void = 'void'
    name = row[row.find(void) + len(void) : row.find('(')].strip()
    kernel_args = [arg.strip() for arg in row[1 + row.find('(') : row.find(')')].split(',')]
    for arg in kernel_args:
        arg = [a.strip() for a in arg.split(' ') if a.strip()]
        if arg[0] in ('global', '__global'):
            arg_obj = KernelArg(True)
            arg.pop(0)
        else:
            arg_obj = KernelArg(False)
        if arg[1].startswith('*'):
            arg[0] += '*'
            arg_obj.name = arg[1][1:]
            if not arg_obj.name:
                arg_obj.name = arg[2]
        else:
            arg_obj.name = arg[1]
        arg_obj.type = arg[0]
        arg_objs.append(arg_obj)
    return Kernel(name, arg_objs)

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
    kernels = []

    with open(args.filename, 'r') as f:
        for row in f:
            r = row.strip()
            if r.startswith('__kernel'):
                output += row
                kernels.append(parse_kernel_row(r))
                continue
            
    print(kernels[0])


if __name__ == '__main__':
    main()