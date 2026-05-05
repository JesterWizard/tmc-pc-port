#include "util.h"
#include <algorithm>
#include <iostream>
#include <fmt/format.h>

#ifdef _WIN32
#include <process.h>
#endif

#ifndef _WIN32
static std::string quote_cmd_argument(const std::string& arg) {
    std::string normalized = arg;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    std::string escaped;
    for (char c : normalized) {
        if (c == '"') {
            escaped += '\\';
        }
        escaped += c;
    }

    return std::string("\"") + escaped + "\"";
}
#endif

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
        return fmt::format(format, value);
    }
    return "";
}
