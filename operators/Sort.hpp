#pragma once
#include "Operator.hpp"

namespace p2c {

// sort operator
struct Sort : public Operator {
    std::unique_ptr<Operator> input;
    std::vector<IU*> keyIUs;
    IU v{"vector", Type::Undefined};

    // constructor
    Sort(std::unique_ptr<Operator> input, const std::vector<IU*>& keyIUs)
        : input(std::move(input)), keyIUs(keyIUs) {}

    // destructor
    ~Sort() {}

    IUSet availableIUs() override { return input->availableIUs(); }

    void produce(const IUSet& required, ConsumerFn consume) override {
        // compute IUs
        IUSet restIUs = required - IUSet(keyIUs);
        std::vector<IU*> allIUs = keyIUs;
        allIUs.insert(allIUs.end(), restIUs.v.begin(), restIUs.v.end());

        // collect tuples
        fmt::print("vector<tuple<{}>> {};\n", formatTypes(allIUs), v.varname);
        input->produce(IUSet(allIUs), [&]() {
            fmt::print("{}.push_back({{{}}});\n", v.varname, formatVarnames(allIUs));
        });

        // sort
        fmt::print(
            "sort({0}.begin(), {0}.end(), [](const auto& t1, const auto& t2) {{ return t1<t2; "
            "}});\n",
            v.varname);

        // iterate
        genBlock(fmt::format("for (auto& t : {})", v.varname), [&]() {
            for (unsigned i = 0; i < allIUs.size(); i++)
                if (required.contains(allIUs[i]))
                    provideIU(allIUs[i], fmt::format("get<{}>(t)", i));
            consume();
        });
    };
};

}  // namespace p2c