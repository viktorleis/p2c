// Viktor Leis, 2023

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

// available types
enum Type { Int, String, Double, Bool, Undefined };

// convert compile-time type to C++ type name
string tname(Type t) {
   switch (t) {
      case Int: return "int";
      case String: return "char*";
      case Double: return "double";
      case Bool: return "bool";
      case Undefined: throw;
   };
   throw;
}

map<string, vector<pair<string, Type>>> schema = {
   {"customer", {{"c_custkey", Type::Int}, {"c_name", Type::String}, {"c_address", Type::String}, {"c_city", Type::String},
                 {"c_nation", Type::String}, {"c_region", Type::String}, {"c_phone", Type::String}, {"c_mktsegment", Type::String}}}};

////////////////////////////////////////////////////////////////////////////////

// counter to make all IU names unique in generated code
unsigned varCounter = 1;

struct IU {
   string name;
   Type type;
   string varname;

   IU(const string& name, Type type) : name(name), type(type), varname(format("{}{}", name, varCounter++)) {}
};

// format comma-separated list of IU types (helper function)
string formatTypes(const vector<IU*>& ius) {
   stringstream ss;
   for (IU* iu : ius)
      ss << tname(iu->type) << ",";
   string result = ss.str();
   if (result.size())
      result.pop_back(); // remove last ','
   return result;
}

// format comma-separated list of IU varnames (helper function)
string formatVarnames(const vector<IU*>& ius) {
   stringstream ss;
   for (IU* iu : ius)
      ss << iu->varname << ",";
   string result = ss.str();
   if (result.size())
      result.pop_back(); // remove last ','
   return result;
}

// provide an IU by generating local variable (helper)
void provideIU(IU* iu, const string& value) {
   print("{} {} = {};\n", tname(iu->type), iu->varname, value);
}

// an unordered set of IUs
struct IUSet {
   // set is represented as array; invariant: IUs are sorted by pointer value
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
         v.insert(it, iu); // O(n), not nice
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

// generate curly-brace block of C++ code (helper function)
template<class Fn>
void genBlock(const string& str, Fn fn) {
   cout << str << "{" << endl;
   fn();
   cout << "}" << endl;
}

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

   // constructor
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

   // destructor
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
            provideIU(iu, format("db.{}.{}[i]", relName, iu->name));
         consume();
      });
   }

   IU* getIU(const string& attName) {
      for (IU& iu : attributes)
         if (iu.name == attName)
            return &iu;
      throw;
   }
};

// selection operator
struct Selection : public Operator {
   unique_ptr<Operator> input;
   unique_ptr<Exp> pred;

   // constructor
   Selection(unique_ptr<Operator> input, unique_ptr<Exp> predicate) : input(std::move(input)), pred(std::move(predicate)) {}

   // destructor
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

// map operator (compute new value)
struct Map : public Operator {
   unique_ptr<Operator> input;
   unique_ptr<Exp> exp;
   IU iu;

   // constructor
   Map(unique_ptr<Operator> input, unique_ptr<Exp> exp, const string& name, Type type) : input(std::move(input)), exp(std::move(exp)), iu{name, type} {}

   // destructor
   ~Map() {}

   IUSet availableIUs() override {
      return input->availableIUs() | IUSet({&iu});
   }

   void produce(const IUSet& required, ConsumerFn consume) override {
      input->produce(required - IUSet({&iu}), [&]() {
            genBlock("", [&]() {
               provideIU(&iu, exp->compile());
               consume();
            });
         });
   }

   IU* getIU(const string& attName) {
      if (iu.name == attName)
         return &iu;
      throw;
   }
};

// sort operator
struct Sort : public Operator {
   unique_ptr<Operator> input;
   vector<IU*> keyIUs;
   IU v{"sortVector", Type::Undefined};

   // constructor
   Sort(unique_ptr<Operator> input, const vector<IU*>& keyIUs) : input(std::move(input)), keyIUs(keyIUs)  {}

   // destructor
   ~Sort() {}

   IUSet availableIUs() override {
      return input->availableIUs();
   }

   void produce(const IUSet& required, ConsumerFn consume) override {
      // compute IUs
      IUSet restIUs = required - IUSet(keyIUs);
      vector<IU*> allIUs = keyIUs;
      allIUs.insert(allIUs.end(), restIUs.v.begin(), restIUs.v.end());

      // collect tuples
      print("vector<tuple<{}>> {};\n", formatTypes(allIUs), v.varname);
      input->produce(IUSet(allIUs), [&]() {
         print("{}.push_back({{{}}});\n", v.varname, formatVarnames(allIUs));
      });

      // sort
      print("sort({0}.begin(), {0}.end(), [](const auto& t1, const auto& t2) {{ return t1<t2; }});\n", v.varname);

      // iterate
      genBlock(format("for (auto& t : {})", v.varname), [&]() {
         for (unsigned i=0; i<allIUs.size(); i++)
            if (required.contains(allIUs[i]))
               provideIU(allIUs[i], format("get<{}>(t)", i));
         consume();
      });
   };
};

// group by operator
struct GroupBy : public Operator {
   enum AggFunction { Sum, Count };

   struct Aggregate {
      AggFunction aggFn; // aggregate function
      IU* inputIU; // IU to aggregate (is nullptr when aggFn==Count)
      IU resultIU;
   };

   unique_ptr<Operator> input;
   IUSet groupKeyIUs;
   vector<Aggregate> aggs;
   IU ht{"aggHT", Type::Undefined};

   // constructor
   GroupBy(unique_ptr<Operator> input, const IUSet& groupKeyIUs) : input(std::move(input)), groupKeyIUs(groupKeyIUs) {}

   // destructor
   ~GroupBy() {}

   void addCount(const string& name) {
      aggs.push_back({AggFunction::Count, nullptr, {name, Type::Int}});
   }

   void addSum(const string& name, IU* inputIU) {
      aggs.push_back({AggFunction::Sum, inputIU, {name, inputIU->type}});
   }

   vector<IU*> resultIUs() {
      vector<IU*> v;
      for (auto&[fn, inputIU, resultIU] : aggs)
         v.push_back(&resultIU);
      return v;
   }

   vector<IU*> inputIUs() {
      vector<IU*> v;
      for (auto&[fn, inputIU, resultIU] : aggs)
         if (inputIU)
            v.push_back(inputIU);
      return v;
   }

   IUSet availableIUs() override {
      return groupKeyIUs | IUSet(resultIUs());
   }

   void produce(const IUSet& required, ConsumerFn consume) override {
      // build hash table
      print("unordered_map<tuple<{}>, tuple<{}>> {};\n", formatTypes(groupKeyIUs.v), formatTypes(resultIUs()), ht.varname);
      input->produce(groupKeyIUs | IUSet(inputIUs()), [&]() {
         // insert tuple into hash table
         print("auto it = {}.find({{{}}});\n", ht.varname, formatVarnames(groupKeyIUs.v));
         genBlock(format("if (it == {}.end())", ht.varname), [&]() {
            vector<string> initValues;
            for (auto&[fn, inputIU, resultIU] : aggs) {
               switch (fn) {
                  case (AggFunction::Sum): initValues.push_back(inputIU->varname); break;
                  case (AggFunction::Count): initValues.push_back("1"); break;
               }
            }
            // insert new group
            print("{}.insert({{{{{}}}, {{{}}}}});\n", ht.varname, formatVarnames(groupKeyIUs.v), fmt::join(initValues, ","));
         });
         genBlock("else", [&]() {
            // update group
            unsigned i=0;
            for (auto&[fn, inputIU, resultIU] : aggs) {
               switch (fn) {
                  case (AggFunction::Sum): print("get<{}>(it->second) += {};\n", i, inputIU->varname); break;
                  case (AggFunction::Count): print("get<{}>(it->second)++;\n", i); break;
               }
               i++;
            }
         });
      });

      // iterate over hash table
      genBlock(format("for (auto& it : {})", ht.varname), [&]() {
         for (unsigned i=0; i<groupKeyIUs.size(); i++) {
            IU* iu = groupKeyIUs.v[i];
            if (required.contains(iu))
               provideIU(iu, format("get<{}>(it.first)", i));
         }
         unsigned i=0;
         for (auto&[fn, inputIU, resultIU] : aggs) {
            provideIU(&resultIU, format("get<{}>(it.second)", i));
            i++;
         }
         consume();
      });
   }

   IU* getIU(const string& attName) {
      for (auto&[fn, inputIU, resultIU] : aggs)
         if (resultIU.name == attName)
            return &resultIU;
      throw;
   }
};

// hash join operator
struct HashJoin : public Operator {
   unique_ptr<Operator> left;
   unique_ptr<Operator> right;
   vector<IU*> leftKeyIUs, rightKeyIUs;
   IU ht{"joinHT", Type::Undefined};

   // constructor
   HashJoin(unique_ptr<Operator> left, unique_ptr<Operator> right, const vector<IU*>& leftKeyIUs, const vector<IU*>& rightKeyIUs) :
      left(std::move(left)), right(std::move(right)), leftKeyIUs(leftKeyIUs), rightKeyIUs(rightKeyIUs) {}

   // destructor
   ~HashJoin() {}

   IUSet availableIUs() override {
      return left->availableIUs() | right->availableIUs();
   }

   void produce(const IUSet& required, ConsumerFn consume) override {
      // figure out where required IUs come from
      IUSet leftRequiredIUs = (required & left->availableIUs()) | IUSet(leftKeyIUs);
      IUSet rightRequiredIUs = (required & right->availableIUs()) | IUSet(rightKeyIUs);
      IUSet leftPayloadIUs = leftRequiredIUs - IUSet(leftKeyIUs); // these we need to store in hash table as payload

      // build hash table
      print("unordered_multimap<tuple<{}>, tuple<{}>> {};\n", formatTypes(leftKeyIUs), formatTypes(leftPayloadIUs.v), ht.varname);
      left->produce(leftRequiredIUs, [&]() {
         // insert tuple into hash table
         print("{}.insert({{{{{}}}, {{{}}}}});\n", ht.varname, formatVarnames(leftKeyIUs), formatVarnames(leftPayloadIUs.v));
      });

      // probe hash table
      right->produce(rightRequiredIUs, [&]() {
         // iterate over matches
         genBlock(format("for (auto range = {}.equal_range({{{}}}); range.first!=range.second; range.first++)", ht.varname, formatVarnames(rightKeyIUs)),
                  [&]() {
                     // unpack payload
                     unsigned countP = 0;
                     for (IU* iu : leftPayloadIUs)
                        provideIU(iu, format("get<{}>(range.first->second)", countP++));
                     // unpack keys if needed
                     for (unsigned i=0; i<leftKeyIUs.size(); i++) {
                        IU* iu = leftKeyIUs[i];
                        if (required.contains(iu))
                           provideIU(iu, format("get<{}>(range.first->first)", i));
                     }
                     // consume
                     consume();
                  });
      });
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
   auto sel = make_unique<Selection>(std::move(c), makeCallExp("std::equal_to()", ck, 1));

   auto c2 = make_unique<Scan>("customer");
   IU* ck2 = c2->getIU("c_custkey");
   IU* ca = c2->getIU("c_address");
   auto j = make_unique<HashJoin>(std::move(sel), std::move(c2), vector<IU*>{{ck, cc}}, vector<IU*>{{ck2, ca}});

   auto m = make_unique<Map>(std::move(j), makeCallExp("std::plus()", ck, 5), "ckNew", Type::Int);
   IU* ckNew = m->getIU("ckNew");

   auto gb = make_unique<GroupBy>(std::move(m), IUSet({ck, cn}));
   gb->addSum("ckNewSum", ckNew);
   gb->addCount("cnt");
   IU* sum = gb->getIU("ckNewSum");
   IU* cnt = gb->getIU("cnt");

   auto s = make_unique<Sort>(std::move(gb), vector<IU*>{{ck, sum}});

   vector<IU*> out{{ck, sum, cnt}};
   s->produce(IUSet(out), [&]() {
      for (IU* iu : out)
         print("cout << {} << \" \";", iu->varname);
      print("cout << endl;\n");
   });

   return 0;
}
