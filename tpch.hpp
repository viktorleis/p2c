#pragma once
#include <string>
#include <map>
#include <utility>
#include <vector>
#include "types.hpp"
#include "io.hpp"

namespace p2c {

struct TPCH {
  template <typename T> using vec = DataColumn<T>;

public:

  struct {
    vec<Integer> p_partkey;
    vec<String> p_name;
    vec<String> p_mfgr;
    vec<String> p_brand;
    vec<String> p_type;
    vec<Integer> p_size;
    vec<String> p_container;
    vec<Numeric> p_retailprice;
    vec<String> p_comment;
    uint64_t tupleCount{0};
  } part;

  struct {
    vec<Integer> s_suppkey;
    vec<String> s_name;
    vec<String> s_address;
    vec<Integer> s_nationkey;
    vec<String> s_phone;
    vec<Numeric> s_acctbal;
    vec<String> s_comment;
    uint64_t tupleCount{0};
  } supplier;

  struct {
    vec<Integer> ps_partkey;
    vec<Integer> ps_suppkey;
    vec<Integer> ps_availqty;
    vec<Numeric> ps_supplycost;
    vec<String> ps_comment;
    uint64_t tupleCount{0};
  } partsupp;

  struct {
    vec<Integer> c_custkey;
    vec<String> c_name;
    vec<String> c_address;
    vec<Integer> c_nationkey;
    vec<String> c_phone;
    vec<Numeric> c_acctbal;
    vec<String> c_mktsegment;
    vec<String> c_comment;
    uint64_t tupleCount{0};
  } customer;

  struct {
    vec<BigInt> o_orderkey;
    vec<Integer> o_custkey;
    vec<Char> o_orderstatus;
    vec<Numeric> o_totalprice;
    vec<Date> o_orderdate;
    vec<String> o_orderpriority;
    vec<String> o_clerk;
    vec<Integer> o_shippriority;
    vec<String> o_comment;
    uint64_t tupleCount{0};
  } orders;

  struct {
    vec<BigInt> l_orderkey;
    vec<Integer> l_partkey;
    vec<Integer> l_suppkey;
    vec<Integer> l_linenumber;
    vec<Numeric> l_quantity;
    vec<Numeric> l_extendedprice;
    vec<Numeric> l_discount;
    vec<Numeric> l_tax;
    vec<Char> l_returnflag;
    vec<Char> l_linestatus;
    vec<Date> l_shipdate;
    vec<Date> l_commitdate;
    vec<Date> l_receiptdate;
    vec<String> l_shipinstruct;
    vec<String> l_shipmode;
    vec<String> l_comment;
    uint64_t tupleCount{0};
  } lineitem;

  struct {
    vec<Integer> n_nationkey;
    vec<String> n_name;
    vec<Integer> n_regionkey;
    vec<String> n_comment;
    uint64_t tupleCount{0};
  } nation;

  struct {
    vec<Integer> r_regionkey;
    vec<String> r_name;
    vec<String> r_comment;
    uint64_t tupleCount{0};
  } region;

    TPCH() {};

    inline static std::map<std::string,
                           std::vector<std::pair<std::string, Type>>>
        schema = {{"part",
                   {{"p_partkey", INTEGER},
                    {"p_name", STRING},
                    {"p_mfgr", STRING},
                    {"p_brand", STRING},
                    {"p_type", STRING},
                    {"p_size", INTEGER},
                    {"p_container", STRING},
                    {"p_retailprice", NUMERIC},
                    {"p_comment", STRING}}},
                  {"supplier",
                   {{"s_suppkey", INTEGER},
                    {"s_name", STRING},
                    {"s_address", STRING},
                    {"s_nationkey", INTEGER},
                    {"s_phone", STRING},
                    {"s_acctbal", NUMERIC},
                    {"s_comment", STRING}}},
                  {"partsupp",
                   {{"ps_partkey", INTEGER},
                    {"ps_suppkey", INTEGER},
                    {"ps_availqty", INTEGER},
                    {"ps_supplycost", NUMERIC},
                    {"ps_comment", STRING}}},
                  {"customer",
                   {{"c_custkey", INTEGER},
                    {"c_name", STRING},
                    {"c_address", STRING},
                    {"c_nationkey", INTEGER},
                    {"c_phone", STRING},
                    {"c_acctbal", NUMERIC},
                    {"c_mktsegment", STRING},
                    {"c_comment", STRING}}},
                  {"orders",
                   {{"o_orderkey", BIGINT},
                    {"o_custkey", INTEGER},
                    {"o_orderstatus", CHAR},
                    {"o_totalprice", NUMERIC},
                    {"o_orderdate", DATE},
                    {"o_orderpriority", STRING},
                    {"o_clerk", STRING},
                    {"o_shippriority", INTEGER},
                    {"o_comment", STRING}}},
                  {"lineitem",
                   {
                       {"l_orderkey", BIGINT},
                       {"l_partkey", INTEGER},
                       {"l_suppkey", INTEGER},
                       {"l_linenumber", INTEGER},
                       {"l_quantity", NUMERIC},
                       {"l_extendedprice", NUMERIC},
                       {"l_discount", NUMERIC},
                       {"l_tax", NUMERIC},
                       {"l_returnflag", CHAR},
                       {"l_linestatus", CHAR},
                       {"l_shipdate", DATE},
                       {"l_commitdate", DATE},
                       {"l_receiptdate", DATE},
                       {"l_shipinstruct", STRING},
                       {"l_shipmode", STRING},
                       {"l_comment", STRING},
                   }},
                  {"nation",
                   {
                       {"n_nationkey", INTEGER},
                       {"n_name", STRING},
                       {"n_regionkey", INTEGER},
                       {"n_comment", STRING},
                   }},
                  {"region",
                   {
                       {"r_regionkey", INTEGER},
                       {"r_name", STRING},
                       {"r_comment", STRING},
                   }}};
};

} // namespace p2c
