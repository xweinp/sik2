#pragma once

#include <stdexcept>
#include <map>
#include <cstdint>
#include <cstring>
#include <signal.h>
#include <chrono>
#include <iostream>

using namespace std;

void print_error(const string& description);

string to_proper_rational(double val);

int64_t get_arg(char arg, map<char, char*> &args, int64_t def, int64_t min, int64_t max);
bool is_id_valid(const string& id);

bool is_proper_rational(const string& str);

vector<double> parse_coefficients(string &coeff_str);
double get_double(const string& msg); 

bool is_valid_scoring(const string& msg);

// I assume that the integer non-negative
int64_t get_int(const string& msg, int64_t mx);