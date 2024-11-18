#pragma once
#include "Expr.hpp"
#include "Operator.hpp"

namespace p2c {

// selection operator
struct Selection : public Operator {
    std::unique_ptr<Operator> input;
    std::unique_ptr<Exp> pred;

    // constructor
    Selection(std::unique_ptr<Operator> input, std::unique_ptr<Exp> predicate)
        : input(std::move(input)), pred(std::move(predicate)) {}

    // destructor
    ~Selection() {}

    IUSet availableIUs() override { return input->availableIUs(); }

    void produce(const IUSet& required, ConsumerFn consume) override {
        input->produce(required | pred->iusUsed(), [&]() {
            genBlock(fmt::format("if ({})", pred->compile()), [&]() { consume(); });
        });
    }
};

}  // namespace p2c