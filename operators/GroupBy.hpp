#pragma once
#include "Operator.hpp"

namespace p2c {

// group by operator
struct GroupBy : public Operator {
    enum AggFunction { Sum, Count, Min };

    struct Aggregate {
        AggFunction aggFn;  // aggregate function
        IU* inputIU;        // IU to aggregate (is nullptr when aggFn==Count)
        IU resultIU;
    };

    std::unique_ptr<Operator> input;
    IUSet groupKeyIUs;
    std::vector<Aggregate> aggs;
    IU ht{"aggHT", Type::Undefined};

    // constructor
    GroupBy(std::unique_ptr<Operator> input, const IUSet& groupKeyIUs)
        : input(std::move(input)), groupKeyIUs(groupKeyIUs) {}

    // destructor
    ~GroupBy() {}

    void addCount(const std::string& name) {
        aggs.push_back({AggFunction::Count, nullptr, {name, Type::Integer}});
    }

    void addSum(const std::string& name, IU* inputIU) {
        aggs.push_back({AggFunction::Sum, inputIU, {name, inputIU->type}});
    }

    void addMin(const std::string& name, IU* inputIU) {
        aggs.push_back({AggFunction::Min, inputIU, {name, inputIU->type}});
    }

    std::vector<IU*> resultIUs() {
        std::vector<IU*> v;
        for (auto& [fn, inputIU, resultIU] : aggs) v.push_back(&resultIU);
        return v;
    }

    IUSet inputIUs() {
        IUSet v;
        for (auto& [fn, inputIU, resultIU] : aggs)
            if (inputIU) v.add(inputIU);
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
                for (auto& [fn, inputIU, resultIU] : aggs) {
                    switch (fn) {
                        case (AggFunction::Sum):
                            initValues.push_back(inputIU->varname);
                            break;
                        case (AggFunction::Min):
                            initValues.push_back(inputIU->varname);
                            break;
                        case (AggFunction::Count):
                            initValues.push_back("1");
                            break;
                    }
                }
                // insert new group
                print("{}.insert({{{{{}}}, {{{}}}}});\n", ht.varname, formatVarnames(groupKeyIUs.v),
                      fmt::join(initValues, ","));
            });
            genBlock("else", [&]() {
                // update group
                unsigned i = 0;
                for (auto& [fn, inputIU, resultIU] : aggs) {
                    switch (fn) {
                        case (AggFunction::Sum):
                            fmt::print("get<{}>(it->second) += {};\n", i, inputIU->varname);
                            break;
                        case (AggFunction::Min):
                            fmt::print("get<{}>(it->second) = std::min(get<{}>(it->second), {});\n",
                                       i, i, inputIU->varname);
                            break;
                        case (AggFunction::Count):
                            fmt::print("get<{}>(it->second)++;\n", i);
                            break;
                    }
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
            for (auto& [fn, inputIU, resultIU] : aggs) {
                provideIU(&resultIU, fmt::format("get<{}>(it.second)", i));
                i++;
            }
            consume();
        });
    }

    IU* getIU(const std::string& attName) {
        for (auto& [fn, inputIU, resultIU] : aggs)
            if (resultIU.name == attName) return &resultIU;
        throw;
    }
};

}  // namespace p2c