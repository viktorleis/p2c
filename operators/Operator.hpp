#pragma once
#include <fmt/core.h>
#include <fmt/ranges.h>

#include <functional>
#include <memory>
#include <source_location>
#include <string>

#include "io.hpp"
#include "iu.hpp"
#include "types.hpp"

namespace p2c {

// consumer callback function
typedef std::function<void(void)> ConsumerFn;

// generate curly-brace block of C++ code (helper function)
template<class Fn>
void genBlock(const std::string& str, Fn fn,
              const std::source_location& location = std::source_location::current()) {
    std::cout << str << "{ //" << location.line() << "; " << location.function_name() << std::endl;
    fn();
    std::cout << "}" << std::endl;
}

// abstract base class of all operators
struct Operator {
    // compute *all* IUs this operator produce
    virtual IUSet availableIUs() = 0;

    // generate code for operator providing 'required' IUs and pushing them to 'consume' callback
    virtual void produce(const IUSet& required, ConsumerFn consume) = 0;

    // destructor
    virtual ~Operator() {}
};

// Print
void produceAndPrint(std::unique_ptr<Operator> root, const std::vector<IU*>& ius,
                     unsigned perfRepeat = 2) {
    genBlock(fmt::format("for (uint64_t {0} = 0; {0} != {1}; {0}++)", genVar("perfRepeat"),
                         perfRepeat - 1),
             [&]() {
                 root->produce(IUSet(ius), [&]() {
                     for (IU* iu : ius) fmt::print("std::cout << {} << \" \";", iu->varname);
                     fmt::print("cout << endl;\n");
                 });
             });
}

}  // namespace p2c