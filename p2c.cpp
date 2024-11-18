// Viktor Leis, 2023

#include <iostream>
#include <string>

#include "operators/GroupBy.hpp"
#include "operators/HashJoin.hpp"
#include "operators/Map.hpp"
#include "operators/Operator.hpp"
#include "operators/Scan.hpp"
#include "operators/Selection.hpp"
#include "operators/Sort.hpp"

using namespace std;
using namespace p2c;

int main(int argc, char* argv[]) {
    // ------------------------------------------------------------
    // Simple test query; should return the following on sf1 according to umbra:
    //   O 58 1333.79 2373761.38
    //   P 1005 6765.52 187335496.90
    //   F 144619 866.90 21856547600.45
    // ------------------------------------------------------------
    //   select o_orderstatus, count(*), min(o_totalprice), sum(o_totalprice)
    //   from orders
    //   where o_orderdate < date '1995-03-15'
    //     and o_orderpriority = '1-URGENT'
    //   group by o_orderstatus
    //   order by count(*)
    // ------------------------------------------------------------

    struct CountAggregate : Aggregate {
        CountAggregate(std::string name, IU* _inputIU) : Aggregate(name, _inputIU) {}
        std::string genInitValue() override { return "1"; }
        std::string genUpdate(std::string oldValueRef) {
            return fmt::format("{} += 1", oldValueRef);
        }
    };

    struct MinAggregate : Aggregate {
        MinAggregate(std::string name, IU* _inputIU) : Aggregate(name, _inputIU) {}
        std::string genInitValue() override { return fmt::format("{}", inputIU->varname); }
        std::string genUpdate(std::string oldValueRef) {
            return fmt::format("{} = std::min({}, {})", oldValueRef, oldValueRef, inputIU->varname);
        }
    };

    struct SumAggregate : Aggregate {
        SumAggregate(std::string name, IU* _inputIU) : Aggregate(name, _inputIU) {}
        std::string genInitValue() override { return fmt::format("{}", inputIU->varname); }
        std::string genUpdate(std::string oldValueRef) {
            return fmt::format("{} += {}", oldValueRef, inputIU->varname);
        }
    };

    {
        std::cout << "//" << stringToType<date>("1995-03-15", 10) << std::endl;
        auto o = make_unique<Scan>("orders");
        IU* od = o->getIU("o_orderdate");
        IU* op = o->getIU("o_totalprice");
        IU* os = o->getIU("o_orderstatus");
        IU* oprio = o->getIU("o_orderpriority");

        std::string prio = "1-URGENT";

        auto sel = make_unique<Selection>(
            std::move(o),
            makeCallExp("std::less()", od, stringToType<date>("1995-03-15", 10).value));
        sel = make_unique<Selection>(std::move(sel),
                                     makeCallExp("std::equal_to()", oprio, std::string_view(prio)));
        auto gb = make_unique<GroupBy>(std::move(sel), IUSet({os}));
        gb->addAggregate(std::make_unique<CountAggregate>("cnt", op));
        gb->addAggregate(std::make_unique<MinAggregate>("min", op));
        gb->addAggregate(std::make_unique<SumAggregate>("sum", op));

        IU* cnt_ = gb->getIU("cnt");
        IU* min_ = gb->getIU("min");
        IU* sum_ = gb->getIU("sum");

        auto sort = make_unique<Sort>(std::move(gb), std::vector<IU*>({cnt_}));
        produceAndPrint(std::move(sort), {os, cnt_, min_, sum_});
    }
    return 0;
}