#include "utils.hpp"

string to_proper_rational(double val) {
    static const size_t buff_len = 1000; 
    static char buffer[buff_len];
    
    auto res = to_chars(buffer, buffer + buff_len, val);
    return string(buffer, res.ptr - buffer);
}
