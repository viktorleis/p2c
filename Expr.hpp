#pragma once
#include <string>

#include "iu.hpp"

namespace p2c {

// abstract base class of all expressions
struct Exp {
    // compile expression to string
    virtual std::string compile() = 0;
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

    std::string compile() override { return iu->varname; }
    IUSet iusUsed() override { return IUSet({iu}); }
};

// expression that represent a constant value
template<typename T>
    requires is_p2c_type<T>
struct ConstExp : public Exp {
    T x;

    // constructor
    ConstExp(T x) : x(x) {};
    // destructor
    ~ConstExp() {}

    std::string compile() override {
        if constexpr (type_tag<T>::tag == Type::String) {
            return fmt::format("\"{}\"", x);  // Add quotes for strings
        } else {
            return fmt::format("{}", x);
        }
    }
    IUSet iusUsed() override { return {}; }
};

// expression that represents function all
struct FnExp : public Exp {
    // function name
    std::string fnName;
    // arguments
    std::vector<std::unique_ptr<Exp>> args;

    // constructor
    FnExp(std::string fnName, std::vector<std::unique_ptr<Exp>>&& v)
        : fnName(fnName), args(std::move(v)) {}
    // destructor
    ~FnExp() {}

    std::string compile() override {
        std::vector<std::string> strs;
        for (auto& e : args) strs.emplace_back(e->compile());
        return fmt::format("{}({})", fnName, fmt::join(strs, ","));
    }

    IUSet iusUsed() override {
        IUSet result;
        for (auto& exp : args)
            for (IU* iu : exp->iusUsed()) result.add(iu);
        return result;
    }
};

// create a function call expression (helper)
template<typename T>
    requires is_p2c_type<T>
std::unique_ptr<Exp> makeCallExp(const std::string& fn, IU* iu, const T& x) {
    std::vector<std::unique_ptr<Exp>> v;
    v.push_back(std::make_unique<IUExp>(iu));
    v.push_back(std::make_unique<ConstExp<T>>(x));
    return std::make_unique<FnExp>(fn, std::move(v));
}

template<typename... T>
std::unique_ptr<Exp> makeCallExp(const std::string& fn, std::unique_ptr<T>... args) {
    std::vector<std::unique_ptr<Exp>> v;
    (v.push_back(std::move(args)), ...);
    return make_unique<FnExp>(fn, std::move(v));
}

}  // namespace p2c