#include <charconv> 
#include "utils-server.hpp"

// TODO: parsowanie liczb z tekstu chyba nie uwzglednia braku spracji przed \r\n
// TODO: jak dajesz penalty to doliczaj do errora!

int ipv6_enabled_sock(uint16_t port) {
    int socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        return -1; // Cannot create socket
    }

    int opt = 1;
    int res= setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (res < 0) {
        close(socket_fd);
        return -1; // Cannot set SO_REUSEADDR
    }


    int off = 0;
    res = setsockopt(
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

    int opt = 1;
    int res= setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (res < 0) {
        close(socket_fd);
        return -1; // Cannot set SO_REUSEADDR
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    res = bind(
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

string id_from_hello(const string& msg) {
    return msg.substr(6, msg.size() - 8); // Remove "HELLO " and "\r\n"
}

size_t get_no_small_letters(const string& str) {
    size_t count = 0;
    for (char c: str) {
        if (c >= 'a' and c <= 'z') {
            ++count;
        }
    }
    return count;
}

string make_penalty(const string &point, const string &value) {
    return "PENALTY " + point + " " + value + "\r\n"; 
}
string make_bad_put(const string &point, const string &value) {
    return "BAD_PUT " + point + " " + value + "\r\n";
}
string make_coeff(ifstream &file) {
    string res;
    getline(file, res);
    res += '\n';
    return res;
}
string make_state(const vector<double> &approx) {
    string res = "STATE";
    for (double val : approx) {
        res += " " + to_proper_rational(val);
    }
    res += "\r\n";
    return res;
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

tuple<string, string> get_point_and_value(const string& msg) {
    // This function assumes that the message is a PUT message checked by is_put.
    size_t point_len = msg.find(' ', 4) - 4;
    string point = msg.substr(4, point_len);
    
    size_t value_start = 4 + point_len + 1;
    size_t value_len = msg.size() - value_start - 2; 
    string value = msg.substr(value_start, value_len);
    
    return {point, value};
}

bool is_put(const string& msg) {
    bool pref_suf = msg.size() > 8 and 
        msg.substr(0, 4) == "PUT " and 
        msg.substr(msg.size() - 2, 2) == "\r\n";
    if (!pref_suf) {
        return false;
    }
    size_t sep = msg.find(' ', 4);
    if (sep == string::npos) {
        return false;
    }

    size_t point_len = sep - 4;
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



    
// This function assumes that the message is a PUT message checked by is_put.
bool is_bad_put(const string& point, const string& value, int32_t k) {
    int64_t point_int = get_int(point, (int64_t) k);
    if (point_int < 0) {
        return true; 
    }
    double value_double = get_double(value);
    if (value_double < -5 or value_double > 5) {
        return true; 
    }
    return false;
}



// MessageQueue

void MessageQueue::push(const string& msg, uint64_t delay_s) {
    auto now = steady_clock::now();
    auto time_to_send = now + seconds(delay_s);
    messages.push({time_to_send, msg});
}
void MessageQueue::get_current() {
    current_message = messages.top().second;
    current_pos = 0;
    messages.pop();
}
bool MessageQueue::currently_sending() const {
    return !current_message.empty();
}
bool MessageQueue::empty() const {
    return current_message.empty() and messages.empty();
}
bool MessageQueue::ready_message() const {
    if (!current_message.empty()) {
        return true; // We are currently sending a message.
    }
    if (messages.empty()) {
        return false;
    }
    auto now = steady_clock::now();
    return messages.top().first <= now;
}
// Returns: -1 iff error, 1 iff the whole message was sent, 0 otherwise
int MessageQueue::send_message(int socket_fd) {
    if (!currently_sending()) {
        get_current();
    }

    ssize_t sent_len = write(
        socket_fd, 
        current_message.data() + current_pos,
        current_message.size() - current_pos
    );
    
    if (sent_len < 0) {
        print_error("Cannot send message to client. errno: " + to_string(errno));
        return -1;
    }
    current_pos += (size_t) sent_len;
    if (current_pos == current_message.size()) {
        // We have sent the whole message.
        current_pos = 0;
        current_message = "";
        return 1;
    }
    return 0;
}
TimePoint MessageQueue::get_ready_time() const {
    if (!current_message.empty()) {
        return steady_clock::now(); // If we are currently sending a message, return now.
    }
    else if (!messages.empty()) {
        return messages.top().first; 
    }
    else {
        return steady_clock::time_point::max();
    }
}
void MessageQueue::send_scoring(const string &scoring, int socket_fd) {
    if (current_message.empty()) {
        current_message = scoring;
        current_pos = 0;
    }
    else {
        current_message += scoring;
    }
    send_message(socket_fd);
}



// Player

int Player::set_port_and_ip() {
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

int Player::read_message(const string& msg, ifstream &file) {
    int32_t k = (int32_t) approx.size() - 1;

    int res = 0;
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
        return 0;
    }
    // There is at lest one full message
    const string& first_message = messages[0];
    
    for (string s: messages) {
        cerr << "Received message: " << s << endl;
    }
    if (!messages_to_send.empty()) {
        // We have not sent all replies yet.
        started_before_reply = true;
    }

    if (!helloed) {
        // First message must be a HELLO.
        if (!proper_hello(first_message)) {
            cerr << 'a' << endl;
            print_error_bad_message(first_message);
            return -1;
        }
        else {
            // First message is a proper HELLO.
            id = id_from_hello(first_message);
            n_small_letters = get_no_small_letters(id);
            helloed = true;
            cout << to_string_wo_id() << " is now known as " << id << "." << endl;
            string coeff = make_coeff(file);
            calc_goal_from_coef(coeff);
            messages_to_send.push(coeff, 0);
        }
    }
    else if (!is_put(first_message)) {
        // This is not even a proper PUT message.
        // I just print ERROR and ignore it.
        cerr << 'b' << endl;
        print_error_bad_message(first_message);
        started_before_reply = false; // Reset the flag.
    }
    else if (started_before_reply) {
        // First message started before we got a reply.
            
        // First message is a PUT message.
        // It was sent before the player received a reply so I resend penalty.
        auto [point, value] = get_point_and_value(first_message);
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
        auto [point, value] = get_point_and_value(first_message);
        if (is_bad_put(point, value, k)) {
            print_error_bad_message(msg);
            messages_to_send.push(make_bad_put(point, value), 1);
        }
        else {
            // Update the approximation.
            ++n_proper_puts;
            update_approximation(point, value);
            messages_to_send.push(make_state(approx), n_small_letters);
            res = 1;
        }
    }

    for (size_t i = 1; i < messages.size(); ++i) {
        
        const string& msg_i = messages[i];
        if (!is_put(msg_i)) {
            // This is not even a proper PUT message.
            // I just print ERROR and ignore it.
            print_error_bad_message(msg_i);
            continue; // Ignore this message.
        }
        auto [point, value] = get_point_and_value(msg_i);
        if (is_bad_put(point, value, k)) {
            print_error_bad_message(msg_i);
            messages_to_send.push(make_bad_put(point, value), 1);
        }
        if (messages_to_send.empty()) {
            // If the message is a bad put then we also send bad_put.
            ++n_proper_puts;
            // Update the approximation.
            update_approximation(point, value);
            messages_to_send.push(make_state(approx), n_small_letters);
            res = 1;
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
    return res;
}

void Player::print_error_bad_message(const string& msg) {
    print_error(
        "bad message from " + 
        to_string_w_id() +
        ": " + msg
    );
}


void Player::calc_goal_from_coef(string &coeff) {
    size_t k = approx.size() - 1;
    goal.resize(k + 1);
    vector<double> coeffs = parse_coefficients(coeff);
    size_t n = coeffs.size();
    for (size_t i = 0; i <= k; ++i) {
        // Calculate the polynomial value at i
        double power = 1.0;
        double i_d = (double) i;
        for (size_t j = 0; j <= n; ++j) {
            goal[i] += coeffs[j] * power;
            power *= i_d;
        }
        error += goal[i] * goal[i]; // Sum of squares of the goal values
    }
}

void Player::update_approximation(const string &point, const string &value) {
    size_t k = approx.size() - 1;
    size_t point_int = (size_t) get_int(point, (int64_t) k);
    double value_double = get_double(value);
    
    // we add (approx + value_double - goal)^2 and subtract (approx - goal)^2
    // a^2 - b^2 = (a - b)(a + b)
    error += value_double * (value_double + 2 * (approx[point_int] - goal[point_int]));
    approx[point_int] += value_double;
}


// Pollvec

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


// Server

// returns 0 on success, -1 on fatal error
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

    Player client((size_t) n);
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

    cout << "New client [" << client.ip << "]:" << client.port << "." << endl;

    pollfd client_fd_struct{};
    client_fd_struct.fd = client.fd;
    client_fd_struct.events = POLLIN; 
    client_fd_struct.revents = 0;

    pollvec.add_client(client_fd_struct);
    players.add_player(client);
}


void Server::delete_client(size_t i) {
    counter_m -= players[i].n_proper_puts; 
    pollvec.delete_client(i);
    players.delete_client(i);
}



string Server::make_scoring() {
    vector<pair<string, double>> scoring;
    for (size_t i = 1; i < pollvec.size(); ++i) {
        scoring.push_back({
            players[i].id,
            players[i].error
        }); 
    }
    auto comp = [](const auto &a, const auto &b) {
        return a.first < b.first;
    };
    sort(scoring.begin(), scoring.end(), comp);

    string res = "SCORING";
    for (const auto &i: scoring) {
        res += " " + i.first + " " + to_string(i.second);
    }
    res += "\r\n";
    return res;
}

void Server::finish_game() {
    string scoring = make_scoring();
    for (size_t i = pollvec.size(); i; --i) {
        auto &client = players[i];
        if (!client.helloed) {
            delete_client(i);
            continue;
        }
        client.send_scoring(scoring);
        // TODO: jakis error jesli nie cale sie wyslalo?
        delete_client(i);
    }
}

void Server::play_a_game() {
    counter_m = 0;
    TimePoint next_event = steady_clock::now() + seconds(1);

    while (counter_m < m and !(*finish_flag)) {
        int timeout = max(0, (int)time_diff(steady_clock::now(), next_event));

        int poll_status = poll(
            pollvec.pollfds.data(), 
            (nfds_t) pollvec.size(),
            timeout
        );

        if (poll_status < 0) {
            print_error("Poll error occurred. errno: " + to_string(errno) + ".");
            if (*finish_flag) {
                return; 
            }
            continue; 
        }
        // Poll status >= 0. 
        // I can have some events POLLIN or POLLOUT.
        // I can also have timeout due to a messege I'm supposed to send right now.

        TimePoint new_next_event = steady_clock::now() + seconds(1);

        // First I accept new connection. (if there is any)
        accept_new_connection();
        pollvec[0].revents = 0; 
        
        for (size_t i = 1; i < pollvec.size(); ++i) {
            auto &pollfd = pollvec[i]; 
            auto &client = players[i];
            pollfd.events = POLLIN; // Reset events to POLLIN for the next poll 

            if ((pollfd.revents & (POLLIN | POLLERR))) {
                // read
                ssize_t read_len = read(
                    pollfd.fd,
                    buffer.data(),
                    buff_len
                );

                if (read_len < 0) {
                    print_error(
                        "Reading message from " + 
                        client.ip + ":" + to_string(client.port) + 
                        " result in error. Closing connection"
                    );
                    delete_client(i);
                    --i;
                    continue; 
                }
                else if (read_len == 0) {
                    cout << "Player " << client.to_string_w_id() << " disconnected." << endl;
                    delete_client(i);
                    --i;
                    continue; 
                }
                else {
                    string pom = buffer.substr(0, (size_t) read_len);
                    int read_res = client.read_message(pom, file);
                    if (read_res == - 1) {
                        delete_client(i);
                        --i;
                        continue;
                    }
                    else if (read_res == 1) {
                        // A proper PUT was made.
                        ++counter_m;
                        if (counter_m == m) {
                            finish_game();
                            return;
                        }
                    }
                }

            }
            if ((pollfd.revents & POLLOUT)) {
                while (client.has_ready_message_to_send()) {
                    
                    if (client.send_message() != 1) {
                        // It reutns 1 iff the whole message was sent.
                        break;
                    }
                }
            }
            pollfd.revents = 0;
            

            if (!client.helloed) {
                if (client.connected_timestamp + seconds(3) <= steady_clock::now()) {
                    cout << "Player " << client.to_string_w_id() 
                            << " did not send HELLO in time. Disconnecting." << endl;
                    delete_client(i);
                    --i;
                    continue; 
                }

                new_next_event = min(
                    new_next_event, 
                    client.connected_timestamp + seconds(3)
                );
            }   

            if (!client.messages_to_send.empty()) {
                if (client.has_ready_message_to_send()) {
                    // If there are messages to send, set POLLOUT
                    pollfd.events |= POLLOUT; 
                }
                pollfd.events |= POLLOUT; // Set POLLOUT if there are messages to send
                new_next_event = min(
                    new_next_event,
                    client.messages_to_send.get_ready_time() // First message to send
                );
            }    
        }

        next_event = new_next_event;
    }
}