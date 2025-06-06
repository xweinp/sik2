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
    
    return is_id_valid(id);
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

int Client::set_port_and_ip() {
    if (addr.ss_family == AF_INET) { //ipv4
        const sockaddr_in *addr4 = (sockaddr_in*)(&addr);
        const in_addr *ia = (in_addr*)(&addr4->sin_addr);
        char buff[INET_ADDRSTRLEN] = {0};

        if (!inet_ntop(AF_INET, ia, buff, sizeof(buff))) {
            print_error("inet_ntoa exited with error.");
            return -1;
        }
        ip = string(buff);
        port = ntohs(addr4->sin_port);
        return 0;
    }
    else {
        const sockaddr_in6 *addr6 = (sockaddr_in6*)(&addr);
        const in6_addr *ia = (in6_addr*)(&addr6->sin6_addr);
        char buff[INET6_ADDRSTRLEN] = {0};

        if (!inet_ntop(AF_INET6, ia, buff, sizeof(buff))) {
            print_error("inet_ntoa exited with error.");
            return -1;
        }
        ip = string(buff);
        port = ntohs(addr6->sin6_port);
        return 0;
    }
}

void Client::read_message(const string& msg, int k, ifstream &file) {
    // every message ends with "\r\n"
    size_t old_len = buffered_message.size();
    buffered_message += msg;
    size_t erase_pref = 0;
    vector<string> messages;

    for (size_t i = old_len > 0? old_len - 1: 0; i + 1 < buffered_message.size(); ++i) {
        if (buffered_message[i] == '\r' and buffered_message[i + 1] == '\n') {
            // Found end of a message.
            messages.push_back(buffered_message.substr(erase_pref, i + 2 - erase_pref));
            erase_pref = i + 2; // Erase everything before this point.
        }
    }
    buffered_message.erase(0, erase_pref);

    if (messages.empty()) {
        if (buffered_message.empty()) {
            started_before_reply = false;
        }
        else if (messages_to_send.empty()) {
            // We have not sent any replies yet.
            started_before_reply = true;
        }
        started_before_reply |= !(buffered_message.empty() or messages_to_send.empty());
        return;
    }
    // There is at lest one full message
    const string& first_message = messages[0];
    
    if (!messages_to_send.empty()) {
        // We have not sent all replies yet.
        started_before_reply = true;
    }

    if (!helloed) {
        // First message must be a HELLO.
        if (!proper_hello(first_message)) {
            print_error_bad_message(first_message);
            // TODO: zamykam to połączenie! potwierdzone z forum.
        }
        else {
            // First message is a proper HELLO.
            id = id_from_hello(first_message);
            n_small_letters = get_n_small_letters(id);
            helloed = true;
                
            cout << to_string_wo_id() << " is now known as " << id << "." << endl;            
            messages_to_send.push(make_coeff(file), 0);
        }
    }
    else if (!is_put(first_message)) {
        // This is not even a proper PUT message.
        // I just print ERROR and ignore it.
        print_error_bad_message(first_message);
        started_before_reply = false; // Reset the flag.
    }
    else if (started_before_reply) {
        // First message started before we got a reply.
            
        // First message is a PUT message.
        // It was sent before the player received a reply so I resend penalty.
        auto [point, value] = get_point_value(first_message);
        messages_to_send.push(make_penalty(point, value), 0);
        // If the message is a bad put then we also send bad_put.
        if (is_bad_put(point, value, k)) {
            print_error_bad_message(msg);
            messages_to_send.push(make_bad_put(point, value), 1);
        }
        started_before_reply = false; // Reset the flag.
    }
    else {
        // First message is a PUT message.
        // If the message is a bad put then we also send bad_put.
        auto [point, value] = get_point_value(first_message);
        if (is_bad_put(point, value, k)) {
            print_error_bad_message(msg);
            messages_to_send.push(make_bad_put(point, value), 1);
        }
        else {
            ++n_proper_puts;
            messages_to_send.push(make_state(), n_small_letters);
        }
    }

    for (int i = 1; i < messages.size(); ++i) {
        
        const string& msg = messages[i];
        if (!is_put(msg)) {
            // This is not even a proper PUT message.
            // I just print ERROR and ignore it.
            print_error_bad_message(msg);
            continue; // Ignore this message.
        }
        auto [point, value] = get_point_value(msg);
        if (is_bad_put(point, value, k)) {
            print_error_bad_message(msg);
            messages_to_send.push(make_bad_put(point, value), 1);
        }
        if (messages_to_send.empty()) {
            // If the message is a bad put then we also send bad_put.
            ++n_proper_puts;
            messages_to_send.push(make_state(), n_small_letters);
        }
        else {
            // We have already sent a reply.
            messages_to_send.push(make_penalty(point, value), 0);
        }
    }
    if (!buffered_message.empty() and !messages_to_send.empty()) {
        // We have some buffered message and we have sent a reply.
        started_before_reply = true;
    }
}

int Pollvec::set_up() {
    int socket_fd = ipv6_enabled_sock(listen_port);
    if (socket_fd < 0) {
        socket_fd = ipv4_only_sock(listen_port);
    }
    if (socket_fd < 0) {
        print_error("Cannot create listening socket.");
        return -1;
    }
    if (fcntl(socket_fd, F_SETFL, O_NONBLOCK)) {
        close(socket_fd);
        print_error("Cannot set listening socket to non-blocking mode.");
        return -1;
    }
    pollfd listen_fd{};
    listen_fd.fd = socket_fd;
    listen_fd.events = POLLIN;
    listen_fd.revents = 0;
    pollfds.push_back(listen_fd);

    return 0;
}

int Server::set_up() {
    if (pollvec.set_up()) {
        // polvec prints error
        return -1; 
    }
    file.open(filename);
    if (!file) {
        print_error("Cannot open file: " + string(filename));
        return -1;
    }
    return 0;
}

void Server::accept_new_connection() {
    auto &listen_pollfd = pollvec[0];
    if ((listen_pollfd.revents & POLLIN) == 0) {
        return;
    }
    // TODO: limit wielkosci tablicy poll!

    listen_pollfd.revents = 0; // Reset revents for the next poll.

    Client client;
    client.addr_len = sizeof(client.addr);
    client.fd = accept(
        listen_pollfd.fd, 
        (sockaddr*)&client.addr, 
        &client.addr_len
    );
    client.connected_timestamp = steady_clock::now();

    if (client.fd < 0) {
        print_error("Cannot accept new connection. errno: " + to_string(errno));
        return;
    }
    if (fcntl(client.fd, F_SETFL, O_NONBLOCK)) {
        close(client.fd);
        print_error("Cannot set client socket to non-blocking mode. errno: " + to_string(errno));
        return;
    }
    if (client.set_port_and_ip()) {
        return;
    }

    cout << "New client " << client.ip << ":" << client.port << "." << endl;

    pollfd client_fd_struct{};
    client_fd_struct.fd = client.fd;
    client_fd_struct.events = POLLIN; 
    client_fd_struct.revents = 0;
    pollvec.add_client(client_fd_struct);

    players.add_player(client);
}
