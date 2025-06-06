#include "utils.hpp"

void print_error(const string& description) {
    cerr << "ERROR: " << description << endl;
}

string to_proper_rational(double val) {
    static const size_t buff_len = 1000; 
    static char buffer[buff_len];
    
    auto res = to_chars(buffer, buffer + buff_len, val);
    return string(buffer, (size_t) (res.ptr - buffer));
}

int64_t get_arg(char arg, map<char, char*> &args, int64_t def, int64_t min, int64_t max) {
    if (!args.contains('p')) {
        return def;
    }
    char* str = args['p'];
    size_t len = strlen(str);
    int64_t res = 0;
    

    auto arg_error = [&]() {
        print_error(string("Invalid argument -") + arg + " value: " + str + ".");
    };

    for (size_t i = 0; i < len; ++i) {
        if (str[i] < '0' or str[i] > '9') {
            arg_error();
            return -1;
        }
        res = res * 10 + str[i] - '0';
        if (res > max) {
            arg_error();
            return -1;
        }
    }
    if (res < min) {
        arg_error();
        return -1;
    }
    return res;
}


bool is_id_valid(const string& id) {
    for (size_t i = 0; i < id.size(); ++i) {
        bool is_digit = id[i] >= '0' and id[i] <= '9';
        bool is_letter = id[i] >= 'a' and id[i] <= 'z';
        bool is_upper_letter = id[i] >= 'A' and id[i] <= 'Z';
        if (!(is_digit or is_letter or is_upper_letter)) {
            return false; // Invalid character in ID
        }
    }
    return true; // ID is valid
}

int install_signal_handler(int signal, void (*handler)(int), int flags) {
    struct sigaction action;
    sigset_t block_mask;

    if(sigemptyset(&block_mask) < 0) {
        print_error("Creating empty signal mask failed!");
        return -1;
    }
    action.sa_handler = handler;
    action.sa_mask = block_mask;
    action.sa_flags = flags;

    if (sigaction(signal, &action, NULL) < 0) {
        print_error("Installing sigaction failed!");
        return -1;
    }
    return 0;
}
