#include <map>
#include <queue>
#include <math.h>

#include "utils.hpp"
#include "utils-client.hpp"

bool check_mandatory_option(const map<char, char*> &args, char option) {
    if (!args.contains(option)) {
        cout << "ERROR: option -" << option << " is mandatory.\n";
        return false;
    }
    // TODO: check player naems and scores
    return true;
}

bool valid_bad_put(const string &msg) {
    return msg.size() > 10 and 
        msg.substr(0, 8) == "BAD_PUT " and
        msg.substr(msg.size() - 2, 2) == "\r\n";

}
bool valid_penalty(const string &msg) {
    return msg.size() > 10 and 
        msg.substr(0, 8) == "PENALTY " and
        msg.substr(msg.size() - 2, 2) == "\r\n"; 
}
bool valid_state(const string &msg) {
    return msg.size() > 8 and 
        msg.substr(0, 6) == "STATE " and
        msg.substr(msg.size() - 2, 2) == "\r\n";
}
bool valid_coeff(string &msg) {
    bool pref_suf = msg.size() > 8 and 
        msg.substr(0, 6) == "COEFF " and
        msg.substr(msg.size() - 2, 2) == "\r\n";
    if (!pref_suf) {
        return false;
    }

    msg.pop_back();
    msg.pop_back(); // Remove "\r\n"

    for (size_t i = 6; i < msg.size();) {
        size_t next_space = msg.find(' ', i);
        if (next_space == string::npos) {
            string coeff = msg.substr(i);
            if (coeff.empty() || !is_proper_rational(coeff)) {
                msg += "\r\n"; 
                return false; // Invalid coefficient format
            }
            break;
        }
        string coeff = msg.substr(i, next_space - i);
        if (coeff.empty() || !is_proper_rational(coeff)) {
            msg += "\r\n"; 
            return false; // Invalid coefficient format
        }
        i = next_space + 1;
    }
    msg += "\r\n"; 
    return true; // Valid COEFF message
}

// ClientMessageQueue
void ClientMessageQueue::push(const string& msg) {
    messages.push(msg);
}
bool ClientMessageQueue::empty() const {
    return messages.empty();
}
void ClientMessageQueue::send_message(int socket_fd) {
    if (current_message.empty()) {
        current_message = messages.front();
        messages.pop();
        current_pos = 0;
    }

    ssize_t bytes_sent = write(
        socket_fd, 
        current_message.c_str() + current_pos, 
        current_message.size() - current_pos
    );
    if (bytes_sent < 0) {
        print_error("Senging message failed: " + string(strerror(errno)));
        return;
    }
    current_pos += (size_t) bytes_sent;
    if (current_pos == current_message.size()) {
        current_message.clear();
        current_pos = 0;
    }
}


// Client



int Client::setup_stdin() {
    int fd = STDIN_FILENO;
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0) {
        print_error("Cannot get flags for stdin: " + string(strerror(errno)));
        return -1;
    }
    flags |= O_NONBLOCK; 
    if (fcntl(fd, F_SETFL, flags) < 0) {
        print_error("Cannot set stdin to non-blocking: " + string(strerror(errno)));
        return -1;
    }
    return 0;
}

int Client::connect_to_server(bool force_ipv4, bool force_ipv6) {
    addrinfo hints{};
    addrinfo *res;

    hints.ai_family = force_ipv4 ? AF_INET : (force_ipv6 ? AF_INET6 : AF_UNSPEC);
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int ret = getaddrinfo(
        server_address.c_str(),
        to_string(server_port).c_str(),
        &hints,
        &res
    );

    if (ret != 0) {
        print_error("getaddrinfo failed: " + string(gai_strerror(ret)));
        return -1;
    }


    socket_fd = socket(
        res->ai_family,
        SOCK_STREAM, 
        0
    );
    if (socket_fd < 0) {
        print_error("Cannot create socket: " + string(strerror(errno)));
        freeaddrinfo(res);
        return -1;
    }

    ret = connect(
        socket_fd,
        res->ai_addr,
        res->ai_addrlen
    );
    if (ret < 0) {
        print_error("Cannot connect to server: " + string(strerror(errno)));
        freeaddrinfo(res);
        close(socket_fd);
        return -1;
    }

    fds[1].fd = socket_fd;
    fds[1].events = POLLIN | POLLOUT;
    fds[1].revents = 0;

    if (res->ai_family == AF_INET) {
        sockaddr_in *ipv4 = (sockaddr_in *)res->ai_addr;
        char ip_str[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &ipv4->sin_addr, ip_str, sizeof(ip_str)) == NULL) {
            print_error("inet_ntop failed: " + string(strerror(errno)));
            freeaddrinfo(res);
            close(socket_fd);
            return -1;
        }
        server_ip = string(ip_str);
    }
    else {
        sockaddr_in6 *ipv6 = (sockaddr_in6 *)res->ai_addr;
        char ip_str[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, &ipv6->sin6_addr, ip_str, sizeof(ip_str)) == NULL) {
            print_error("inet_ntop failed: " + string(strerror(errno)));
            freeaddrinfo(res);
            close(socket_fd);
            return -1;
        }
        server_ip = string(ip_str);
    }

    cout << "Connected to " << server_ip << ":" << server_port << endl;
    freeaddrinfo(res);
    return socket_fd;
}

int Client::send_hello() {
    messages_to_send.push("HELLO " + player_id + "\r\n");
    return 0;
}

int Client::read_message() {
    ssize_t read_len = read(fds[1].fd, buffer.data(), buff_len);
    if (read_len < 0) {
        print_error("Reading message from server failed: " + string(strerror(errno)));
        return -1;
    }
    else if (read_len == 0) {
        cout << "Server closed the connection." << endl;
        return 1;
    }
    size_t old_len = received_buffer.size();
    received_buffer += string(buffer.data(), (size_t) read_len);

    vector<string> messages;
    size_t erase_pref = 0;
    for (size_t i = old_len ? old_len - 1 : 0; i < received_buffer.size() - 1; ++i) {
        if (received_buffer[i] == '\r' and received_buffer[i + 1] == '\n') {
            // Found end of a message.
            messages.push_back(received_buffer.substr(erase_pref, i + 2 - erase_pref));
            erase_pref = i + 2; 
        }
    }
    received_buffer.erase(0, erase_pref);

    for (size_t i = 0; i < messages.size(); ++i) {
        string &msg = messages[i];
        if(is_valid_scoring(msg)) {
            cout << "Game end, scoring: " << msg.substr(8, msg.size() - 10) << "." << endl;
            return 1; // Game ended
        }    
        else if (!got_coeff) {
            if (valid_coeff(msg)) {
                got_coeff = true;
                got_response = true;
                cout << "Received coefficients: " << msg.substr(6, msg.size() - 8) << "." << endl;
                coefficients = parse_coefficients(msg);
                n = (int32_t) coefficients.size() - 1;
            } 
            else {
                print_error("bad message from " + msg); // TODO: serwer ip i ports
                return -1; // TODO zakoncz gre kodem 1!
            }
        }
        else {
            if (valid_bad_put(msg)) {
                // TODO idk czy cos pisac
            } 
            else if (valid_penalty(msg)) {
                // TODO idk czy cos pisac
            }
            else if (valid_state(msg)) {
                got_response = true;
                k = (int32_t) count(msg.begin(), msg.end(), ' ') - 1;
                cout << "Received state: " << msg.substr(6, msg.size() - 8) << "." << endl;
            }
            else {
                print_error("bad message from " + msg); // TODO: serwer ip i ports
            }
        }
    }
    return 0;
}

int Client::read_from_stdin() {
    string line;
    getline(cin, line);
    if (line.empty()) {
        print_error("Empty input from stdin.");
        return -1; // Error reading from stdin
    }
    size_t space_pos = line.find(' ');
    string point = line.substr(0, space_pos);
    string value = line.substr(space_pos + 1);


    int64_t point_int = get_int(point, INT32_MAX);
    if (point_int < 0 or !is_proper_rational(value)) {
        print_error("invalid input line: " + line);
        return 0; 
    }
    messages_to_send.push("PUT " + point + " " + value + "\r\n");
    return 0;
}

int Client::auto_play() {
    // here I dont need stdin. 
    // first I need to get the coefficients
    while (!got_coeff) {
        fds[1].revents = 0;
        fds[1].events = POLLIN;
        if (!messages_to_send.empty()) {
            fds[1].events |= POLLOUT; // I have to send Hello
        }
        int poll_status = poll(fds + 1, 1, -1);
        if (poll_status < 0) {
            print_error("Poll error occurred: " + string(strerror(errno)));
            return -1;
        }
        if (fds[1].revents & POLLIN) {
            int res = read_message();
            if (res < 0) {
                return -1; // Error reading message
            }
            else if (res) {
                return 0; // Game ended
            }
        }
        if (fds[1].revents & POLLOUT) {
            if (!messages_to_send.empty()) {
                messages_to_send.send_message(fds[1].fd);
            }
        }
    }


    // now I have the coefficients
    // I send PUT 0 0 to get to know k
    messages_to_send.push("PUT 0 0\r\n");
    got_response = false;
    while (!got_response) {
        fds[1].revents = 0;
        fds[1].events = POLLIN;
        if (!messages_to_send.empty()) {
            fds[1].events |= POLLOUT; 
        }
        int poll_status = poll(fds + 1, 1, -1);
        if (poll_status < 0) {
            print_error("Poll error occurred: " + string(strerror(errno)));
            return -1;
        }
        if (fds[1].revents & POLLIN) {
            int res = read_message();
            if (res < 0) {
                return -1; // Error reading message
            }
            else if (res) {
                return 0; // Game ended
            }
        }
        if (fds[1].revents & POLLOUT) {
            if (!messages_to_send.empty()) {
                messages_to_send.send_message(fds[1].fd);
            }
        }
    }

    // now that i know k I calculate values of the polynomial
    // at all point 0, 1, ..., k and sort them
    using el = pair<pair<double, double>, int32_t>;
    priority_queue<el, vector<el>, less<el>> val_que;

    for (size_t i = 0; i <= (size_t) k; ++i) {
        double power = 1.0;
        double value = 0.0;
        for (size_t j = 0; j <= (size_t) n; ++j) {
            value += coefficients[j] * power;
            power *= (double) i;
        }
        val_que.push({{fabs(value), value}, (int32_t) i});
    }
    
    // now I send PUT messages for all points form largest to smallest
    while (!val_que.empty()) {
        auto values = val_que.top();
        val_que.pop();
        if (values.first.first >= 5) {
            double val = 0;
            if (values.first.second < 0) {
                val = -5;
            }
            else {
                val = 5;
            }
            string msg = "PUT " + to_string(values.second) + " " + to_proper_rational(val) + "\r\n";
            messages_to_send.push(msg);

            values.first.first -= val;
            values.first.second -= val;
            val_que.push(values);
        }
        else {
            double val = values.first.second;
            string msg = "PUT " + to_string(values.second) + " " + to_proper_rational(val) + "\r\n";
            messages_to_send.push(msg);
        }
        

        while (!messages_to_send.empty()) {
            fds[1].revents = 0;
            fds[1].events = POLLIN | POLLOUT;
            int poll_status = poll(fds + 1, 1, -1);
            if (poll_status < 0) {
                print_error("Poll error occurred: " + string(strerror(errno)));
                return -1;
            }
            if (fds[1].revents & POLLIN) {
                int res = read_message();
                if (res < 0) {
                    return -1; // Error reading message
                }
                else if (res) {
                    return 0; // Game ended
                }
            }
            if (fds[1].revents & POLLOUT) {
                messages_to_send.send_message(fds[1].fd);
            }
        }
        got_response = false; 

        while (!got_response) {
            fds[1].revents = 0;
            fds[1].events = POLLIN;
            int poll_status = poll(fds + 1, 1, -1);
            if (poll_status < 0) {
                print_error("Poll error occurred: " + string(strerror(errno)));
                return -1;
            }
            if (fds[1].revents & POLLIN) {
                int res = read_message();
                if (res < 0) {
                    return -1; // Error reading message
                }
                else if (res) {
                    return 0; // Game ended
                }
            }
        }
    }

    // now I just send 0s until the game ends
    while (true) {
        if (got_response) {
            // I truy to send PUT 0 0\r\n
            messages_to_send.push("PUT 0 0\r\n");
            while (!messages_to_send.empty()) {
                fds[1].revents = 0;
                fds[1].events = POLLIN | POLLOUT;
                int poll_status = poll(fds + 1, 1, -1);
                if (poll_status < 0) {
                    print_error("Poll error occurred: " + string(strerror(errno)));
                    return -1;
                }
                if (fds[1].revents & POLLIN) {
                    int res = read_message();
                    if (res < 0) {
                        return -1; // Error reading message
                    }
                    else if (res) {
                        return 0; // Game ended
                    }
                }
                if (fds[1].revents & POLLOUT) {
                    messages_to_send.send_message(fds[1].fd);
                }
            }
            got_response = false; 
        }
        else {
            fds[1].revents = 0;
            fds[1].events = POLLIN;
            int poll_status = poll(fds + 1, 1, -1);
            if (poll_status < 0) {
                print_error("Poll error occurred: " + string(strerror(errno)));
                return -1;
            }
            if (fds[1].revents & POLLIN) {
                int res = read_message();
                if (res)
                    return -1; // Error reading message
                else if (res) {
                    return 0; // Game ended
                }
            }
        }
    }
    return 0;
}


int Client::interactive_play() {
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = socket_fd;
    fds[1].events = POLLIN;

    bool finished = false;
    while (!finished) {
        fds[0].revents  = fds[1].revents = 0;
        if (!got_coeff) {
            // not reading stdin
            int poll_status = poll(fds + 1, 1, -1);
            if (poll_status < 0) {
                print_error("Poll error occurred: " + string(strerror(errno)));
                return -1;
            }
            if (fds[1].revents & POLLIN) {
                int res = read_message();
                if (res < 0) {
                    return -1; // Error reading message
                }
                else if (res) {
                    finished = true;
                    return 0;
                }
            }
        }
        else {
            if (messages_to_send.empty()) { 
                fds[1].events = POLLIN;
            }
            else { 
                fds[1].events = POLLIN | POLLOUT;
            }

            int poll_status = poll(fds, 2, -1);

            if (poll_status < 0) {
                print_error("Poll error occurred: " + string(strerror(errno)));
                return -1;
            }
            else if (poll_status == 0) {
                // Timeout, nothing to do
                continue;
            }
            if (fds[1].revents & POLLIN) {
                int read_res = read_message();
                if (read_res < 0) {
                    return -1; // Error reading message
                }
                else if (read_res) {
                    finished = true;
                    return 0; // Game ended
                }
            }
            if (fds[1].revents & POLLOUT and !messages_to_send.empty()) {
                messages_to_send.send_message(fds[1].fd);
            }
            if (fds[0].revents & POLLIN) {
                if (read_from_stdin() < 0) {
                    return -1; // Error reading from stdin
                }
            }
        }
    }
    return 0;
}