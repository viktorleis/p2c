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
#include <sstream>

using namespace std;
using namespace fmt;

template<class Fn>
void genBlock(const string& str, Fn fn) {
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

map<string, vector<pair<string, Type>>> schema = {
   {"customer", {{"c_custkey", Type::Int}, {"c_name", Type::String}, {"c_address", Type::String}, {"c_city", Type::String},
                 {"c_nation", Type::String}, {"c_region", Type::String}, {"c_phone", Type::String}, {"c_mktsegment", Type::String}}}};

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

   IUSet(IUSet&& x) {
      v = std::move(x.v);
   }

   IUSet(const IUSet& x) {
      v = x.v;
   }

   explicit IUSet(const vector<IU*>& vv) {
      v = vv;
      sort(v.begin(), v.end());
   }

   IU** begin() { return v.data(); }
   IU** end() { return v.data() + v.size(); }
   IU* const * begin() const { return v.data(); }
   IU* const * end() const { return v.data() + v.size(); }

   void add(IU* iu) {
      auto it = lower_bound(v.begin(), v.end(), iu);
      if (it == v.end() || *it != iu)
         v.insert(it, iu);
   }

   bool contains (IU* iu) const {
      auto it = lower_bound(v.begin(), v.end(), iu);
      return (it != v.end() && *it == iu);
   }

   unsigned size() const { return v.size(); };
};

bool operator==(const IUSet& a, const IUSet& b) {
   return equal(a.v.begin(), a.v.end(), b.v.begin(), b.v.end());
}

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
      return IUSet({iu});
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

typedef std::function<void(void)> ConsumerFn;

struct Operator {
   virtual IUSet availableIUs() = 0;
   virtual void produce(const IUSet& required, ConsumerFn consume) = 0;
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

   void produce(const IUSet& required, ConsumerFn consume) override {
      genBlock(format("for (uint64_t {0} = 0; {0} != db.{1}.size(); {0}++)", varname(&tid), relName), [&]() {
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

   void produce(const IUSet& required, ConsumerFn consume) override {
      input->produce(required | pred->iusUsed(), [&](){
         genBlock(format("if ({})", pred->compile()), [&]() {
            consume();
         });
      });
   }
};

string formatTypes(const vector<IU*>& ius) {
   stringstream ss;
   for (IU* iu : ius)
      ss << tname(iu->type) << ",";
   string result = ss.str();
   if (result.size())
      result.pop_back(); // remove last ,
   return result;
}

string formatValues(const vector<IU*>& ius) {
   stringstream ss;
   for (IU* iu : ius)
      ss << varname(iu) << ",";
   string result = ss.str();
   if (result.size())
      result.pop_back(); // remove last ,
   return result;
}

struct HashJoin : public Operator {
   unique_ptr<Operator> left;
   unique_ptr<Operator> right;
   vector<IU*> leftKeyIUs, rightKeyIUs;
   IU ht = {"ht", Type::Int};

   HashJoin(unique_ptr<Operator> left, unique_ptr<Operator> right, const vector<IU*>& leftKeyIUs, const vector<IU*>& rightKeyIUs) :
      left(std::move(left)), right(std::move(right)), leftKeyIUs(leftKeyIUs), rightKeyIUs(rightKeyIUs) {}

   ~HashJoin() {}

   void produce(const IUSet& required, ConsumerFn consume) override {
      IUSet leftIUs = (required - right->availableIUs()) | IUSet(leftKeyIUs);
      IUSet rightIUs = (required - left->availableIUs()) | IUSet(rightKeyIUs);
      IUSet leftPayloadIUs = leftIUs - IUSet(leftKeyIUs);

      print("unordered_multimap<tuple<{}>, tuple<{}>> {};\n", formatTypes(leftKeyIUs), formatTypes(leftPayloadIUs.v), varname(&ht));
      left->produce(leftIUs, [&](){
         // insert tuple into hash table
         print("{}.emplace({{{}}}, {{{}}});\n", varname(&ht), formatValues(leftKeyIUs), formatValues(leftPayloadIUs.v));
      });
      right->produce(rightIUs, [&]() {
         genBlock(format("for (auto it = {0}.find({{{1}}}); it!={0}.end(); it++)", // iterate
                         varname(&ht), formatValues(rightKeyIUs)), [&]() {
                            // unpack payload from hash table
                            unsigned countP=0;
                            for (IU* iu : leftPayloadIUs)
                               print("{} {} = get<{}>(it->second);\n", tname(iu->type), varname(iu), countP++);
                            // unpack keys from entry if needed
                            for (unsigned i=0; i<leftKeyIUs.size(); i++) {
                               IU* iu = leftKeyIUs[i];
                               if (required.contains(iu))
                                  print("{} {} = get<{}>(it->first);\n", tname(iu->type), varname(iu), i);
                            }
                            consume();
                         });
      });
   }

   IUSet availableIUs() override {
      return left->availableIUs() | right->availableIUs();
   }
};

////////////////////////////////////////////////////////////////////////////////

unique_ptr<Exp> callExp(const string& fn, IU* iu, int x) {
   vector<unique_ptr<Exp>> v;
   v.push_back(make_unique<IUExp>(iu));
   v.push_back(make_unique<ConstIntExp>(x));
   return make_unique<FnExp>(fn, std::move(v));
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
   auto c = make_unique<Scan>("customer");
   IU* ck = c->getIU("c_custkey");
   IU* cc = c->getIU("c_city");
   IU* cn = c->getIU("c_nation");
   auto sel = make_unique<Selection>(std::move(c), callExp("std::equal_to", ck, 1));

   auto c2 = make_unique<Scan>("customer");
   IU* ck2 = c2->getIU("c_custkey");
   IU* ca = c2->getIU("c_address");
   auto j = make_unique<HashJoin>(std::move(sel), std::move(c2), vector<IU*>{{ck, cc}}, vector<IU*>{{ck2, ca}});
   j->produce(IUSet{{cn, ck}}, []() {
      
   });

   return 0;
}


/*

shorter names
print -> multi-arg gen 
mmap vectors

 */
