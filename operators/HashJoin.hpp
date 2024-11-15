#pragma once
#include "Operator.hpp"

namespace p2c {

// hash join operator
struct HashJoin : public Operator {
    std::unique_ptr<Operator> left;
    std::unique_ptr<Operator> right;
    std::vector<IU*> leftKeyIUs, rightKeyIUs;
    IU ht{"joinHT", Type::Undefined};

    // constructor
    HashJoin(std::unique_ptr<Operator> left, std::unique_ptr<Operator> right,
             const std::vector<IU*>& leftKeyIUs, const std::vector<IU*>& rightKeyIUs)
        : left(std::move(left)),
          right(std::move(right)),
          leftKeyIUs(leftKeyIUs),
          rightKeyIUs(rightKeyIUs) {}

    // destructor
    ~HashJoin() {}

    IUSet availableIUs() override { return left->availableIUs() | right->availableIUs(); }

    void produce(const IUSet& required, ConsumerFn consume) override {
        // figure out where required IUs come from
        IUSet leftRequiredIUs = (required & left->availableIUs()) | IUSet(leftKeyIUs);
        IUSet rightRequiredIUs = (required & right->availableIUs()) | IUSet(rightKeyIUs);
        IUSet leftPayloadIUs =
            leftRequiredIUs - IUSet(leftKeyIUs);  // these we need to store in hash table as payload

        // build hash table
        fmt::print("unordered_multimap<tuple<{}>, tuple<{}>> {};\n", formatTypes(leftKeyIUs),
                   formatTypes(leftPayloadIUs.v), ht.varname);
        left->produce(leftRequiredIUs, [&]() {
            // insert tuple into hash table
            fmt::print("{}.insert({{{{{}}}, {{{}}}}});\n", ht.varname, formatVarnames(leftKeyIUs),
                       formatVarnames(leftPayloadIUs.v));
        });

        // probe hash table
        right->produce(rightRequiredIUs, [&]() {
            // iterate over matches
            genBlock(
                fmt::format("for (auto range = {}.equal_range({{{}}}); range.first!=range.second; "
                            "range.first++)",
                            ht.varname, formatVarnames(rightKeyIUs)),
                [&]() {
                    // unpack payload
                    unsigned countP = 0;
                    for (IU* iu : leftPayloadIUs)
                        provideIU(iu, fmt::format("get<{}>(range.first->second)", countP++));
                    // unpack keys if needed
                    for (unsigned i = 0; i < leftKeyIUs.size(); i++) {
                        IU* iu = leftKeyIUs[i];
                        if (required.contains(iu))
                            provideIU(iu, fmt::format("get<{}>(range.first->first)", i));
                    }
                    // consume
                    consume();
                });
        });
    }
};

}  // namespace p2c