#pragma once
#include "Operator.hpp"

namespace p2c {

struct Aggregate {
    IU* inputIU;  // IU to aggregate (is nullptr when aggFn==Count)
    IU resultIU;

    Aggregate(std::string name, IU* _inputIU) : inputIU(_inputIU), resultIU(name, _inputIU->type) {}

    virtual std::string genInitValue() = 0;
    virtual std::string genUpdate(std::string oldValueRef) = 0;
};

struct CountAggregate : Aggregate {
    CountAggregate(std::string name, IU* _inputIU) : Aggregate(name, _inputIU) {}
    std::string genInitValue() override { return "1"; }
    std::string genUpdate(std::string oldValueRef) { return oldValueRef + "+= 1;\n"; }
};

struct MinAggregate : Aggregate {
    MinAggregate(std::string name, IU* _inputIU) : Aggregate(name, _inputIU) {}
    std::string genInitValue() override { return fmt::format("{}", inputIU->varname); }
    std::string genUpdate(std::string oldValueRef) {
        return fmt::format("{} = std::min({}, {});\n", oldValueRef, oldValueRef, inputIU->varname);
    }
};

struct SumAggregate : Aggregate {
    SumAggregate(std::string name, IU* _inputIU) : Aggregate(name, _inputIU) {}
    std::string genInitValue() override { return fmt::format("{}", inputIU->varname); }
    std::string genUpdate(std::string oldValueRef) {
        return fmt::format("{} += {};\n", oldValueRef, inputIU->varname);
    }
};

// group by operator
struct GroupBy : public Operator {
    std::unique_ptr<Operator> input;
    IUSet groupKeyIUs;
    std::vector<std::unique_ptr<Aggregate>> aggs;
    IU ht{"aggHT", Type::Undefined};

    // constructor
    GroupBy(std::unique_ptr<Operator> input, const IUSet& groupKeyIUs)
        : input(std::move(input)), groupKeyIUs(groupKeyIUs) {}

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

    IUSet availableIUs() override { return groupKeyIUs | IUSet(resultIUs()); }

    void produce(const IUSet& required, ConsumerFn consume) override {
        // build hash table
        fmt::print("unordered_map<tuple<{}>, tuple<{}>> {};\n", formatTypes(groupKeyIUs.v),
                   formatTypes(resultIUs()), ht.varname);
        input->produce(groupKeyIUs | inputIUs(), [&]() {
            // insert tuple into hash table
            fmt::print("auto it = {}.find({{{}}});\n", ht.varname, formatVarnames(groupKeyIUs.v));
            genBlock(fmt::format("if (it == {}.end())", ht.varname), [&]() {
                std::vector<std::string> initValues;
                for (auto& agg : aggs) initValues.push_back(agg->genInitValue());

                // insert new group
                print("{}.insert({{{{{}}}, {{{}}}}});\n", ht.varname, formatVarnames(groupKeyIUs.v),
                      fmt::join(initValues, ","));
            });
            genBlock("else", [&]() {
                // update group
                unsigned i = 0;
                for (auto& agg : aggs) {
                    std::cout << agg->genUpdate(fmt::format("get<{}>(it->second)", i));
                    i++;
                }
            });
        });

        // iterate over hash table
        genBlock(fmt::format("for (auto& it : {})", ht.varname), [&]() {
            for (unsigned i = 0; i < groupKeyIUs.size(); i++) {
                IU* iu = groupKeyIUs.v[i];
                if (required.contains(iu)) provideIU(iu, fmt::format("get<{}>(it.first)", i));
            }
            unsigned i = 0;
            for (auto& agg : aggs) {
                provideIU(&agg->resultIU, fmt::format("get<{}>(it.second)", i));
                i++;
            }
            consume();
        });
    }

    IU* getIU(const std::string& attName) {
        for (auto& agg : aggs)
            if (agg->resultIU.name == attName) return &agg->resultIU;
        throw;
    }
};

}  // namespace p2c