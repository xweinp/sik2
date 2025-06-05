#pragma once

#include <stdexcept>
#include <map>
#include <cstdint>
#include <cstring>
#include <signal.h>
#include <chrono>

using namespace std;
using namespace std::chrono;

uint64_t get_arg(char arg, map<char, char*> &args, uint64_t def, uint64_t min, uint64_t max) {
    if (!args.contains('p')) {
        return def;
    }
    char* str = args['p'];
    size_t len = strlen(str);
    uint64_t res = 0;

    for (int i = 0; i < len; ++i) {
        if (str[i] < '0' or str[i] > '9') {
            throw invalid_argument(string("invalid ") + arg + " - " + str + ".");
        }
        res = res * 10 + str[i] - '0';
        if (res > max) {
            throw invalid_argument(string("argument ") + str + "for option " + arg + "is too big.");
        }
    }
    if (res < min) {
        throw invalid_argument(string("argument ") + str + "for option " + arg + "is too small.");
    }
    return res;
}

void install_signal_handler(int signal, void (*handler)(int), int flags) {
    struct sigaction action;
    sigset_t block_mask;

    sigemptyset(&block_mask);
    action.sa_handler = handler;
    action.sa_mask = block_mask;
    action.sa_flags = flags;

    if (sigaction(signal, &action, NULL) < 0) {
        throw runtime_error("sigaction failed!");
    }
}

bool is_id_valid(const string& id) {
    for (int i = 0; i < id.size(); ++i) {
        bool is_digit = id[i] >= '0' and id[i] <= '9';
        bool is_letter = id[i] >= 'a' and id[i] <= 'z';
        bool is_upper_letter = id[i] >= 'A' and id[i] <= 'Z';
        if (!(is_digit or is_letter or is_upper_letter)) {
            return false; // Invalid character in ID
        }
    }
    return true; // ID is valid
}

