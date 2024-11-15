#pragma once
#include "../Expr.hpp"
#include "Operator.hpp"

namespace p2c {

// map operator (compute new value)
struct Map : public Operator {
    std::unique_ptr<Operator> input;
    std::unique_ptr<Exp> exp;
    IU iu;

    // constructor
    Map(std::unique_ptr<Operator> input, std::unique_ptr<Exp> exp, const std::string& name,
        Type type)
        : input(std::move(input)), exp(std::move(exp)), iu{name, type} {}

    // destructor
    ~Map() {}

    IUSet availableIUs() override { return input->availableIUs() | IUSet({&iu}); }

    void produce(const IUSet& required, ConsumerFn consume) override {
        input->produce((required | exp->iusUsed()) - IUSet({&iu}), [&]() {
            genBlock("", [&]() {
                provideIU(&iu, exp->compile());
                consume();
            });
        });
    }

    IU* getIU(const std::string& attName) {
        if (iu.name == attName) return &iu;
        throw;
    }
};

}  // namespace p2c