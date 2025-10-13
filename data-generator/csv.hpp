// Maximilian Kuschewski, 2023
#pragma once

#include <x86intrin.h>

#include <cassert>
#include <string>
#include <vector>
#include <thread>

#include <string>
#include <cassert>
#include <iostream>
#include <vector>
#include <array>
#include <utility>
#include <filesystem>

#include "../io.hpp"

namespace p2c::csv {
struct CharIter {
  const char *iter;
  const char *limit;

  template <typename T> static CharIter from_iterable(const T &iter) {
    return CharIter{iter.begin(), iter.end()};
  }
};

namespace {

template <char delim> inline void find(CharIter &pos) {
  auto &&[iter, limit] = pos;
  const __m256i search_mask = _mm256_set1_epi8(delim);
  auto limit32 = limit - 32;
  while (iter < limit32) {
    auto block = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(iter));
    uint32_t matches =
        _mm256_movemask_epi8(_mm256_cmpeq_epi8(block, search_mask));
    if (matches) {
      iter += __builtin_ctz(matches);
      return;
    } else {
      iter += 32;
    }
  }
  while ((iter != limit) && ((*iter) != delim)) {
    ++iter;
  }
}

template <char delim1, char delim2> inline void find_either(CharIter &pos) {
  auto &&[iter, limit] = pos;
  const __m256i search_mask1 = _mm256_set1_epi8(delim1);
  const __m256i search_mask2 = _mm256_set1_epi8(delim2);
  auto limit32 = limit - 32;
  while (iter < limit32) {
    auto block = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(iter));
    uint32_t matches =
        _mm256_movemask_epi8(_mm256_cmpeq_epi8(block, search_mask1)) |
        _mm256_movemask_epi8(_mm256_cmpeq_epi8(block, search_mask2));
    if (matches) {
      iter += __builtin_ctz(matches);
      return;
    }
    iter += 32;
  }
  while ((iter != limit) && ((*iter) != delim1) && ((*iter) != delim2)) {
    ++iter;
  }
}

template <char delim> inline void find_nth(CharIter &pos, unsigned n) {
  auto &&[iter, limit] = pos;
  const __m256i search_mask = _mm256_set1_epi8(delim);
  auto limit32 = limit - 32;
  while (iter < limit32) {
    auto block = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(iter));
    uint32_t matches =
        _mm256_movemask_epi8(_mm256_cmpeq_epi8(block, search_mask));
    if (matches) {
      unsigned hits = _mm_popcnt_u32(matches);
      if (hits < n) {
        n -= hits;
        iter += 32;
      } else {
        while (n > 1) {
          matches &= matches - 1;
          --n;
        }
        iter += __builtin_ctz(matches);
        return;
      }
    } else {
      iter += 32;
    }
  }
  for (; iter != limit && n; ++iter) {
    n -= ((*iter) == delim);
  }
  iter -= iter != limit;
}

template <char delim, char eol = '\n'>
inline size_t parse_unsigned(CharIter &pos) {
  auto &&[iter, limit] = pos;
  size_t v = *iter++ - '0';
  for (; iter != limit; ++iter) {
    char c = *iter;
    if (c == delim || c == eol)
      break;
    v = 10 * v + c - '0';
  }
  return v;
}

template <typename F, char delim, char eol = '\n'>
inline auto parse_from_to(F fn, CharIter &pos) {
  auto start = pos.iter;
  char *end = nullptr;
  auto result = fn(start, &end);
  assert(end != nullptr && (*end == delim || *end == eol));
  pos.iter = end;
  return result;
}

template <char delim, char eol = '\n', int base = 10>
inline long parse_int(CharIter &pos) {
  auto start = pos.iter;
  char *end = nullptr;
  auto result = std::strtol(start, &end, base);
  assert(end != nullptr && (*end == delim || *end == eol));
  pos.iter = end;
  return result;
}

template <char delim, char eol = '\n'>
inline double parse_double(CharIter &pos) {
  auto start = pos.iter;
  char *end = nullptr;
  auto result = std::strtod(start, &end);
  assert(end != nullptr && (*end == delim || *end == eol));
  pos.iter = end;
  return result;
}

template <typename Executor>
inline void
parallel_exec(const Executor &executor,
              unsigned thread_count = std::thread::hardware_concurrency() / 2) {
  std::vector<std::thread> threads;
  for (auto thread_id = 1u; thread_id != thread_count; ++thread_id) {
    threads.push_back(std::thread([thread_id, thread_count, &executor]() {
      executor(thread_id, thread_count);
    }));
  }
  executor(0, thread_count);
  for (auto &thread : threads) {
    thread.join();
  }
}

} // namespace

template <typename T> struct Parser {
  template <char delim, char eol = '\n'> T parse_value(CharIter &pos);
};

template <> struct Parser<char> {
  static constexpr char TYPE_NAME[] = "char";
  template <char delim, char eol = '\n'> inline int parse_value(CharIter &pos) {
    return *(pos.iter++);
  }
};

template <> struct Parser<unsigned long> {
  static constexpr char TYPE_NAME[] = "long.unsigned";
  template <char delim, char eol = '\n'>
  inline unsigned long parse_value(CharIter &pos) {
    return parse_unsigned<delim, eol>(pos);
  }
};

template <> struct Parser<unsigned> {
  static constexpr char TYPE_NAME[] = "int.unsigned";
  template <char delim, char eol = '\n'>
  inline unsigned parse_value(CharIter &pos) {
    return parse_unsigned<delim, eol>(pos);
  }
};

template <> struct Parser<long> {
  static constexpr char TYPE_NAME[] = "long";
  template <char delim, char eol = '\n'>
  inline long parse_value(CharIter &pos) {
    return parse_int<delim, eol>(pos);
  }
};

template <> struct Parser<int> {
  static constexpr char TYPE_NAME[] = "int";
  template <char delim, char eol = '\n'> inline int parse_value(CharIter &pos) {
    return parse_int<delim, eol>(pos);
  }
};

template <> struct Parser<double> {
  static constexpr char TYPE_NAME[] = "double";
  template <char delim, char eol = '\n'>
  inline double parse_value(CharIter &pos) {
    return parse_double<delim, eol>(pos);
  }
};

template <> struct Parser<std::string_view> {
  static constexpr char TYPE_NAME[] = "string";
  template <char delim, char eol = '\n'>
  inline std::string_view parse_value(CharIter &pos) {
    auto start = pos.iter;
    find_either<delim, eol>(pos);
    return std::string_view(start, pos.iter - start);
  }
};

template <char delim = ',', char eol = '\n', typename Consumer>
inline bool read_line(CharIter &pos, const std::vector<unsigned> &cols,
                      const Consumer &consumer) {
  unsigned skipped = 0;
  for (auto col : cols) {
    auto n = col - skipped;
    switch (n) {
    case 0:
      break;
    case 1:
      find<delim>(pos);
      break;
    default:
      find_nth<delim>(pos, n);
    }
    if (pos.iter >= pos.limit) {
      return false;
    }
    pos.iter += *pos.iter == delim;
    consumer(col, pos);
    skipped = col + 1;
    pos.iter += pos.iter < pos.limit && *pos.iter != eol;
  }
  if (pos.iter < pos.limit && *pos.iter != eol) {
    find<eol>(pos);
  }
  pos.iter += pos.iter != pos.limit;
  return true;
}

template <char delim = ',', char eol = '\n', typename Consumer>
inline std::size_t read_file(const char *filename,
                             const std::vector<unsigned> &cols,
                             const Consumer &consumer) {
  FileMapping<char> input(filename);
  auto pos = CharIter::from_iterable(input);
  std::size_t lines{0};
  while (read_line<delim, eol, Consumer>(pos, cols, consumer)) {
    ++lines;
  }
  return lines;
}

template <char delim = ',', char eol = '\n', typename Consumer>
inline std::size_t read_file(const FileMapping<char> &input,
                             const std::vector<unsigned> &cols,
                             const Consumer &consumer) {
  auto pos = CharIter::from_iterable(input);
  std::size_t lines{0};
  while (read_line<delim, eol, Consumer>(pos, cols, consumer)) {
    ++lines;
  }
  return lines;
}

} // namespace p2c::csv
