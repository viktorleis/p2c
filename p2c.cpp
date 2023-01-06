#include <cassert>
#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <string>
#include <string_view>
#include <sstream>

using namespace std;
using namespace fmt;

// generate curly-brace block of C++ code (helper function)
template<class Fn>
void genBlock(const string& str, Fn fn) {
   cout << str << "{" << endl;
   fn();
   cout << "}" << endl;
}

// available types
enum Type { Int, String, Double, Bool };

// convert compile-time type to C++ type name
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

// counter to make all IU names unique in generated code
unsigned varCounter = 1;

////////////////////////////////////////////////////////////////////////////////

struct IU {
   string name;
   Type type;
   string varname;

   IU(const string& name, Type type) : name(name), type(type), varname(format("{}{}", name, varCounter++)) {}
};

////////////////////////////////////////////////////////////////////////////////

// represent an unordered set of IUs
struct IUSet {
   // invariant: IUs are always sorted by pointer value
   vector<IU*> v;

   // empty set constructor
   IUSet() {}

   // move constructor
   IUSet(IUSet&& x) {
      v = std::move(x.v);
   }

   // copy constructor
   IUSet(const IUSet& x) {
      v = x.v;
   }

   // convert vector to set of IUs (assumes vector is unique, but not sorted)
   explicit IUSet(const vector<IU*>& vv) {
      v = vv;
      sort(v.begin(), v.end());
   }

   // iterate over IUs
   IU** begin() { return v.data(); }
   IU** end() { return v.data() + v.size(); }
   IU* const* begin() const { return v.data(); }
   IU* const* end() const { return v.data() + v.size(); }

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

// set union operator
IUSet operator|(const IUSet& a, const IUSet& b) {
   IUSet result;
   set_union(a.v.begin(), a.v.end(), b.v.begin(), b.v.end(), back_inserter(result.v));
   return result;
}

// set intersection operator
IUSet operator&(const IUSet& a, const IUSet& b) {
   IUSet result;
   set_intersection(a.v.begin(), a.v.end(), b.v.begin(), b.v.end(), back_inserter(result.v));
   return result;
}

// set difference operator
IUSet operator-(const IUSet& a, const IUSet& b) {
   IUSet result;
   set_difference(a.v.begin(), a.v.end(), b.v.begin(), b.v.end(), back_inserter(result.v));
   return result;
}

// set equality operator
bool operator==(const IUSet& a, const IUSet& b) {
   return equal(a.v.begin(), a.v.end(), b.v.begin(), b.v.end());
}

////////////////////////////////////////////////////////////////////////////////

// abstract base class of all expressions
struct Exp {
   // compile expression to string
   virtual string compile() = 0;
   // set of all IUs used in this expression
   virtual IUSet iusUsed() = 0;
   // destructor
   virtual ~Exp() {};
};

// expression that simply references an IU
struct IUExp : public Exp {
   IU* iu;

   // constructor
   IUExp(IU* iu) : iu(iu) {}
   // destructor
   ~IUExp() {}

   string compile() override { return format("{}", iu->varname); }
   IUSet iusUsed() override { return IUSet({iu}); }
};

// expression that represent a constant integer
struct ConstIntExp : public Exp {
   int x;

   // constructor
   ConstIntExp(int x) : x(x) {};
   // destructor
   ~ConstIntExp() {}

   string compile() override { return format("{}", x); }
   IUSet iusUsed() override { return {}; }
};

// expression that represents function all
struct FnExp : public Exp {
   // function name
   string fnName;
   // arguments
   vector<unique_ptr<Exp>> args;

   // constructor
   FnExp(string fnName, vector<unique_ptr<Exp>>&& v) : fnName(fnName), args(std::move(v)) {}
   // destructor
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

// consumer callback function
typedef std::function<void(void)> ConsumerFn;

// abstract base class of all operators
struct Operator {
   // compute *all* IUs this operator produce
   virtual IUSet availableIUs() = 0;

   // generate code for operator providing 'required' IUs and pushing them to 'consume' callback
   virtual void produce(const IUSet& required, ConsumerFn consume) = 0;

   // destructor
   virtual ~Operator() {}
};

// table scan operator
struct Scan : public Operator {
   // IU storage for all available attributes
   vector<IU> attributes;
   // relation name
   string relName;

   Scan(const string& relName) : relName(relName) {
      // get relation info from schema
      auto it = schema.find(relName);
      assert(it != schema.end());
      auto& rel = it->second;
      // create IUs for all available attributes
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
      genBlock(format("for (uint64_t i = 0; i != db.{}.tupleCount; i++)", relName), [&]() {
         for (IU* iu : required)
            print("{} {} = db.{}.{}[i];\n", tname(iu->type), iu->varname, relName, iu->name);
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

// selection operator
struct Selection : public Operator {
   unique_ptr<Operator> input;
   unique_ptr<Exp> pred;

   Selection(unique_ptr<Operator> op, unique_ptr<Exp> predicate) : input(std::move(op)), pred(std::move(predicate)) {}

   ~Selection() {}

   IUSet availableIUs() override {
      return input->availableIUs();
   }

   void produce(const IUSet& required, ConsumerFn consume) override {
      input->produce(required | pred->iusUsed(), [&]() {
         genBlock(format("if ({})", pred->compile()), [&]() {
            consume();
         });
      });
   }
};

// map operator
struct Map : public Operator {
   unique_ptr<Operator> input;
   unique_ptr<Exp> exp;
   IU iu;

   Map(unique_ptr<Operator> op, unique_ptr<Exp> exp, const string& name, Type type) : input(std::move(op)), exp(std::move(exp)), iu{name, type} {}

   ~Map() {}

   IUSet availableIUs() override {
      return input->availableIUs() | IUSet({&iu});
   }

   void produce(const IUSet& required, ConsumerFn consume) override {
      input->produce(required - IUSet({&iu}), [&]() {
            genBlock("", [&]() {
               print("{} {} = {};\n", tname(iu.type), iu.varname, exp->compile());
               consume();
            });
         });
   }

   IU* getIU(const string& attName) {
      if (iu.name == attName)
         return &iu;
      return nullptr;
   }
};

// format comma-separated list of IU types (helper function)
string formatTypes(const vector<IU*>& ius) {
   stringstream ss;
   for (IU* iu : ius)
      ss << tname(iu->type) << ",";
   string result = ss.str();
   if (result.size())
      result.pop_back(); // remove last ,
   return result;
}

// format comma-separated list of IU varnames (helper function)
string formatVarnames(const vector<IU*>& ius) {
   stringstream ss;
   for (IU* iu : ius)
      ss << iu->varname << ",";
   string result = ss.str();
   if (result.size())
      result.pop_back(); // remove last ,
   return result;
}

// hash join operator
struct HashJoin : public Operator {
   unique_ptr<Operator> left;
   unique_ptr<Operator> right;
   vector<IU*> leftKeyIUs, rightKeyIUs;
   IU ht{"ht", Type::Int};

   HashJoin(unique_ptr<Operator> left, unique_ptr<Operator> right, const vector<IU*>& leftKeyIUs, const vector<IU*>& rightKeyIUs) :
      left(std::move(left)), right(std::move(right)), leftKeyIUs(leftKeyIUs), rightKeyIUs(rightKeyIUs) {}

   ~HashJoin() {}

   void produce(const IUSet& required, ConsumerFn consume) override {
      // figure out where required IUs come from
      IUSet leftIUs = (required - right->availableIUs()) | IUSet(leftKeyIUs);
      IUSet rightIUs = (required - left->availableIUs()) | IUSet(rightKeyIUs);
      IUSet leftPayloadIUs = leftIUs - IUSet(leftKeyIUs); // these we need to store in hash table as payload

      // build hash table
      print("unordered_multimap<tuple<{}>, tuple<{}>> {};\n", formatTypes(leftKeyIUs), formatTypes(leftPayloadIUs.v), ht.varname);
      left->produce(leftIUs, [&](){
         // insert tuple into hash table
         print("{}.insert({{{{{}}}, {{{}}}}});\n", ht.varname, formatVarnames(leftKeyIUs), formatVarnames(leftPayloadIUs.v));
      });

      // probe hash table
      right->produce(rightIUs, [&]() {
         // iterate over matches
         genBlock(format("for (auto it = {0}.find({{{1}}}); it!={0}.end(); it++)", ht.varname, formatVarnames(rightKeyIUs)),
                  [&]() {
                     // unpack payload
                     unsigned countP=0;
                     for (IU* iu : leftPayloadIUs)
                        print("{} {} = get<{}>(it->second);\n", tname(iu->type), iu->varname, countP++);
                     // unpack keys if needed
                     for (unsigned i=0; i<leftKeyIUs.size(); i++) {
                        IU* iu = leftKeyIUs[i];
                        if (required.contains(iu))
                           print("{} {} = get<{}>(it->first);\n", tname(iu->type), iu->varname, i);
                     }
                     // consume
                     consume();
                  });
      });
   }

   IUSet availableIUs() override {
      return left->availableIUs() | right->availableIUs();
   }
};

////////////////////////////////////////////////////////////////////////////////

// create a function call expression (helper)
unique_ptr<Exp> makeCallExp(const string& fn, IU* iu, int x) {
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
   auto sel = make_unique<Selection>(std::move(c), makeCallExp("std::equal_to<int>()", ck, 1));

   auto c2 = make_unique<Scan>("customer");
   IU* ck2 = c2->getIU("c_custkey");
   IU* ca = c2->getIU("c_address");
   auto j = make_unique<HashJoin>(std::move(sel), std::move(c2), vector<IU*>{{ck, cc}}, vector<IU*>{{ck2, ca}});

   auto m = make_unique<Map>(std::move(j), makeCallExp("std::plus<int>()", ck, 5), "ckNew", Type::Int);
   IU* ckNew = m->getIU("ckNew");

   vector<IU*> out{{cn, ck, ckNew}};
   m->produce(IUSet(out), [&]() {
      for (IU* iu : out)
         print("cout << {} << \" \";", iu->varname);
      print("cout << endl;\n");
      //print("cout << {} << \" \" << {} << endl;\n", cn->varname, ck->varname);
   });

   return 0;
}
