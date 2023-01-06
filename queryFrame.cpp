#include <functional>
#include <tuple>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <cassert>
#include <boost/functional/hash.hpp>

// make sure we can hash; XXX: fix string hashing
namespace std {
template<typename... T>
struct hash<tuple<T...>> {
   size_t operator()(tuple<T...> const& arg) const noexcept { return boost::hash_value(arg); }
};
}

using namespace std;

struct CustomerRelation {
   vector<int> c_custkey;
   vector<char*> c_name;
   vector<char*> c_address;
   vector<char*> c_city;
   vector<char*> c_nation;
   vector<char*> c_region;
   vector<char*> c_phone;
   vector<char*> c_mktsegment;
   uint64_t tupleCount = 0;
};

struct Database {
   CustomerRelation customer;
};


int main() {
   Database db;
#include "gen.cpp"
   return 0;
}
