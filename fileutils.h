#include <vector>
#include <fstream>
#ifndef FILEUTILS_H
#define FILEUTILS_H
constexpr char const *OUT = "@Output: ";
constexpr char const *IN = "@Input:";
constexpr char const *CONFIG = "@Config:";
constexpr char const *FILTER = "@Filter:";
int parseInput(std::string& asmpath, std::vector<char*>& input_names, std::vector<uint*>& inputs, size_t* wlsize, size_t* wgsize, char*& output);
int getAsmPath(std::string& spvpath, std::string& asmpath);
#endif
