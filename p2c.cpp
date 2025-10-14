// Viktor Leis, 2023

#include <cassert>
#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <format>
#include <print>
#include <source_location>
#include <string>
#include <string_view>
#include <sstream>
#include "types.hpp"
#include "tpch.hpp"

using namespace std;
using namespace p2c;

////////////////////////////////////////////////////////////////////////////////

// counter to make all IU names unique in generated code

string genVar(const string& name) {
   static unsigned varCounter = 1;
   return format("{}{}", name, varCounter++);
}

struct IU {
   string name;
   Type type;
   string varname;

   IU(const string& name, Type type) : name(name), type(type), varname(genVar(name)) {}
};

// format generic list of strings with delimites (helper function)
string join(const vector<string>& strs, const string& delim) {
   string result = "";
   bool first = true;
   for (const auto& str : strs){
      if (first) first = false;
      else result += delim;
      result += str;
   }
   return result;
}

// format comma-separated list of IU types (helper function)
string formatTypes(const vector<IU*>& ius) {
   vector<string> iuNames;
   for (IU* iu : ius)
      iuNames.push_back(tname(iu->type));
   return join(iuNames, ",");
}

// format comma-separated list of IU varnames (helper function)
string formatVarnames(const vector<IU*>& ius) {
   vector<string> varNames;
   for (IU* iu : ius)
      varNames.push_back(iu->varname);
   return join(varNames, ",");
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
      // check that there are no duplicates
      assert(adjacent_find(v.begin(), v.end()) == v.end());
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

   string compile() override { return iu->varname; }
   IUSet iusUsed() override { return IUSet({iu}); }
};

// expression that represent a constant value
template<typename T> requires is_p2c_type<T>
struct ConstExp : public Exp {
   T x;

   // constructor
   ConstExp(T x) : x(x) {};
   // destructor
   ~ConstExp() {}

   string compile() override {
      if constexpr (type_tag<T>::tag == Type::String) {
         return format("\"{}\"", x);  // Add quotes for strings
      } else {
         return format("{}", x);
      }
    }
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
void genBlock(const string& str, Fn fn, const std::source_location& location = std::source_location::current()) {
   cout << str << "{ //" << location.line() << "; " << location.function_name() << endl;
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
      auto it = TPCH::schema.find(relName);
      assert(it != TPCH::schema.end());
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
      input->produce((required | exp->iusUsed()) - IUSet({&iu}), [&]() {
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
   IU v{"vector", Type::Undefined};

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

struct Aggregate {
   IU* inputIU;  // IU to aggregate (is nullptr when aggFn==Count)
   IU resultIU;

   Aggregate(string name, IU* _inputIU) : inputIU(_inputIU), resultIU(name, _inputIU->type) {}
   virtual ~Aggregate() = default;

   virtual string genInitValue() = 0;
   virtual string genUpdate(string oldValueRef) = 0;
};

// group by operator
struct GroupBy : public Operator {
   unique_ptr<Operator> input;
   IUSet groupKeyIUs;
   vector<unique_ptr<Aggregate>> aggs;
   IU ht{"aggHT", Type::Undefined};

   // constructor
   GroupBy(unique_ptr<Operator> input, const IUSet& groupKeyIUs) : input(std::move(input)), groupKeyIUs(groupKeyIUs) {}

    // destructor
     ~GroupBy() {}

    void addAggregate(std::unique_ptr<Aggregate> agg) { aggs.emplace_back(std::move(agg)); }

    std::vector<IU*> resultIUs() {
        std::vector<IU*> v;
        for (auto& agg : aggs) v.push_back(&agg->resultIU);
        return v;
   }

   IUSet inputIUs() {
      IUSet v;
      for (auto& agg : aggs)
         if (agg->inputIU) v.add(agg->inputIU);
      return v;
   }

   IUSet availableIUs() override {
      return groupKeyIUs | IUSet(resultIUs());
   }

   void produce(const IUSet& required, ConsumerFn consume) override {
      // build hash table
      print("unordered_map<tuple<{}>, tuple<{}>> {};\n", formatTypes(groupKeyIUs.v), formatTypes(resultIUs()), ht.varname);
      input->produce(groupKeyIUs | inputIUs(), [&]() {
         // insert tuple into hash table
         print("auto it = {}.find({{{}}});\n", ht.varname, formatVarnames(groupKeyIUs.v));
         genBlock(format("if (it == {}.end())", ht.varname), [&]() {
            vector<string> initValues;
            for (auto& agg : aggs) initValues.push_back(agg->genInitValue());
               // insert new group
               print("{}.insert({{{{{}}}, {{{}}}}});\n", ht.varname, formatVarnames(groupKeyIUs.v), join(initValues, ","));
            });
            genBlock("else", [&]() {
               // update group
               unsigned i=0;
               for (auto& agg : aggs) {
                  print("{};\n", agg->genUpdate(format("get<{}>(it->second)", i++)));
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
         for (auto& agg : aggs) {
            provideIU(&agg->resultIU, format("get<{}>(it.second)", i));
            i++;
         }
         consume();
      });
   }

   IU* getIU(const string& attName) {
      for (auto& agg : aggs)
         if (agg->resultIU.name == attName) return &agg->resultIU;
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
template<typename T> requires is_p2c_type<T>
unique_ptr<Exp> makeCallExp(const string& fn, IU* iu, const T& x) {
   vector<unique_ptr<Exp>> v;
   v.push_back(make_unique<IUExp>(iu));
   v.push_back(make_unique<ConstExp<T>>(x));
   return make_unique<FnExp>(fn, std::move(v));
}

template<typename... T>
unique_ptr<Exp> makeCallExp(const string& fn,  std::unique_ptr<T>... args) {
   vector<unique_ptr<Exp>> v;
   (v.push_back(std::move(args)), ...);
   return make_unique<FnExp>(fn, std::move(v));
}

// Print
void produceAndPrint(unique_ptr<Operator> root, const std::vector<IU*>& ius, unsigned perfRepeat = 2) {
   genBlock(
      format("for (uint64_t {0} = 0; {0} != {1}; {0}++)", genVar("perfRepeat"), perfRepeat - 1),
      [&]() {
         root->produce(IUSet(ius), [&]() {
            for (IU *iu : ius)
               print("cout << {} << \" \";", iu->varname);
            print("cout << endl;\n");
         });
      });
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
    // ------------------------------------------------------------
    // Simple test query; should return the following on sf1 according to umbra:
    //   O 58 1333.79 2373761.38
    //   P 1005 6765.52 187335496.90
    //   F 144619 866.90 21856547600.45
    // ------------------------------------------------------------
    //   select o_orderstatus, count(*), min(o_totalprice), sum(o_totalprice)
    //   from orders
    //   where o_orderdate < date '1995-03-15'
    //     and o_orderpriority = '1-URGENT'
    //   group by o_orderstatus
    //   order by count(*)
    // ------------------------------------------------------------

    struct CountAggregate final : Aggregate {
        CountAggregate(string name, IU* _inputIU) : Aggregate(name, _inputIU) {}
        string genInitValue() override { return "1"; }
        string genUpdate(string oldValueRef) override {
            return format("{} += 1", oldValueRef);
        }
    };

    struct MinAggregate final : Aggregate {
        MinAggregate(string name, IU* _inputIU) : Aggregate(name, _inputIU) {}
        string genInitValue() override { return format("{}", inputIU->varname); }
        string genUpdate(string oldValueRef) override {
            return format("{} = std::min({}, {})", oldValueRef, oldValueRef, inputIU->varname);
        }
    };

    struct SumAggregate final : Aggregate {
        SumAggregate(string name, IU* _inputIU) : Aggregate(name, _inputIU) {}
        string genInitValue() override { return format("{}", inputIU->varname); }
        string genUpdate(string oldValueRef) override {
            return format("{} += {}", oldValueRef, inputIU->varname);
        }
    };

    {
        std::cout << "//" << stringToType<date>("1995-03-15", 10) << std::endl;
        auto o = make_unique<Scan>("orders");
        IU* od = o->getIU("o_orderdate");
        IU* op = o->getIU("o_totalprice");
        IU* os = o->getIU("o_orderstatus");
        IU* oprio = o->getIU("o_orderpriority");

        std::string prio = "1-URGENT";

        auto sel = make_unique<Selection>(
            std::move(o),
            makeCallExp("std::less()", od, stringToType<date>("1995-03-15", 10).value));
        sel = make_unique<Selection>(std::move(sel), makeCallExp("std::equal_to()", oprio, std::string_view(prio)));
        auto gb = make_unique<GroupBy>(std::move(sel), IUSet({os}));
        gb->addAggregate(std::make_unique<CountAggregate>("cnt", op));
        gb->addAggregate(std::make_unique<MinAggregate>("min", op));
        gb->addAggregate(std::make_unique<SumAggregate>("sum", op));

        IU* cnt_ = gb->getIU("cnt");
        IU* min_ = gb->getIU("min");
        IU* sum_ = gb->getIU("sum");

        auto sort = make_unique<Sort>(std::move(gb), std::vector<IU*>({cnt_}));
        produceAndPrint(std::move(sort), {os, cnt_, min_, sum_});
    }
    return 0;
}
