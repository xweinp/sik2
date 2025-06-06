#include <charconv>

#include "utils.hpp"

void print_error(const string& description) {
    cerr << "ERROR: " << description << endl;
}


string to_proper_rational(double val) {
    static const size_t buff_len = 1000; 
    static char buffer[buff_len];
    
    auto res = to_chars(
        buffer, 
        buffer + buff_len, 
        val, std::chars_format::fixed, 
        7
    );
    
    return string(buffer, (size_t) (res.ptr - buffer));
}

int64_t get_arg(char arg, map<char, char*> &args, int64_t def, int64_t min, int64_t max) {
    if (!args.contains(arg)) {
        return def;
    }
    char* str = args[arg];
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

bool is_proper_rational(const string& str) {
    if (str.empty()) 
        return false;
    size_t i = 0;
    if (str[0] == '-') {
        i = 1; // Skip the minus sign
    }
    for (; i < str.size(); ++i) {
        if (str[i] == '.') {
            ++i;
            break;
        }
        if (!isdigit(str[i])) {
            return false;
        }
    }
    if (i == str.size())
        return true;
    for (size_t j = i + 1; j < str.size(); ++j) {
        if (!isdigit(str[j])) {
            return false;
        }
        if (j - i > 7) {
            // More than 7 digits after the decimal point
            return false;
        }
    }
    return true;
}

vector<double> parse_coefficients(string &coeff_str) {
    vector<double> coeffs;

    coeff_str.pop_back();
    coeff_str.pop_back(); // Remove "\r\n"
    for(size_t i = coeff_str.find(' ') + 1; i < coeff_str.size();) {
        size_t next_space = coeff_str.find(' ', i);
        string coeff = coeff_str.substr(i, next_space - i);
        coeffs.push_back(get_double(coeff));
        i = next_space + 1;
        if (i == 0) {
            break;
        }
    }
    coeff_str += "\r\n"; // Restore "\r\n"
    return coeffs;
}

double get_double(const string& msg) {
    bool negative = false;
    double res = 0;
    size_t i = 0;
    if (msg[0] == '-') {
        negative = true;
        i = 1; 
    }
    for (; i < msg.size(); ++i) {
        if (msg[i] == '.') {
            ++i; 
            break;
        }
        res = res * 10 + (msg[i] - '0');
    }
    double multiplier = 0.1;
    for (; i < msg.size(); ++i) {
        res += (msg[i] - '0') * multiplier;
        multiplier *= 0.1;
    }
    if (negative) {
        res = -res;
    }
    return res;
}

bool is_valid_scoring(const string& msg) {
    if (msg.size() <= 10 || msg.substr(0, 8) != "SCORING ") {
        return false;
    }
    if (msg.substr(msg.size() - 2, 2) != "\r\n") {
        return false; // Must end with "\r\n"
    }
    return true;
}

// I assume that the integer non-negative
int64_t get_int(const string& msg, int64_t mx) {
    int64_t res = 0;
    for (size_t i = 0; i < msg.size(); ++i) {
        if (msg[i] < '0' || msg[i] > '9') {
            return -1; // Invalid character for integer
        }
        res = res * 10 + (msg[i] - '0');
        if (res > mx) {
            return -1; 
        }
    }
    return res;
}
