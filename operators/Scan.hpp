#pragma once
#include <string>
#include <vector>

#include "Operator.hpp"
#include "tpch.hpp"

namespace p2c {

// table scan operator
struct Scan : public Operator {
    // IU storage for all available attributes
    std::vector<IU> attributes;
    // relation name
    std::string relName;

    // constructor
    Scan(const std::string& relName) : relName(relName) {
        // get relation info from schema
        auto it = TPCH::schema.find(relName);
        assert(it != TPCH::schema.end());
        auto& rel = it->second;
        // create IUs for all available attributes
        attributes.reserve(rel.size());
        for (auto& att : rel) attributes.emplace_back(IU{att.first, att.second});
    }

    // destructor
    ~Scan() {}

    IUSet availableIUs() override {
        IUSet result;
        for (auto& iu : attributes) result.add(&iu);
        return result;
    }

    void produce(const IUSet& required, ConsumerFn consume) override {
        genBlock(fmt::format("for (uint64_t i = 0; i != db.{}.tupleCount; i++)", relName), [&]() {
            for (IU* iu : required) provideIU(iu, fmt::format("db.{}.{}[i]", relName, iu->name));
            consume();
        });
    }

    IU* getIU(const std::string& attName) {
        for (IU& iu : attributes)
            if (iu.name == attName) return &iu;
        throw;
    }
};

}  // namespace p2c