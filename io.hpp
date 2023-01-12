#pragma once

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <string>
#include <cassert>
#include <cstdint>

namespace p2c {

template <typename T = char> struct FileMapping {
  uintptr_t file_size;
  int handle;
  T *mapping;

  using iterator = T *;

public:
  FileMapping() : file_size(0), handle(-1), mapping(nullptr) {}
  FileMapping(const std::string &filename, int flags = 0, uintptr_t size = 0)
      : FileMapping() {
    open(filename.data(), flags, size);
  }
  FileMapping(uintptr_t size, int flags = 0) : FileMapping() {
    open(nullptr, flags, size);
  }
  FileMapping(FileMapping &&other) { *this = std::move(other); }
  ~FileMapping() { close(); }

  FileMapping &operator=(FileMapping &&other) {
    handle = other.handle;
    file_size = other.file_size;
    mapping = other.mapping;
    other.handle = -1;
    other.file_size = 0;
    other.mapping = nullptr;
    return *this;
  }

  inline bool is_backed() {
    return handle != -1;
  }

  inline void open(const char *file, int flags, std::size_t size) {
    close();
    int h = -1;
    if (file) {
      h = ::open(file, flags, 0655);
      if (h < 0) {
        auto err = errno;
        throw std::logic_error("Could not open file " + std::string(file) +
                               ":" + std::string(strerror(err)));
      }

      if (size == 0) {
        lseek(h, 0, SEEK_END);
        size = lseek(h, 0, SEEK_CUR);
      } else {
        auto res = ::ftruncate(h, size);
        if (res < 0) {
          auto err = errno;
          throw std::logic_error("Could not resize file: " +
                                 std::string(strerror(err)));
        }
      }
    }
    file_size = size;

    auto mmap_perm = (flags == 0)         ? PROT_READ
                     : (flags & O_WRONLY) ? PROT_WRITE
                                          : PROT_READ | PROT_WRITE;
    auto mmap_type = h == -1 ? MAP_PRIVATE | MAP_ANONYMOUS : MAP_SHARED;
    auto m = mmap(nullptr, file_size, mmap_perm, mmap_type, h, 0);
    if (m == MAP_FAILED) {
      auto err = errno;
      ::close(h);
      throw std::logic_error("Could not map file: " +
                             std::string(strerror(err)));
    }
    if (file_size > 1024 * 1024) {
      madvise(m, file_size, MADV_HUGEPAGE);
    }

    handle = h;
    mapping = static_cast<T *>(m);
  }

  inline void close() {
    if (mapping) {
      ::munmap(mapping, file_size);
      mapping = nullptr;
    }
    if (handle >= 0) {
      ::close(handle);
      handle = -1;
    }
  }

  inline void flush() const {
    if (handle >= 0) {
      ::fdatasync(handle);
    }
  }

  inline T *data() const { return mapping; }
  inline const iterator begin() const { return data(); }
  inline const iterator end() const {
    return data() + (this->file_size / sizeof(T));
  }
};

struct fixed_size {
  static constexpr bool IS_VARIABLE = false;
};

struct variable_size {
  static constexpr bool IS_VARIABLE = true;
  struct StringIndexSlot {
    uint64_t size;
    uint64_t offset;
  };

  struct StringData {
    uint64_t count;
    StringIndexSlot slot[];
  };
};

template <typename T> struct DataColumn : FileMapping<T> {
  using size_tag = fixed_size;
  using iterator = T *;
  using value_type = T;

  static constexpr uintptr_t GLOBAL_OVERHEAD = 0;
  static constexpr uintptr_t PER_ITEM_OVERHEAD = 0;

  uintptr_t count = 0ul;

  DataColumn() : FileMapping<T>(){}
  DataColumn(const char *filename, int flags = 0, uintptr_t size = 0)
      : FileMapping<T>(filename, flags, size),
        count(this->file_size / sizeof(T)) {
    assert(this->file_size % sizeof(T) == 0);
  }

  DataColumn(const DataColumn &) = delete;
  DataColumn(DataColumn &&other) { *this = std::move(other); }

  DataColumn &operator=(DataColumn &&o) {
    FileMapping<T>::operator=(std::move(o));
    count = o.count;
    o.count = 0;
    return *this;
  }

  uintptr_t size() const { return count; }
  const T &operator[](std::size_t idx) const { return this->data()[idx]; }
};

template <>
struct DataColumn<std::string_view> : FileMapping<variable_size::StringData> {
  using size_tag = variable_size;
  using Data = typename variable_size::StringData;
  using value_type = std::string_view;

  static constexpr uintptr_t PER_ITEM_OVERHEAD =
      sizeof(variable_size::StringIndexSlot);
  static constexpr uintptr_t GLOBAL_OVERHEAD = sizeof(uint64_t);

  struct iterator {
    uint64_t idx;
    Data *data;

    inline iterator &operator++() {
      ++idx;
      return *this;
    }

    inline iterator operator++(int) {
      auto prev = *this;
      ++idx;
      return prev;
    }

    std::string_view operator*() {
      auto slot = data->slot[idx];
      return std::string_view(reinterpret_cast<char *>(data) + slot.offset,
                              slot.size);
    }

    bool operator==(const iterator &rhs) {
      return this->idx == rhs.idx && this->data == rhs.data;
    }
  };

  DataColumn() : FileMapping<Data>(){}
  DataColumn(const char *filename, int flags = 0, uintptr_t size = 0)
      : FileMapping<Data>(filename, flags, size) {}

  DataColumn(const DataColumn &other) = delete;
  DataColumn(DataColumn &&other) : FileMapping<Data>(std::move(other)) {}

  DataColumn &operator=(DataColumn &&o) {
    FileMapping<Data>::operator=(std::move(o));
    return *this;
  };

  Data *data() const { return this->mapping; }
  uintptr_t size() const { return data()->count; }

  inline const iterator begin() const { return iterator{0, data()}; }
  inline const iterator end() const { return iterator{data()->count, data()}; }

  inline variable_size::StringIndexSlot &slot_at(std::size_t idx) const {
    return data()->slot[idx];
  }

  inline std::string_view operator[](std::size_t idx) const {
    auto slot = data()->slot[idx];
    return std::string_view(reinterpret_cast<char *>(data()) + slot.offset,
                            slot.size);
  }
};
} // namespace p2c
