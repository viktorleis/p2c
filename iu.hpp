#pragma once
#include <sstream>
#include <string>

#include "types.hpp"

////////////////////////////////////////////////////////////////////////////////

namespace p2c {

// counter to make all IU names unique in generated code

std::string genVar(const std::string& name) {
    static unsigned varCounter = 1;
    return fmt::format("{}{}", name, varCounter++);
}

struct IU {
    std::string name;
    Type type;
    std::string varname;

    IU(const std::string& name, Type type) : name(name), type(type), varname(genVar(name)) {}
};

// format comma-separated list of IU types (helper function)
std::string formatTypes(const std::vector<IU*>& ius) {
    std::stringstream ss;
    for (IU* iu : ius) ss << tname(iu->type) << ",";
    std::string result = ss.str();
    if (result.size()) result.pop_back();  // remove last ','
    return result;
}

// format comma-separated list of IU varnames (helper function)
std::string formatVarnames(const std::vector<IU*>& ius) {
    std::stringstream ss;
    for (IU* iu : ius) ss << iu->varname << ",";
    std::string result = ss.str();
    if (result.size()) result.pop_back();  // remove last ','
    return result;
}

// provide an IU by generating local variable (helper)
void provideIU(IU* iu, const std::string& value) {
    fmt::print("{} {} = {};\n", tname(iu->type), iu->varname, value);
}

// an unordered set of IUs
struct IUSet {
    // set is represented as array; invariant: IUs are sorted by pointer value
    std::vector<IU*> v;

    // empty set constructor
    IUSet() {}

    // move constructor
    IUSet(IUSet&& x) { v = std::move(x.v); }

    // copy constructor
    IUSet(const IUSet& x) { v = x.v; }

    // convert vector to set of IUs (assumes vector is unique, but not sorted)
    explicit IUSet(const std::vector<IU*>& vv) {
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
        if (it == v.end() || *it != iu) v.insert(it, iu);  // O(n), not nice
    }

    bool contains(IU* iu) const {
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

}  // namespace p2c