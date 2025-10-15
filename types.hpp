#pragma once

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cstdint>
#include <limits>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <tuple>

namespace p2c {

struct date;

////////////////////////////////////////////////////////////////////////////////
// establish an absolute order for types so we can index them
// clang-format off
enum class Type : uint8_t { Integer = 0, Double = 1, Char = 2, String = 3, BigInt = 4, Bool = 5, Date = 6, Undefined = 7 };
static constexpr char const *TYPE_NAMES[] = {"int32_t", "double", "char", "std::string_view", "int64_t", "bool", "date"};
using TypeOrder = std::tuple<int32_t, double, char, std::string_view, int64_t, bool, date>;
// clang-format on
using Tid = uint64_t;

////////////////////////////////////////////////////////////////////////////////
// Generic Operations on Types

// given a type tag, return its index
inline constexpr uint8_t tindex(Type t) {
   return static_cast<uint8_t>(t);
}

// given a type tag, return its runtime name
inline std::string tname(Type t) {
   if (t < Type::Undefined) {
      return TYPE_NAMES[tindex(t)];
   }
   throw std::logic_error("Unknown Type");
}

template<typename T>
struct type_tag;

template<Type t>
using tag_type = typename std::tuple_element<tindex(t), TypeOrder>::type;

template<typename T>
concept is_p2c_type = requires { type_tag<T>::tag; };

// parse a string to a type
template<typename type>
requires is_p2c_type<type>
type stringToType(const char *str, uint32_t strLen) {
   type parsed;
   auto result = std::from_chars(str, str + strLen, &parsed, 10);
   if (result == std::errc()) {
      return parsed;
   } else {
      throw std::logic_error("Error while parsing " + std::string(str) + " to " + tname(type_tag<type>::tag));
   }
}

////////////////////////////////////////////////////////////////////////////////
// Char

template<>
struct type_tag<char> {
   using type = char;
   static constexpr Type tag = Type::Char;
};

template<>
inline char stringToType(const char *str, uint32_t strLen) {
   assert(strLen == 1);
   return *str;
}

////////////////////////////////////////////////////////////////////////////////
// BigInt

template<>
struct type_tag<int64_t> {
   using type = int64_t;
   static constexpr Type tag = Type::Integer;
};

////////////////////////////////////////////////////////////////////////////////
// Integer

template<>
struct type_tag<int32_t> {
   using type = int32_t;
   static constexpr Type tag = Type::Integer;
};

////////////////////////////////////////////////////////////////////////////////
// String

template<>
struct type_tag<std::string_view> {
   using type = std::string_view;
   static constexpr Type tag = Type::String;
};

template<>
inline std::string_view stringToType(const char *str, uint32_t strLen) {
   return {str, strLen};
};

////////////////////////////////////////////////////////////////////////////////
// Double

template<>
struct type_tag<double> {
   using type = double;
   static constexpr Type tag = Type::Double;
};

////////////////////////////////////////////////////////////////////////////////
// Date
template<>
struct type_tag<date> {
   using type = date;
   static constexpr Type tag = Type::Date;
};

struct date {
   int32_t value;

   date() {}
   date(int32_t value) : value(value) {}
   date(unsigned year, unsigned month, unsigned day) : date(toInt(year, month, day)) {}

   /// Comparison
   inline friend auto operator<=>(const date &d1, const date &d2) = default;

   /// Output
   friend std::ostream &operator<<(std::ostream &out, const date &date) {
      unsigned year, month, day;
      fromInt(date.value, year, month, day);
      char buffer[30];
      snprintf(buffer, sizeof(buffer), "%04u-%02u-%02u", year, month, day);
      return out << buffer;
   }

   // Julian Day Algorithm from the Calendar FAQ
   static void fromInt(unsigned date, unsigned &year, unsigned &month, unsigned &day) {
      unsigned a = date + 32044;
      unsigned b = (4 * a + 3) / 146097;
      unsigned c = a - ((146097 * b) / 4);
      unsigned d = (4 * c + 3) / 1461;
      unsigned e = c - ((1461 * d) / 4);
      unsigned m = (5 * e + 2) / 153;

      day = e - ((153 * m + 2) / 5) + 1;
      month = m + 3 - (12 * (m / 10));
      year = (100 * b) + d - 4800 + (m / 10);
   }

   // Julian Day Algorithm from the Calendar FAQ
   static unsigned toInt(unsigned year, unsigned month, unsigned day) {
      unsigned a = (14 - month) / 12;
      unsigned y = year + 4800 - a;
      unsigned m = month + (12 * a) - 3;
      return day + ((153 * m + 2) / 5) + (365 * y) + (y / 4) - (y / 100) + (y / 400) - 32045;
   }
};

// TODO use cdate for parsing
template<>
inline date stringToType(const char *str, uint32_t strLen) {
   auto iter = str, limit = str + strLen;
   // Trim WS
   while ((iter != limit) && ((*iter) == ' '))
      ++iter;
   while ((iter != limit) && ((*(limit - 1)) == ' '))
      --limit;
   // Year
   unsigned year = 0;
   while (true) {
      if (iter == limit)
         throw "invalid date format";
      char c = *(iter++);
      if (c == '-')
         break;
      if ((c >= '0') && (c <= '9')) {
         year = 10 * year + (c - '0');
      } else
         throw "invalid date format";
   }
   // Month
   unsigned month = 0;
   while (true) {
      if (iter == limit)
         throw "invalid date format";
      char c = *(iter++);
      if (c == '-')
         break;
      if ((c >= '0') && (c <= '9')) {
         month = 10 * month + (c - '0');
      } else
         throw "invalid date format";
   }
   // Day
   unsigned day = 0;
   while (true) {
      if (iter == limit)
         break;
      char c = *(iter++);
      if ((c >= '0') && (c <= '9')) {
         day = 10 * day + (c - '0');
      } else
         throw "invalid date format";
   }
   // Range check
   if ((year > 9999) || (month < 1) || (month > 12) || (day < 1) || (day > 31))
      throw "invalid date format";
   return date(year, month, day);
}
}  // namespace p2c

////////////////////////////////////////////////////////////////////////////////
// std::hash for missing types
namespace std {

template<>
struct hash<p2c::date> {
   inline size_t operator()(p2c::date date) const {
      hash<int32_t> value_hasher;
      return value_hasher(date.value);
   }
};

template<typename... Args>
struct hash<tuple<Args...>> {
   inline size_t operator()(const tuple<Args...> &args) const {
      return fold_tuple(args, 0ul, [](const size_t acc, const auto &val) -> uint64_t {
         hash<typename decay<decltype(val)>::type> hasher;
         return hasher(val) ^ acc;
      });
   }

private:
   template<typename T, typename F, unsigned I = 0, typename... Tuple>
   constexpr inline static T fold_tuple(const tuple<Tuple...> &tuple, T acc_or_init, const F &fn) {
      if constexpr (I == sizeof...(Args)) {
         return acc_or_init;
      } else {
         return fold_tuple<T, F, I + 1, Tuple...>(tuple, fn(acc_or_init, get<I>(tuple)), fn);
      }
   }
};
}  // namespace std

// --------------------------------------------------------------------------
// custom c++23 formatter
// from https://en.cppreference.com/w/cpp/utility/format/formatter.html
// --------------------------------------------------------------------------
template<>
struct std::formatter<p2c::date> {
   // Parses format specifications; we have none at the moment
   constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) { return ctx.end(); }
   template<typename FormatContext>
   auto format(const p2c::date &d, FormatContext &ctx) const -> decltype(ctx.out()) {
      // ctx.out() is an output iterator to write to.
      unsigned year, month, day;
      p2c::date::fromInt(d.value, year, month, day);
      return std::format_to(ctx.out(), "({:04}-{:02}-{:02})", year, month, day);
   }
};
