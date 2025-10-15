// Maximilian Kuschewski, 2023
#pragma once
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "io.hpp"
#include "types.hpp"

namespace p2c {

class DatabaseAutoload {
   std::string base_path;

public:
   DatabaseAutoload(const std::string& base_path) : base_path(base_path) {}

   struct Relation {
      const DatabaseAutoload* loader;
      std::string name;
      Relation(const DatabaseAutoload* loader, const std::string& name) : loader(loader), name(name) {}
   };

   template<typename T>
   struct DataColumnFile : DataColumn<T> {
      DataColumnFile(const Relation* r, const std::string& name)
          : DataColumn<T>(r->loader->getFullPath(r->name, name)) {}
   };

   std::string getFullPath(const std::string& relation_name, const std::string& name) const {
      return base_path + '/' + relation_name + '/' + name + ".bin";
   }
};

class TPCH : DatabaseAutoload {
   template<typename T>
   using vec = DatabaseAutoload::DataColumnFile<T>;

public:
   struct : Relation {
      vec<int32_t> p_partkey{this, "p_partkey"};
      vec<std::string_view> p_name{this, "p_name"};
      vec<std::string_view> p_mfgr{this, "p_mfgr"};
      vec<std::string_view> p_brand{this, "p_brand"};
      vec<std::string_view> p_type{this, "p_type"};
      vec<int32_t> p_size{this, "p_size"};
      vec<std::string_view> p_container{this, "p_container"};
      vec<double> p_retailprice{this, "p_retailprice"};
      vec<std::string_view> p_comment{this, "p_comment"};
      uint64_t tupleCount{p_partkey.size()};
   } part{{this, "part"}};

   struct : Relation {
      vec<int32_t> s_suppkey{this, "s_suppkey"};
      vec<std::string_view> s_name{this, "s_name"};
      vec<std::string_view> s_address{this, "s_address"};
      vec<int32_t> s_nationkey{this, "s_nationkey"};
      vec<std::string_view> s_phone{this, "s_phone"};
      vec<double> s_acctbal{this, "s_acctbal"};
      vec<std::string_view> s_comment{this, "s_comment"};
      uint64_t tupleCount{s_suppkey.size()};
   } supplier{{this, "supplier"}};

   struct : Relation {
      vec<int32_t> ps_partkey{this, "ps_partkey"};
      vec<int32_t> ps_suppkey{this, "ps_suppkey"};
      vec<int32_t> ps_availqty{this, "ps_availqty"};
      vec<double> ps_supplycost{this, "ps_supplycost"};
      vec<std::string_view> ps_comment{this, "ps_comment"};
      uint64_t tupleCount{ps_partkey.size()};
   } partsupp{{this, "partsupp"}};

   struct : Relation {
      vec<int32_t> c_custkey{this, "c_custkey"};
      vec<std::string_view> c_name{this, "c_name"};
      vec<std::string_view> c_address{this, "c_address"};
      vec<int32_t> c_nationkey{this, "c_nationkey"};
      vec<std::string_view> c_phone{this, "c_phone"};
      vec<double> c_acctbal{this, "c_acctbal"};
      vec<std::string_view> c_mktsegment{this, "c_mktsegment"};
      vec<std::string_view> c_comment{this, "c_comment"};
      uint64_t tupleCount{c_custkey.size()};
   } customer{{this, "customer"}};

   struct : Relation {
      vec<int64_t> o_orderkey{this, "o_orderkey"};
      vec<int32_t> o_custkey{this, "o_custkey"};
      vec<char> o_orderstatus{this, "o_orderstatus"};
      vec<double> o_totalprice{this, "o_totalprice"};
      vec<date> o_orderdate{this, "o_orderdate"};
      vec<std::string_view> o_orderpriority{this, "o_orderpriority"};
      vec<std::string_view> o_clerk{this, "o_clerk"};
      vec<int32_t> o_shippriority{this, "o_shippriority"};
      vec<std::string_view> o_comment{this, "o_comment"};
      uint64_t tupleCount{o_orderkey.size()};
   } orders{{this, "orders"}};

   struct : Relation {
      vec<int64_t> l_orderkey{this, "l_orderkey"};
      vec<int32_t> l_partkey{this, "l_partkey"};
      vec<int32_t> l_suppkey{this, "l_suppkey"};
      vec<int32_t> l_linenumber{this, "l_linenumber"};
      vec<double> l_quantity{this, "l_quantity"};
      vec<double> l_extendedprice{this, "l_extendedprice"};
      vec<double> l_discount{this, "l_discount"};
      vec<double> l_tax{this, "l_tax"};
      vec<char> l_returnflag{this, "l_returnflag"};
      vec<char> l_linestatus{this, "l_linestatus"};
      vec<date> l_shipdate{this, "l_shipdate"};
      vec<date> l_commitdate{this, "l_commitdate"};
      vec<date> l_receiptdate{this, "l_receiptdate"};
      vec<std::string_view> l_shipinstruct{this, "l_shipinstruct"};
      vec<std::string_view> l_shipmode{this, "l_shipmode"};
      vec<std::string_view> l_comment{this, "l_comment"};
      uint64_t tupleCount{l_orderkey.size()};
   } lineitem{{this, "lineitem"}};

   struct : Relation {
      vec<int32_t> n_nationkey{this, "n_nationkey"};
      vec<std::string_view> n_name{this, "n_name"};
      vec<int32_t> n_regionkey{this, "n_regionkey"};
      vec<std::string_view> n_comment{this, "n_comment"};
      uint64_t tupleCount{n_nationkey.size()};
   } nation{{this, "nation"}};

   struct : Relation {
      vec<int32_t> r_regionkey{this, "r_regionkey"};
      vec<std::string_view> r_name{this, "r_name"};
      vec<std::string_view> r_comment{this, "r_comment"};
      uint64_t tupleCount{r_regionkey.size()};
   } region{{this, "region"}};

   TPCH(const std::string& path) : DatabaseAutoload(path){};

   using enum Type;
   inline static std::map<std::string, std::vector<std::pair<std::string, Type>>> schema = {
       {"part",
        {{"p_partkey", Integer},
         {"p_name", String},
         {"p_mfgr", String},
         {"p_brand", String},
         {"p_type", String},
         {"p_size", Integer},
         {"p_container", String},
         {"p_retailprice", Double},
         {"p_comment", String}}},
       {"supplier",
        {{"s_suppkey", Integer},
         {"s_name", String},
         {"s_address", String},
         {"s_nationkey", Integer},
         {"s_phone", String},
         {"s_acctbal", Double},
         {"s_comment", String}}},
       {"partsupp",
        {{"ps_partkey", Integer},
         {"ps_suppkey", Integer},
         {"ps_availqty", Integer},
         {"ps_supplycost", Double},
         {"ps_comment", String}}},
       {"customer",
        {{"c_custkey", Integer},
         {"c_name", String},
         {"c_address", String},
         {"c_nationkey", Integer},
         {"c_phone", String},
         {"c_acctbal", Double},
         {"c_mktsegment", String},
         {"c_comment", String}}},
       {"orders",
        {{"o_orderkey", BigInt},
         {"o_custkey", Integer},
         {"o_orderstatus", Char},
         {"o_totalprice", Double},
         {"o_orderdate", Date},
         {"o_orderpriority", String},
         {"o_clerk", String},
         {"o_shippriority", Integer},
         {"o_comment", String}}},
       {"lineitem",
        {
            {"l_orderkey", BigInt},
            {"l_partkey", Integer},
            {"l_suppkey", Integer},
            {"l_linenumber", Integer},
            {"l_quantity", Double},
            {"l_extendedprice", Double},
            {"l_discount", Double},
            {"l_tax", Double},
            {"l_returnflag", Char},
            {"l_linestatus", Char},
            {"l_shipdate", Date},
            {"l_commitdate", Date},
            {"l_receiptdate", Date},
            {"l_shipinstruct", String},
            {"l_shipmode", String},
            {"l_comment", String},
        }},
       {"nation",
        {
            {"n_nationkey", Integer},
            {"n_name", String},
            {"n_regionkey", Integer},
            {"n_comment", String},
        }},
       {"region",
        {
            {"r_regionkey", Integer},
            {"r_name", String},
            {"r_comment", String},
        }}};
};

}  // namespace p2c
