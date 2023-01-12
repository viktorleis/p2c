#pragma once
#include <fcntl.h>
#include <string>
#include <vector>
#include "../io.hpp"
#include "csv.hpp"

namespace p2c {
static constexpr char delim = '|';
struct RunConfig {
  std::string input;
};

RunConfig read_config() {
  auto file = getenv("INPUT");
  assert(file);
  return RunConfig{.input = file};
}

template <typename T> struct ColumnOutput {
  using value_t = T;
  using page_t = DataColumn<T>;

  uintptr_t output_size;
  std::vector<T> items;

  ColumnOutput(unsigned expected_rows = 1024)
      : output_size(page_t::GLOBAL_OVERHEAD), items() {
    items.reserve(expected_rows);
  }

  ~ColumnOutput() {}

  bool append(const T &val) {
    items.push_back(val);
    if constexpr (page_t::size_tag::IS_VARIABLE) {
      output_size += val.size() + page_t::PER_ITEM_OVERHEAD;
    } else {
      output_size += sizeof(T);
    }
    return true;
  }

  page_t make_page(const char *filename) const {
    auto page = page_t(filename, O_CREAT | O_RDWR, output_size);
    if constexpr (page_t::size_tag::IS_VARIABLE) {
      auto idx = 0ul;
      auto offset = page.file_size;
      char *data = reinterpret_cast<char *>(page.data());
      for (auto &str : items) {
        offset -= str.size();
        std::copy(str.begin(), str.end(), data + offset);
        page.slot_at(idx++) = {str.size(), offset};
      }
      page.data()->count = idx;
    } else {
      std::copy(items.begin(), items.end(), page.begin());
    }
    return page;
  }
};

template <typename... Ts> struct TableImport {

  using tuple_type = std::tuple<Ts...>;
  using outputs_t = std::tuple<ColumnOutput<Ts>...>;

  outputs_t outputs;
  FileMapping<char> input;

  TableImport(const char *filename) : outputs(), input(filename) {}

  TableImport(size_t size) : outputs(), input(size) {}

  ~TableImport() {}

  inline unsigned read() {
    std::vector<unsigned> columns(sizeof...(Ts));
    for (auto i = 0u; i != sizeof...(Ts); ++i) {
      columns[i] = i;
    }

    unsigned rows = csv::read_file<delim>(input, columns, [&](unsigned col, csv::CharIter &pos) {
      fold_outputs(0, [&](auto &output, unsigned idx, unsigned num, unsigned v) {
        if (idx == col) {
          using value_t =
              typename std::remove_reference<decltype(output)>::type::value_t;
          csv::Parser<value_t> parser;
          auto value = parser.template parse_value<delim>(pos);
          output.append(value);
        }
        return 0;
      });
    });

    return rows;
  }
  inline unsigned operator()() { return read(); }

  inline constexpr static unsigned column_count() {
    return std::tuple_size_v<outputs_t>;
  }

  inline size_t row_count() { return std::get<0>(outputs).items.size(); }

  template <typename T, typename F, unsigned I = 0>
  constexpr inline T fold_outputs(T init_value, const F &fn) {
    if constexpr (I == sizeof...(Ts)) {
      return init_value;
    } else {
      return fold_outputs<T, F, I + 1>(
          fn(std::get<I>(outputs), I, sizeof...(Ts), init_value), fn);
    }
  }
}; // struct TableImport

template <typename... Ts> struct TableReader : TableImport<Ts...> {
  using super_t = TableImport<Ts...>;
  std::array<std::string, sizeof...(Ts)> output_files;

  TableReader(const std::string &output_prefix, const char *filename, char const* const* colnames)
      : super_t(filename) {
    // initialize output files
    std::filesystem::create_directories(output_prefix);
    this->fold_outputs(
        0, [&](const auto &output, unsigned idx, unsigned num, unsigned v) {
          output_files[idx] = output_prefix + colnames[idx] + ".bin";
          return 0;
        });
  }

  ~TableReader() {
    // write to files
    this->fold_outputs(
        0, [&](const auto &output, unsigned idx, unsigned num, unsigned v) {
          auto page = output.make_page(output_files[idx].c_str());
          page.flush();
          // for (auto item : page) {
          //   std::cout << "idx " << idx << " item " << item << std::endl;
          // }
          return 0;
        });
  }
}; // struct TableReader
} // namespace p2c
