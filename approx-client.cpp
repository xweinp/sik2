#include <iostream>
#include <unordered_set>


#include "utils.hpp"
#include "utils-client.hpp"

using namespace std;

const uint64_t DEF_P = 0, MIN_P = 0, MAX_P = 65535;

int main(int argc, char* argv[]) {
    map<char, char*> args;

    unordered_set<string> valid_args = {
        "-u", "-s", "-p", "-4", "-6", "-a"
    };

    bool auto_strategy = false;
    bool force_ipv4 = false, force_ipv6 = false;

    for (int i = 1; i < argc; i += 2) {
        if (!valid_args.contains(argv[i])) {
            cout << "ERROR: invalid option " << argv[i] << ".\n";
            return 1;
        }
        if (argv[i] == "-a") {
            auto_strategy = true;
            --i;
        }
        else if (argv[i] == "-4") {
            force_ipv4 = true;
            --i;
        }
        else if (argv[i] == "-6") {
            force_ipv6 = true;
            --i;
        }
        else if (args.contains(argv[i][1])) {
            cout << "ERROR: double parameter " << argv[i] << ".\n";
            return 1;
        }
        else {
            args[argv[i][1]] = argv[i + 1];
        }
    }

    if (force_ipv4 and force_ipv6) {
        force_ipv4 = force_ipv6 = false; 
    }

    string player_id;
    string server_address;
    uint16_t port;
    
    
    if (
        !check_mandatory_option(args, 'u') ||
        !check_mandatory_option(args, 's') ||
        !check_mandatory_option(args, 'p')
    ) {
        return 1;
    }

    player_id = args['u'];

    if (!is_id_valid(player_id)) {
        cout << "ERROR: invalid player ID '" << player_id << "'.\n";
        return 1;
    }
    server_address = args['s'];
    port = get_arg('p', args, DEF_P, MIN_P, MAX_P);
    
    Client client(
        player_id, 
        server_address, 
        port, 
        auto_strategy
    );

    
    if (client.connect_to_server(force_ipv4, force_ipv6) < 0) {
        return 1;
    }
    cout << "Connected to " + server_address + ":" + to_string(port) + "\n";

    if(client.send_hello() < 0) {
        return 1;
    }

    if (auto_strategy) {
        client.auto_play();
    } else {
        if (client.setup_stdin() < 0) {
            return 1;
        }
        client.interactive_play();
    }


    
    return 0;
}