#include "utils-server.hpp"

int ipv6_enabled_sock(uint16_t port) {
    int socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        return -1; // Cannot create socket
    }

    int off = 0;
    int res = setsockopt(
        socket_fd, 
        IPPROTO_IPV6, 
        IPV6_V6ONLY, 
        &off,
        sizeof(off)
    );

    if (res < 0) {
        close(socket_fd);
        return -1; // Cannot set IPV6_V6ONLY option
    }

    sockaddr_in6 addr6{};
    addr6.sin6_family = AF_INET6;
    addr6.sin6_addr = in6addr_any;
    addr6.sin6_port = htons(port);

    res = bind(
        socket_fd,
        (sockaddr*)&addr6,
        sizeof(addr6)
    );

    if (res < 0) {
        close(socket_fd);
        return -1; // Cannot bind socket
    }

    res = listen(socket_fd, SOMAXCONN);

    if (res < 0) {
        close(socket_fd);
        return -1; // Cannot listen on socket
    }
    
    return socket_fd;
}

int ipv4_only_sock(uint16_t port) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd < 0) {
        return -1; // Cannot create socket
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    int res = bind(
        socket_fd,
        (sockaddr*)&addr,
        sizeof(addr)
    );

    if (res < 0) {
        close(socket_fd);
        return -1; // Cannot bind socket
    }

    res = listen(socket_fd, SOMAXCONN);

    if (res < 0) {
        close(socket_fd);
        return -1; // Cannot listen on socket
    }
    
    return socket_fd;
}

bool proper_hello(const string& msg) {
    bool pref_suf =  msg.size() > 8 and 
        msg.substr(0, 6) == "HELLO " and
        msg.back() == '\n' and 
        msg[msg.size() - 2] == '\r';

    if (!pref_suf) {
        return false; // Invalid HELLO message format
    }

    string id = id_from_hello(msg);
    
    for (int i = 0; i < id.size(); ++i) {
        bool is_digit = id[i] >= '0' and id[i] <= '9';
        bool is_letter = id[i] >= 'a' and id[i] <= 'z';
        bool is_upper_letter = id[i] >= 'A' and id[i] <= 'Z';
        if (!(is_digit or is_letter or is_upper_letter)) {
            return false; // Invalid character in ID
        }
    }
    return true; // Valid HELLO message 
}

bool is_integer(const string& str) {
    if (str.empty()) 
        return false;
    for (char c: str) {
        if (!isdigit(c)) {
            return false;
        }
    }
    return true;
}
bool is_proper_rational(const string& str) {
    if (str.empty()) 
        return false;
    int i = 0;
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
    for (int j = i + 1; j < str.size(); ++j) {
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

tuple<string, string> get_point_value(const string& msg) {
    // This function assumes that the message is a PUT message checked by is_put.
    size_t point_len = msg.find(' ', 4) - 4;
    string point = msg.substr(4, point_len);
    
    size_t value_start = 4 + point_len + 1;
    size_t value_len = msg.size() - value_start - 2; 
    string value = msg.substr(value_start, value_len);
    
    return {point, value};
}

bool is_put(const string& msg) {
    bool pref_suf =   msg.size() > 9 and 
        msg.substr(0, 4) == "PUT " and 
        msg.substr(msg.size() - 2, 2) == "\r\n";
    
    if (!pref_suf) {
        return false;
    }
    size_t point_len = msg.find(' ', 4) - 4;
    string point = msg.substr(4, point_len);
    
    size_t value_start = 4 + point_len + 1;
    size_t value_len = msg.size() - value_start - 2; 
    if (value_len < 1) {
        return false; 
    }
    string value = msg.substr(value_start, value_len);
    if (!is_integer(point) or !is_proper_rational(value)) {
        return false;
    }
    return true;
}

// I assume that the integer non-negative
int get_int(const string& msg, int mx) {
    int64_t res = 0;
    for (size_t i = 0; i < msg.size(); ++i) {
        res = res * 10 + (msg[i] - '0');
        if (res > mx) {
            return -1; 
        }
    }
    return res;
}

double get_double(const string& msg) {
    bool negative = false;
    double res = 0;
    int i = 0;
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
    

// This function assumes that the message is a PUT message checked by is_put.
bool is_bad_put(const string& point, const string& value, unsigned int k) {
    int point_int = get_int(point, k);
    if (point_int < 0) {
        return true; 
    }
    double value_double = get_double(value);
    if (value_double < -5 or value_double > 5) {
        return true; 
    }
    return false;
}

string id_from_hello(const string& msg) {
    return msg.substr(6, msg.size() - 8); // Remove "HELLO " and "\r\n"
}

size_t get_n_small_letters(const string& str) {
    size_t count = 0;
    for (char c: str) {
        if (c >= 'a' and c <= 'z') {
            ++count;
        }
    }
    return count;
}