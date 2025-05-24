#include <iostream>
#include <cstdint>
#include <cstring>
#include <map>
#include <stdexcept>

using namespace std;

const uint64_t DEF_P = 0, MIN_P = 0, MAX_P = 65535;
const uint64_t DEF_K = 100, MIN_K = 1, MAX_K = 10000;
const uint64_t DEF_N = 4, MIN_N = 1, MAX_N = 8;
const uint64_t DEF_M = 131, MIN_M = 1, MAX_M = 12341234;

// TODO: co na cerr co na cout???

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



int main(int argc, char* argv[]) {
    
    map<char, char*> args;

    if (argc % 2 != 1) {
        cout << "ERROR: Every option must have a value.\n";
        return 1; // TODO: erorry jak w tresi
    }


    
    for (int i = 1; i < argc; i += 2) {
        if (argv[i] != "-p" and argv[i] != "-k" and 
            argv[i] != "-n" and argv[i] != "-m" and
            argv[i] != "-f") {
            cout << "ERROR: invalid option " << argv[i] << ".\n";
            return 1; // TODO: erorry jak w tresi
        }
        if (args.contains(argv[i][1])) {
            cout << "ERROR: double parameter " << argv[i] << ".\n";
            return 1;
        }
        args[argv[i][1]] = argv[i + 1];
    }

    uint16_t port;
    uint16_t k;
    uint8_t n;
    uint32_t m;
    char *f = NULL;
    uint32_t f_len;

    try {
        port = get_arg('p', args, DEF_P, MIN_P, MAX_P);
        k = get_arg('k', args, DEF_K, MIN_K, MAX_K);
        n = get_arg('n', args, DEF_N, MIN_N, MAX_N);
        m = get_arg('m', args, DEF_M, MIN_M, MAX_M);
    }
    catch (const invalid_argument &e) {
        cout << "ERROR: " << e.what();
        return 1;
    } 

    if (!args.contains('f')) {
        cout << "ERROR: option -f is mandatory.\n";
        return 1;
    }
    f = args['f'];


    

    return 0;
}