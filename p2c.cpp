#include <cassert>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <string>
#include <string_view>

using namespace std;
using namespace fmt;

#define blk(str, ...) do { cout << str << "{" << endl; __VA_ARGS__; cout << "}" << endl; } while (0);
#define gen(...) do { cout << "{" << endl; __VA_ARGS__; cout << "}" << endl; } while (0);

enum Type { Int, String, Double, Bool };

map<string, map<string, Type>> schema = {{"customer", {{"c_custkey", Type::Int}}}};

struct IU {
   Type type;
   string name;
};

struct Operator {
   vector<IU*> required;

   virtual void prepare(Operator* consumerInit, const vector<IU*>& requiredInit);
   virtual void produce();
};

struct Scan : public Operator {
   map<string, IU*> scope;
   string relName;
   Operator* consumer;

   Scan(const string& relName) : relName(relName) {
      auto rel = schema.find(relName);
      assert(rel != schema.end());
      scope.insert(rel->second.begin(), rel->second.end());
   }

   void prepare(Operator* consumerInit, const vector<IU*>& requiredInit) {
      consumer = consumerInit;
      required.insert(required.begin(), requiredInit.begin(), requiredInit.end());
   }

   void produce() {
      
   }
};

struct PrintSink : public Operator {
   Operator* input;
};


int main(int argc, char* argv[]) {
   

   return 0;
}


/*

-IU, tablescan, printop
-expressions (=, <, >, <=, >=, OR, less, less_equal, equal_to, logical_and), map, selection
-join (hash types, std:tuple)

IU: datatype
scope: map (name -> IU)
context: map (IU -> symbol) "compilation context that offers access to attributes by mapping IUs to variables"

bool type?
mmap vectors

---

Scan n0("nation");
Select n(n0, exp("equal_to", getIU(n0, "n_name"), constStr("foo")));
Scan r("region");
HashJoin hj(move(n), move(r), {"n_regionkey"}, {"r_regionkey"});

 */
