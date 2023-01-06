#include <cassert>
#include <algorithm>
#include <functional>
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

template<class Fn>
void blk(const string& str, Fn fn) {
   cout << str << "{" << endl;
   fn();
   cout << "}" << endl;
}

enum Type { Int, String, Double, Bool };
string tname(Type t) {
   switch (t) {
      case Int: return "int";
      case String: return "char*";
      case Double: return "double";
      case Bool: return "bool";
   };
   throw;
}

map<string, vector<pair<string, Type>>> schema = {{"customer", {{"c_custkey", Type::Int}}}};

struct IU {
   string name;
   Type type;
};

string varname(IU* iu) {
   return format("{}{:x}", iu->name, reinterpret_cast<uintptr_t>(iu));
}

////////////////////////////////////////////////////////////////////////////////

struct IUSet {
   vector<IU*> v;

   IUSet() {}

   IUSet(IU* iu) {
      v.push_back(iu);
   }

   IUSet& operator=(IUSet other) {
      std::swap(v, other.v);
      return *this;
   }

   IUSet(IUSet&& x) {
      v = std::move(x.v);
   }

   IUSet(const IUSet& x) {
      v = x.v;
   }

   IUSet(IUSet& x) {
      v = x.v;
   }

   IUSet(const vector<IU*>& vv) {
      v = vv;
      sort(v.begin(), v.end());
   }

   IU** begin() { return v.data(); }
   IU** end() { return v.data() + v.size(); }

   void add(IU* iu) {
      auto it = lower_bound(v.begin(), v.end(), iu);
      if (it == v.end() || *it != iu)
         v.insert(it, iu);
   }
};

IUSet operator|(const IUSet& a, const IUSet& b) {
   IUSet result;
   set_union(a.v.begin(), a.v.end(), b.v.begin(), b.v.end(), back_inserter(result.v));
   return result;
}

IUSet operator&(const IUSet& a, const IUSet& b) {
   IUSet result;
   set_intersection(a.v.begin(), a.v.end(), b.v.begin(), b.v.end(), back_inserter(result.v));
   return result;
}

IUSet operator-(const IUSet& a, const IUSet& b) {
   IUSet result;
   set_difference(a.v.begin(), a.v.end(), b.v.begin(), b.v.end(), back_inserter(result.v));
   return result;
}

////////////////////////////////////////////////////////////////////////////////

struct Exp {
   virtual string compile() = 0;
   virtual IUSet iusUsed() = 0;
   virtual ~Exp() {};
};

struct IUExp : public Exp {
   IU* iu;

   IUExp(IU* iu) : iu(iu) {}
   ~IUExp() {}

   string compile() override {
      return format("{}", varname(iu));
   }

   IUSet iusUsed() override {
      return {iu};
   }
};

struct ConstIntExp : public Exp {
   int x;

   ConstIntExp(int x) : x(x) {};
   ~ConstIntExp() {}

   string compile() override {
      return format("{}", x);
   }

   IUSet iusUsed() override {
      return {};
   }
};

struct FnExp : public Exp {
   string fnName;
   vector<unique_ptr<Exp>> args;

   FnExp(string fnName, vector<unique_ptr<Exp>>&& v) : fnName(fnName), args(std::move(v)) {}
   ~FnExp() {}

   string compile() override {
      vector<string> strs;
      for (auto& e : args)
         strs.emplace_back(e->compile());
      return format("{}({})", fnName, join(strs, ","));
   }

   IUSet iusUsed() override {
      IUSet result;
      for (auto& exp : args)
         for (IU* iu : exp->iusUsed())
            result.add(iu);
      return result;
   }
};

////////////////////////////////////////////////////////////////////////////////

typedef std::function<void(void)> Consumer;

struct Operator {
   IUSet required;
   virtual IUSet availableIUs() = 0;
   virtual void computeRequired(IUSet requiredInit) = 0;
   virtual void produce(Consumer consume) = 0;
   virtual ~Operator() {}
};

struct Scan : public Operator {
   vector<IU> attributes;
   IU tid = {"tid", Type::Int};
   string relName;

   Scan(const string& relName) : relName(relName) {
      auto it = schema.find(relName);
      assert(it != schema.end());
      auto& rel = it->second;
      attributes.reserve(rel.size());
      for (auto& att : rel)
         attributes.emplace_back(IU{att.first, att.second});
   }

   ~Scan() {}

   IUSet availableIUs() override {
      IUSet result;
      for (auto& iu : attributes)
         result.add(&iu);
      return result;
   }

   void computeRequired(IUSet requiredInit) override {
      required = requiredInit;
   }

   void produce(Consumer consume) override {
      blk(format("for (uint64_t {0} = 0; {0} != db.{1}.size(); {0}++)", varname(&tid), relName), [&]() {
         for (IU* iu : required)
            print("{} {} = db.{}.{}[{}];\n", tname(iu->type), varname(iu), relName, iu->name, varname(&tid));
         consume();
      });
   }

   IU* getIU(const string& attName) {
      for (IU& iu : attributes)
         if (iu.name == attName)
            return &iu;
      return nullptr;
   }

   vector<IU*> getIUs(const vector<string>& atts) {
      vector<IU*> v;
      for (const string& a : atts)
         v.push_back(getIU(a));
      return v;
   }
};

struct Selection : public Operator {
   unique_ptr<Operator> input;
   unique_ptr<Exp> pred;

   Selection(unique_ptr<Operator> op, unique_ptr<Exp> predicate) : input(std::move(op)), pred(std::move(predicate)) {}

   ~Selection() {}

   IUSet availableIUs() override {
      return input->availableIUs();
   }

   void computeRequired(IUSet requiredInit) override {
      required = requiredInit | pred->iusUsed();
      input->computeRequired(required);
   }

   void produce(Consumer consume) override {
      input->produce([&](){
         blk(format("if ({})", pred->compile()), [&]() {
            consume();
         });
      });
   }
};

/*
struct HashJoin : public Operator {
   unique_ptr<Operator> left;
   unique_ptr<Operator> right;
   vector<IU*> leftKeyIUs, rightKeyIUs, payloadIUs;

   void produce(Consumer consume) override {
      print("unordered_map<tuple<{}>, tuple{}> {};\n",);
      left->produce([&](){
         gen("map.insert(tuple<{}>({}))", leftKeyIUs, payloadIUs);
      });
      right->produce([&]() {
      });
   }
};
*/

////////////////////////////////////////////////////////////////////////////////

unique_ptr<Exp> constCmp(const string& str, IU* iu, int x) {
   auto e1 = make_unique<IUExp>(iu);
   auto e2 = make_unique<ConstIntExp>(x);
   vector<unique_ptr<Exp>> v;
   v.push_back(std::move(e1));
   v.push_back(std::move(e2));
   return make_unique<FnExp>(str, std::move(v));
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
   auto c = make_unique<Scan>("customer");
   IU* ck = c->getIU("c_custkey");
   auto sel = make_unique<Selection>(std::move(c), constCmp("std::equal_to", ck, 1));
   sel->computeRequired({ck});
   sel->produce([]() {
      
   });

   return 0;
}


/*

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
