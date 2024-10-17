#include "csv.hpp"
#include "table-reader.hpp"
#include "tpch.hpp"

#include <iostream>
#include <array>
#include <filesystem>
#include <string_view>

using namespace p2c;
namespace fs = std::filesystem;

int main(int argc, char *argv[]) {
  // takes one argument: the directory containing all database tables
    assert(argc == 2);
    fs::path iprefix(argv[1]);
    // orders
    {
      orders::reader reader("output/orders/", (iprefix / "orders.tbl").c_str(), orders_c.data());
      auto rows = reader.read();
      std::cout << "read " << rows << " rows for orders" << std::endl;
    }
    // nation
    {
      nation::reader reader("output/nation/", (iprefix / "nation.tbl").c_str(), nation_c.data());
      auto rows = reader.read();
      std::cout << "read " << rows << " rows for nation" << std::endl;
    }
    // customer
    {
      customer::reader reader("output/customer/", (iprefix / "customer.tbl").c_str(), customer_c.data());
      auto rows = reader.read();
      std::cout << "read " << rows << " rows for customer" << std::endl;
    }
    // lineitem
    {
      lineitem::reader reader("output/lineitem/", (iprefix / "lineitem.tbl").c_str(), lineitem_c.data());
      auto rows = reader.read();
      std::cout << "read " << rows << " rows for lineitem" << std::endl;
    }
    // part
    {
     part::reader reader("output/part/", (iprefix / "part.tbl").c_str(), part_c.data());
      auto rows = reader.read();
      std::cout << "read " << rows << " rows for part" << std::endl;
    }
    // partsupp
    {
     partsupp::reader reader("output/partsupp/", (iprefix / "partsupp.tbl").c_str(), partsupp_c.data());
      auto rows = reader.read();
      std::cout << "read " << rows << " rows for partsupp" << std::endl;
    }
    // region
    {
      region::reader reader("output/region/", (iprefix / "region.tbl").c_str(), region_c.data());
      auto rows = reader.read();
      std::cout << "read " << rows << " rows for region" << std::endl;
    }
    // supplier
    {
      supplier::reader reader("output/supplier/", (iprefix / "supplier.tbl").c_str(), supplier_c.data());
      auto rows = reader.read();
      std::cout << "read " << rows << " rows for supplier" << std::endl;
    }
    // io::csv::read_file<'|', '\n', decltype(consume_cell)>(cfg.input.c_str(), nation_cols, consume_cell);
    return 0;
}
