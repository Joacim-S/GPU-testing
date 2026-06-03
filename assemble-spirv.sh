#!/bin/bash
# Assembles the spvasm files. Requires spirv-tools.
mkdir spirv/spv
cp -r spirv/spvasm/* spirv/spv/

find spirv/spv -type f -name '*.spvasm' -exec bash -c '
  for file; do
    # Compile directly to the .spv extension, then remove the source .spvasm copy
    spirv-as "$file" -o "${file%.spvasm}.spv" && rm "$file"
  done
' _ {} +
