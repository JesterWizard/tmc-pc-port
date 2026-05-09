#include "util.h"
#include "simple_format.h"
#include <iostream>
#include <iomanip>
#include <sstream>

void check_call(const std::vector<std::string>& cmd) {
#ifdef _WIN32
    std::vector<char*> args;
    args.reserve(cmd.size() + 1);
    for (auto& segment : cmd) {
        args.push_back(const_cast<char*>(segment.c_str()));
    }
    args.push_back(nullptr);

    int code = _spawnv(_P_WAIT, cmd[0].c_str(), args.data());
    if (code == -1) {
        std::cerr << "Failed to launch process: " << cmd[0] << std::endl;
        std::exit(1);
    }
    if (code != 0) {
        std::cerr << cmd[0] << " failed with return code " << code << std::endl;
        std::exit(code);
    }
#else
    std::string cmdstr;
    bool first = true;
    for (const auto& segment : cmd) {
        if (first) {
            first = false;
        } else {
            cmdstr += " ";
        }
        cmdstr += quote_cmd_argument(segment);
    }
    int code = system(cmdstr.c_str());
    if (code != 0) {
        std::cerr << cmdstr << " failed with return code " << code << std::endl;
        std::exit(1);
    }
#endif
}

std::string opt_param(const std::string& format, int defaultVal, int value) {
    if (value != defaultVal) {
        return assetfmt::Format(format, value);
    }
    return "";
}

std::string hex_u32(uint32_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::nouppercase << value;
    return out.str();
}
