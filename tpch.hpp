#pragma once
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "io.hpp"
#include "types.hpp"

namespace p2c {

struct TPCH {
    template<typename T>
    using vec = DataColumn<T>;

   public:
    struct {
        vec<int32_t> p_partkey;
        vec<std::string_view> p_name;
        vec<std::string_view> p_mfgr;
        vec<std::string_view> p_brand;
        vec<std::string_view> p_type;
        vec<int32_t> p_size;
        vec<std::string_view> p_container;
        vec<double> p_retailprice;
        vec<std::string_view> p_comment;
        uint64_t tupleCount{0};
    } part;

    struct {
        vec<int32_t> s_suppkey;
        vec<std::string_view> s_name;
        vec<std::string_view> s_address;
        vec<int32_t> s_nationkey;
        vec<std::string_view> s_phone;
        vec<double> s_acctbal;
        vec<std::string_view> s_comment;
        uint64_t tupleCount{0};
    } supplier;

    struct {
        vec<int32_t> ps_partkey;
        vec<int32_t> ps_suppkey;
        vec<int32_t> ps_availqty;
        vec<double> ps_supplycost;
        vec<std::string_view> ps_comment;
        uint64_t tupleCount{0};
    } partsupp;

    struct {
        vec<int32_t> c_custkey;
        vec<std::string_view> c_name;
        vec<std::string_view> c_address;
        vec<int32_t> c_nationkey;
        vec<std::string_view> c_phone;
        vec<double> c_acctbal;
        vec<std::string_view> c_mktsegment;
        vec<std::string_view> c_comment;
        uint64_t tupleCount{0};
    } customer;

    struct {
        vec<int64_t> o_orderkey;
        vec<int32_t> o_custkey;
        vec<char> o_orderstatus;
        vec<double> o_totalprice;
        vec<date> o_orderdate;
        vec<std::string_view> o_orderpriority;
        vec<std::string_view> o_clerk;
        vec<int32_t> o_shippriority;
        vec<std::string_view> o_comment;
        uint64_t tupleCount{0};
    } orders;

    struct {
        vec<int64_t> l_orderkey;
        vec<int32_t> l_partkey;
        vec<int32_t> l_suppkey;
        vec<int32_t> l_linenumber;
        vec<double> l_quantity;
        vec<double> l_extendedprice;
        vec<double> l_discount;
        vec<double> l_tax;
        vec<char> l_returnflag;
        vec<char> l_linestatus;
        vec<date> l_shipdate;
        vec<date> l_commitdate;
        vec<date> l_receiptdate;
        vec<std::string_view> l_shipinstruct;
        vec<std::string_view> l_shipmode;
        vec<std::string_view> l_comment;
        uint64_t tupleCount{0};
    } lineitem;

    struct {
        vec<int32_t> n_nationkey;
        vec<std::string_view> n_name;
        vec<int32_t> n_regionkey;
        vec<std::string_view> n_comment;
        uint64_t tupleCount{0};
    } nation;

    struct {
        vec<int32_t> r_regionkey;
        vec<std::string_view> r_name;
        vec<std::string_view> r_comment;
        uint64_t tupleCount{0};
    } region;

    TPCH(){};

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
