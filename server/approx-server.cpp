#include <poll.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <unordered_set>

#include "../common/utils.hpp"
#include "utils-server.hpp"

using namespace std;

constexpr int64_t DEF_P = 0, MIN_P = 0, MAX_P = 65535;
constexpr int64_t DEF_K = 100, MIN_K = 1, MAX_K = 10000;
constexpr int64_t DEF_N = 4, MIN_N = 1, MAX_N = 8;
constexpr int64_t DEF_M = 131, MIN_M = 1, MAX_M = 12341234;

int main(int argc, char* argv[]) {
  map<char, char*> args;

  if (argc % 2 != 1) {
    print_error("every option must have a value.");
    return 1;
  }

  unordered_set<string> valid_args = {"-p", "-k", "-n", "-m", "-f"};

  for (int i = 1; i < argc; i += 2) {
    if (!valid_args.contains(argv[i])) {
      print_error(string("invalid option ") + argv[i] + ".");
      return 1;
    }
    if (args.contains(argv[i][1])) {
      print_error(string("double parameter ") + argv[i] + ".");
      return 1;
    }
    args[argv[i][1]] = argv[i + 1];
  }

  int32_t port;
  int32_t k;
  int32_t n;
  int32_t m;
  char* f = NULL;

  port = (int32_t)get_arg('p', args, DEF_P, MIN_P, MAX_P);
  k = (int32_t)get_arg('k', args, DEF_K, MIN_K, MAX_K);
  n = (int32_t)get_arg('n', args, DEF_N, MIN_N, MAX_N);
  m = (int32_t)get_arg('m', args, DEF_M, MIN_M, MAX_M);

  if (port < 0 or k < 0 or n < 0 or m < 0) {
    return 1;
  }

  if (!args.contains('f')) {
    print_error("option -f is mandatory.");
    return 1;
  }
  f = args['f'];

  Server server((uint16_t)port, k, n, m, f);
  if (server.set_up() < 0) {
    return 1;
  }

  while (true) {
    server.play_a_game();
    sleep(1);  // Sleep for 1 second before the next game
  }
  return 0;
}