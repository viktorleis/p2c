#pragma once

#include <algorithm>
#include <cstdint>
#include <cassert>
#include <limits>
#include <ostream>
#include <string>
#include <string_view>
#include <fmt/format.h>

namespace p2c {

    inline constexpr uint64_t murmurHash64(uint64_t k) {
        // MurmurHash64A
        const uint64_t m = 0xc6a4a7935bd1e995;
        const int r = 47;
        uint64_t h = 0x8445d61a4e774912 ^ (8 * m);
        k *= m;
        k ^= k >> r;
        k *= m;
        h ^= k;
        h *= m;
        h ^= h >> r;
        h *= m;
        h ^= h >> r;
        return h;
    }

    // --------------------------------------------------------------------------
    // Generic Operations on Types 
    // --------------------------------------------------------------------------

    enum Type : uint8_t { INTEGER, NUMERIC, CHAR, STRING, BIGINT, BOOL, DATE, UNDEFINED };
    constexpr char const* TYPE_NAMES[] = {"Integer", "Numeric", "Char", "String", "BigInt", "bool", "Date"};
    using Tid = uint64_t;

    // convert compile-time type to C++ type name
    inline std::string tname(Type t) {
      if (t < UNDEFINED) {
        return TYPE_NAMES[t];
      }
      throw "Unknown Type";
    }

    template <typename Type>
    Type stringToType(const char *str, uint32_t strLen);

    template <typename T>
    struct HashKey {
        uint64_t operator()(const T& value);
    };

    template <typename T>
    struct type_tag;

    // --------------------------------------------------------------------------
    // Char 
    // --------------------------------------------------------------------------

    using Char = char;
    template<>
    struct type_tag<char> {
        static constexpr Type TAG = CHAR;
    };

    template<>
    struct HashKey<char> {
        uint64_t operator()(char value) const {
            uint64_t r = 88172645463325252ull ^ value;
            r ^= (r << 13);
            r ^= (r >> 7);
            return (r ^= (r << 17));
        }
    };

    template<> inline Char stringToType(const char *str, uint32_t strLen) {
     assert(strLen == 1);
     return *str;
   }

    // --------------------------------------------------------------------------
    // BigInt 
    // --------------------------------------------------------------------------

    using BigInt = int64_t;
    template <>
    struct type_tag<BigInt> {
        static constexpr Type TAG = BIGINT;
    };
    template <>
    inline uint64_t HashKey<BigInt>::operator()(const BigInt& value) {
        return murmurHash64(value);
    }

    template <> inline BigInt stringToType(const char *str, uint32_t strLen) {
      auto iter = str, limit = str + strLen;

      // Trim WS
      while ((iter != limit) && ((*iter) == ' '))
        ++iter;
      while ((iter != limit) && ((*(limit - 1)) == ' '))
        --limit;

      // Check for a sign
      bool neg = false;
      if (iter != limit) {
        if ((*iter) == '-') {
          neg = true;
          ++iter;
        } else if ((*iter) == '+') {
          ++iter;
        }
      }

      // Parse
      if (iter == limit)
        throw "invalid number format: found non-integer characters";

      uint64_t result = 0;
      unsigned digitsSeen = 0;
      for (; iter != limit; ++iter) {
        char c = *iter;
        if ((c >= '0') && (c <= '9')) {
          result = (result * 10) + (c - '0');
          ++digitsSeen;
        } else if (c == '.') {
          break;
        } else {
          throw "invalid number format: invalid character in integer string";
        }
      }

      if (digitsSeen > 20 || result > std::numeric_limits<int64_t>::max())
        throw "invalid number format: too many characters (64bit integers can "
              "at "
              "most consist of 20 numeric characters)";

      BigInt r;
      r = neg ? -result : result;
      return r;
    }

    // --------------------------------------------------------------------------
    // Integer 
    // --------------------------------------------------------------------------

    using Integer = int32_t;
    template <>
    struct type_tag<Integer> {
        static constexpr Type TAG = INTEGER;
    };
    template <>
    inline uint64_t HashKey<Integer>::operator()(const Integer& value) {
        return murmurHash64(value);
    }
    template <> inline Integer stringToType(const char *str, uint32_t strLen) {
      return stringToType<BigInt>(str, strLen);
    }

    // --------------------------------------------------------------------------
    // String 
    // --------------------------------------------------------------------------

    using String = std::string_view;
    template <>
    struct type_tag<String> {
        static constexpr Type TAG = STRING;
    };
    template <>
    inline uint64_t HashKey<String>::operator()(const String& x) {
        unsigned result = 0;
        for (char c : x) {
            result = ((result << 5) | (result >> 27)) ^ (static_cast<unsigned char>(c));
        }
        return murmurHash64(result);
    }
    template<> inline String stringToType(const char* str,uint32_t strLen) { return {str, strLen}; };

    // --------------------------------------------------------------------------
    // Numeric 
    // --------------------------------------------------------------------------

    using Numeric = double;
    template <>
    struct type_tag<Numeric> {
        static constexpr Type TAG = NUMERIC;
    };
    template <>
    inline uint64_t HashKey<Numeric>::operator()(const Numeric& x) {
        return murmurHash64(*reinterpret_cast<const uint64_t*>(&x));
    }

    template <> inline Numeric stringToType(const char *str, uint32_t strLen) {
      auto iter = str, limit = str + strLen;

      // Trim WS
      while ((iter != limit) && ((*iter) == ' '))
        ++iter;
      while ((iter != limit) && ((*(limit - 1)) == ' '))
        --limit;

      char *end;
      return std::strtod(iter, &end);
    }

    // --------------------------------------------------------------------------
    // Date 
    // --------------------------------------------------------------------------
    class Date {
    public:
      static constexpr Type TAG = DATE;
      int32_t value;

      Date() {}
      Date(int32_t value) : value(value) {}
      Date(unsigned year, unsigned month, unsigned day) : Date(toInt(year, month, day)) {}

      /// Hash
      inline uint64_t hash() const;
      /// Comparison
      inline bool operator==(const Date &n) const { return value == n.value; }
      /// Comparison
      inline bool operator!=(const Date &n) const { return value != n.value; }
      /// Comparison
      inline bool operator<(const Date &n) const { return value < n.value; }
      /// Comparison
      inline bool operator<=(const Date &n) const { return value <= n.value; }
      /// Comparison
      inline bool operator>(const Date &n) const { return value > n.value; }
      /// Comparison
      inline bool operator>=(const Date &n) const { return value >= n.value; }
      /// Output
      friend std::ostream &operator<<(std::ostream &out, const Date &date) {
        unsigned year, month, day;
        fromInt(date.value, year, month, day);
        char buffer[30];
        snprintf(buffer, sizeof(buffer), "%04u-%02u-%02u", year, month, day);
        return out << buffer;
      }

      // Julian Day Algorithm from the Calendar FAQ
      static void fromInt(unsigned date, unsigned &year, unsigned &month,
                          unsigned &day) {
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
        return day + ((153 * m + 2) / 5) + (365 * y) + (y / 4) - (y / 100) +
               (y / 400) - 32045;
      }
    };

    template <>
    struct type_tag<Date> {
      static constexpr Type TAG = DATE;
    };
    template <>
    inline uint64_t HashKey<Date>::operator()(const Date& x) {
        return murmurHash64(x.value);
    }
    template <> inline Date stringToType(const char *str, uint32_t strLen) {
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
      if ((year > 9999) || (month < 1) || (month > 12) || (day < 1) ||
          (day > 31))
        throw "invalid date format";
      return Date(year, month, day);
    }

    // --------------------------------------------------------------------------
    // Other Utilities 
    // --------------------------------------------------------------------------

    // Hashing two integers
    inline uint64_t hash(Integer x, Integer y) {
        uint64_t k = x | ((static_cast<uint64_t>(y)) << 32);
        return murmurHash64(k);
    }

    // Hashing an entire tuple
    template <typename T, typename F, unsigned I = 0, typename... Args>
    constexpr inline T fold_tuple(const std::tuple<Args...>& tuple, T acc_or_init, const F& fn) {
        if constexpr (I == sizeof...(Args)) {
            return acc_or_init;
        } else {
            return fold_tuple<T, F, I + 1, Args...>(tuple, fn(acc_or_init, std::get<I>(tuple)), fn);
        }
    }
    template <typename... Args>
    struct HashKey<std::tuple<Args...>> {
        inline uint64_t operator()(const std::tuple<Args...>& args) const {
            return fold_tuple(args, 0ul, [](const uint64_t acc, const auto& val) -> uint64_t {
                HashKey<typename std::decay<decltype(val)>::type> hasher;
                return hasher(val) ^ acc;
            });
        }
    };
} // namespace p2c

// --------------------------------------------------------------------------
// std::hash for types
// --------------------------------------------------------------------------
namespace std {
template <typename T>
requires requires { p2c::type_tag<T>::TAG; }
struct hash<T> {
  std::size_t operator()(const T &k) const {
    using _hasher = ::p2c::HashKey<T>;
    static constexpr _hasher h = _hasher();
    return h(k);
  }
};

template <typename ... Args>
struct hash<std::tuple<Args...>> {
  std::size_t operator()(const std::tuple<Args...> &k) const {
    using _hasher = ::p2c::HashKey<std::tuple<Args...>>;
    static constexpr _hasher h = _hasher();
    return h(k);
  }
};

} // namespace std

// --------------------------------------------------------------------------
// custom c++20 formatter
// from https://fmt.dev/latest/api.html#udt
// --------------------------------------------------------------------------
template <>
struct fmt::formatter<p2c::Date> {
  // Parses format specifications; we have none at the moment
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
      return ctx.end();
  }
  template <typename FormatContext>
  auto format(const p2c::Date& d, FormatContext& ctx) const -> decltype(ctx.out()) {
      // ctx.out() is an output iterator to write to.
      unsigned year, month, day;
      p2c::Date::fromInt(d.value, year, month, day);
      return fmt::format_to(ctx.out(), "({:04}-{:02}-{:02})", year, month, day);
  }
};
