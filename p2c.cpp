// Viktor Leis, 2023

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <source_location>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

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
        gb->addCount("cnt");
        gb->addMin("min", op);
        gb->addSum("sum", op);

        IU* cnt_ = gb->getIU("cnt");
        IU* min_ = gb->getIU("min");
        IU* sum_ = gb->getIU("sum");

        auto sort = make_unique<Sort>(std::move(gb), std::vector<IU*>({cnt_}));
        produceAndPrint(std::move(sort), {os, cnt_, min_, sum_});
    }
    return 0;
}