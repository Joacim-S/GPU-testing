# Input = .cl file for dartagnan
# Output = file with stress for parallel execution. Result kernel in seperate file.
# Gather memory locations from kernel inputs.
# Replace id == x with id % max(x) == y?
# Use id = get_local_size(0) * (group_id / max(x)) + get_local_id(0) to have duplicate ids
# Have x[id] r0[id] and so on
# Else == prev(x) + 1 = max(x)?

