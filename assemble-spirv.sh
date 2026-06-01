#!/bin/bash
# Assembles the spvasm files. Requires spirv-tools.
mkdir spirv/spv
cp -r spirv/spvasm/* spirv/spv/
find spirv/spv -type f -exec spirv-as {} -o {} \; -exec rename .spvasm .spv {} \;
