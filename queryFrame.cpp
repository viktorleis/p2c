#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "tpch.hpp"

using namespace std;
using namespace p2c;

int main(int argc, char** argv) {
   TPCH db(argc >= 2 ? argv[1] : "data-generator/output/");
   unsigned run_count = argc >= 3 ? atoi(argv[2]) : 1;
   for (unsigned run = 0; run < run_count; ++run) {
#include "gen.cpp"
   }
   return 0;
}
