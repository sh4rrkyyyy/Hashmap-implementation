#pragma once

#include <bit>
#include <cstddef>
#include <stdexcept>
#include <sys/types.h>
#include <utility>
#include <vector>

template <class K, class V, class H = std::hash<K>> class HashMap {
public:
  HashMap(H hasher = H()) : hasher_(hasher) { data_.resize(8); }
  HashMap(std::size_t elements_count, H hasher = H()) : hasher_(hasher) {
    data_.resize(std::bit_ceil(elements_count * 2));
  }

  template <class Iterator>
  HashMap(Iterator first, Iterator last, H hasher = H()) : hasher_(hasher) {
    data_.resize(std::bit_ceil((size_t)std::distance(first, last) * 2));
    for (auto it = first; it != last; ++it) {
      insert(*it);
    }
  }

  HashMap(std::initializer_list<std::pair<K, V>> l, H hasher = H())
      : hasher_(hasher) {
    data_.resize(std::bit_ceil(l.size() * 2));
    for (auto &p : l) {
      insert(p);
    }
  }

  std::size_t size() const { return elements_count_; }

  bool empty() const { return size() == 0; }

  void insert(const K &key, V value) {

    auto bucket = hasher_(key) & (data_.size() - 1);
    for (size_t i = 0; i < data_.size(); ++i) {
      auto ind = (bucket + i) & (data_.size() - 1);
      if (data_[ind].state == State::EMPTY) {
        break;
      }
      if (data_[ind].state == State::BUSY && data_[ind].p.first == key) {
        data_[ind].p.second = value;
        return;
      }
    }
    bucket = hasher_(key) & (data_.size() - 1);
    size_t i = 0;
    for (; i < data_.size(); ++i) {
      auto ind = (bucket + i) & (data_.size() - 1);
      if (!(data_[ind].state == State::BUSY)) {
        data_[ind] = {{key, value}, State::BUSY, i};
        incr();
        if (occupied()) {
          resize();
        }
        break;
      }

      if (i == data_.size()) {
        resize();
        insert(key, value);
      }
    }
  }
  void insert(const std::pair<K, V> &elem) {
    return insert(elem.first, elem.second);
  }

  // returns true if key exists
  bool erase(const K &key) {
    auto bucket = hasher_(key) & (data_.size() - 1);
    for (size_t i = 0; i < data_.size(); ++i) {
      if (data_[(i + bucket) & (data_.size() - 1)].state == State::EMPTY) {
        return false;
      }
      if (data_[(i + bucket) & (data_.size() - 1)].p.first == key &&
          data_[(i + bucket) & (data_.size() - 1)].state == State::BUSY) {
        data_[(i + bucket) & (data_.size() - 1)].state = State::EMPTY;
        decr();
        auto pos = i + bucket;

        for (auto j = (pos + 1) & (data_.size() - 1);
             data_[j].state != State::EMPTY; j = (j + 1) & (data_.size() - 1)) {
          if (data_[j].state == State::EMPTY) {
            break;
          }
          if (data_[j].state != State::BUSY) {
            continue;
          }
          auto elem_bucket = hasher_(data_[j].p.first) & (data_.size() - 1);
          if (data_[j].distance >
              ((pos - elem_bucket + data_.size()) & (data_.size() - 1))) {
            std::swap(data_[j], data_[pos]);
            data_[pos].distance =
                (pos + data_.size() - elem_bucket) & (data_.size() - 1);
            pos = j;
          } else {
            break;
          }
        }
        return true;
      }
    }
    return false;
  }

  void clear() {
    for (size_t i = 0; i < data_.size(); ++i) {
      data_[i].state = State::EMPTY;
    }
    elements_count_ = 0;
  }

  V &operator[](const K &key) {
    auto bucket = hasher_(key) & (data_.size() - 1);
    for (size_t i = 0; i < data_.size(); ++i) {
      if (data_[(i + bucket) & (data_.size() - 1)].p.first == key &&
          data_[(i + bucket) & (data_.size() - 1)].state == State::BUSY) {
        return data_[(i + bucket) & (data_.size() - 1)].p.second;
      }
    }
    insert(key, V());
    for (size_t i = 0; i < data_.size(); ++i) {
      if (data_[(i + bucket) & (data_.size() - 1)].p.first == key &&
          data_[(i + bucket) & (data_.size() - 1)].state == State::BUSY) {
        return data_[(i + bucket) & (data_.size() - 1)].p.second;
      }
    }
    throw std::runtime_error("Insert failed in operator[]");
  }

  // return a copy of hasher object
  H hash_function() const { return hasher_; }

  // for testing purposes
  // returns the bucket this key resides on
  // or -1 if table doesn't contain key
  ssize_t bucket(const K &key) const {
    auto b = hasher_(key) & (data_.size() - 1);
    for (size_t i = 0; i < data_.size(); ++i) {
      if (data_[(i + b) & (data_.size() - 1)].p.first == key &&
          data_[(i + b) & (data_.size() - 1)].state == State::BUSY) {
        return (b + i) & (data_.size() - 1);
      }
    }
    return -1;
  }

private:
  void incr() { elements_count_++; }
  void decr() { elements_count_--; }
  bool occupied() { return 2 * elements_count_ >= data_.size(); }
  void resize() {
    auto old = std::move(data_);
    data_.resize(std::bit_ceil(old.size() * 2));
    clear();
    for (auto &item : old) {
      if (item.state == State::BUSY) {
        // std::cout << old.size() << std::endl;
        insert_without_resize(item.p.first, item.p.second);
      }
    }
  }
  void insert_without_resize(const K &key, V value) {
    auto bucket = hasher_(key) & (data_.size() - 1);
    size_t i = 0;
    for (; i < data_.size(); ++i) {
      auto ind = (bucket + i) & (data_.size() - 1);
      if (!(data_[ind].state == State::BUSY)) {
        data_[ind] = {{key, value}, State::BUSY, i};
        incr();
        break;
      }
    }
  }

  enum class State { EMPTY, BUSY };
  struct Item {
    std::pair<K, V> p;
    State state = State::EMPTY;
    size_t distance;
  };
  H hasher_;
  std::size_t size_ = 0;
  std::size_t elements_count_ = 0;
  std::vector<Item> data_;

public:
  class iterator {
  public:
    iterator(const std::vector<Item> *data, size_t pos)
        : data_(data), pos_(pos) {
      next();
    }
    iterator &operator++() {
      pos_++;
      next();
      return *this;
    }

    iterator operator++(int) {
      auto tmp = *this;
      ++(*this);
      return tmp;
    }

    auto &operator*() const { return (*data_)[pos_].p; }
    auto *operator->() const { return &(*data_)[pos_].p; }

    bool operator==(const iterator &rhs) const {
      return pos_ == rhs.pos_ && data_ == rhs.data_;
    }
    bool operator!=(const iterator &rhs) const { return !(*this == rhs); }

  private:
    void next() {
      while (pos_ < data_->size() && (*data_)[pos_].state != State::BUSY) {
        pos_++;
      }
    }
    const std::vector<Item> *data_;
    size_t pos_;
  };
  iterator find(const K &key) const {
    auto bucket = hasher_(key) & (data_.size() - 1);
    for (auto i = bucket; i < data_.size(); ++i) {
      if (data_[i].p.first == key && data_[i].state == State::BUSY) {
        return iterator(&data_, i);
      }
    }
    return end();
  }

  iterator begin() const { return iterator(&data_, 0); };

  iterator end() const { return iterator(&data_, data_.size()); };
};
