#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>
#include <fstream>
#include "fileutils.h"

//Parses parameters for running the kernel and saves them to given pointers.
//Inputs is an array of pointers to arrays of uints starting with the length of the array.
//0 Length implies the value is a constant.
int parseInput(std::string& asmpath, std::vector<char*>& input_names, std::vector<uint*>& inputs, size_t* wlsize, size_t* wgsize, char*& output) {
    #define IN "@Input:"
    #define OUT "@Output:"
    #define CONFIG "@Config:"
    #define FILTER "@Filter:"

    std::ifstream asmin(asmpath);
    if (!asmin.good()) {
        std::cerr << "Failed to open file:" << asmpath << std::endl;
        exit(EXIT_FAILURE);
    }
    size_t li, s, ic = 0;
    std::string line;
    std::getline(asmin, line);
    while(line.find("@") != std::string::npos){
        if ((li = line.find(IN)) != std::string::npos) {
            size_t p = line.find("%");
            s = line.find(" ", p) - p;
            input_names.push_back(new char[s+1]);
            strncpy(input_names[ic], &line[p], s);
            input_names[ic++][s] = '\0';

            size_t lb, rb, il = 1;
            li = line.find("=");
            bool is_array = (lb = line.find("{", li) != std::string::npos);

            if (is_array) {
                if ((rb = line.find("}", lb)) == std::string::npos) {
                    std::cerr << "Invalid input row in file:" << asmpath << std::endl << line << std::endl;
                    return -1;
                }
                il += std::count(line.begin()+lb, line.begin()+rb, ','); //Input length
            }

            uint* input = new uint[il + 1];
            input[0] = is_array * il;
            for (int i=1; i < il+1; i++) {
                while (!std::isdigit(line[li])) 
                    li++;
                input[i] = atoi(&line[li]);
                while (std::isdigit(line[li]))
                    li++;
            }
            inputs.push_back(input);
        }

        else if ((li = line.find(OUT)) != std::string::npos) {
            li += sizeof(OUT);
            s = 1 + line.length() - li;
            output = new char[s];
            strncpy(output, &line[li], s);
            output[s] = '\0';
        }

        else if ((li = line.find(CONFIG)) != std::string::npos) {
            size_t config[3];
            li += sizeof(CONFIG);
            for (int i=0; i < 3; i++) {
                while (!std::isdigit(line[li]))
                    li++;

                config[i] = atol(&line[li]);
                
                while (std::isdigit(line[li]))
                    li++;
            }
            wlsize[0] = config[0] * config[1];
            wgsize[0] = wlsize[0] * config[2];
        }
        else if ((li = line.find(FILTER)) != std::string::npos) {
            //TODO
        }
        else
            std::cerr << "Invalid input row in file:" << asmpath << std::endl << line << std::endl;
        std::getline(asmin, line);
    }
    asmin.close();
    return 0;
}

//Forms the path to a corresponding spvasm file
int getAsmPath(std::string& spvpath, std::string& asmpath) {
    #define SPV_DIR "spirv/spv"
    #define SPVASM_DIR "spirv/spvasm/"
    size_t p;
    if ((p = spvpath.find(SPV_DIR)) == std::string::npos) {
        return -1;
    }
    p += sizeof(SPV_DIR);
    asmpath = SPVASM_DIR;
    asmpath.append(spvpath, p);
    asmpath += "asm";
    return 0;
}