/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Copyright 2021 util LLC, All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Apache-2.0 notice:
 *  Changed by Chris Johns. Copyright Chris Johns <chris@contemporary.software>
 */

/** @file buffer.hpp
 * @brief Defines functions and data structures for creating threaded data
 * buffers
 */

#ifndef BUFFER_H
#define BUFFER_H

#include <atomic>
#include <cstdint>
#include <cstring>
#include <forward_list>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <vector>
#include <deque>
#include <optional>

namespace util {
/**
 * @brief Handles threaded data buffers to read data from the External FIFO.
 */
namespace buffer {
/*
 * Local error
 */
typedef std::runtime_error error;
/**
 * @brief Defines the Buffer lock type
 */
typedef std::mutex lock_type;
/**
 * @brief A vector of lock_types is a lock_guard.
 */
typedef std::lock_guard<lock_type> lock_guard;
/**
 * @brief The buffer types.
 */
template <typename T> using buffer_entity_type = T;
template <typename T> using buffer_entity_ptr_type = buffer_entity_type<T> *;
template <typename T> using handle_entity_type = std::shared_ptr<buffer_entity_type<T>>;

/**
 * @brief The buffer pool to manage the buffer workers.
 */
template <typename T> struct pool {
  using entity = buffer_entity_type<T>;
  using entity_ptr = buffer_entity_ptr_type<T>;
  using entity_handle = handle_entity_type<T>;

  pool();
  ~pool();

  void create_entity_pool(const size_t number_, const size_t size_);
  void destroy();

  std::optional<entity_handle> request_entity_handle();

  bool valid() const { return number != 0; }

  bool empty() const { return count_.load() == 0; }

  bool full() const { return number != 0 && count_.load() == number; }

  size_t count() const { return count_.load(); }

  size_t number;
  size_t size;

  void output(std::ostream &out);

private:
  struct releaser {
    pool &pool_;
    releaser(pool &pool__) : pool_(pool__) {}
    void operator()(entity *buf) const { pool_.release(buf); }
  };

  void release(entity_ptr val);

  std::atomic_size_t count_;
  std::deque<entity_ptr> entities;
  lock_type lock;
};

/**
 * @brief Buffer queue for allocating work to the workers.
 */
template <typename T> struct queue {
  using buffer_entity_ptr = buffer_entity_ptr_type<T>;
  using entity = buffer_entity_type<T>;
  using entity_handle = handle_entity_type<T>;

  using entity_handles = std::deque<entity_handle>;

  queue();
  queue(const queue &que);

  void push(entity_handle val);

  entity_handle pop_entity();

  void copy(entity &to);
  void copy(buffer_entity_ptr to, const size_t count);

  void compact();

  bool empty() const { return count_.load() == 0; }

  size_t size() const { return size_.load(); }
  size_t count() const { return count_.load(); }

  void flush();

  void output(std::ostream &out);

private:
  void copy_unprotected(buffer_entity_ptr to, const size_t count);
  void copy_unprotected(entity to, const size_t count);

  entity_handles entities;
  lock_type lock;
  std::atomic_size_t size_;
  std::atomic_size_t count_;
};

template <typename T> pool<T>::pool() : number(0), size(0), count_(0) {}

template <typename T> pool<T>::~pool() {
  try {
    destroy();
  } catch (...) {
    /* any error will be logged */
  }
}

template <typename T>
void pool<T>::create_entity_pool(const size_t number_, const size_t size_) {
  lock_guard guard(lock);
  if (valid()) {
    throw -1;
  }
  number = number_;
  size = size_;
  for (size_t n = 0; n < number; ++n) {
    entity_ptr buf = new entity;
    entities.push_front(buf);
  }
  count_ = number;
}

template <typename T> void pool<T>::destroy() {
  lock_guard guard(lock);
  if (number > 0) {
    if (count_.load() != number) {
      throw error("pool destroy made while busy");
    }
    while (!entities.empty()) {
      entity_ptr buf = entities.front();
      delete buf;
      entities.pop_front();
    }
    number = 0;
    size = 0;
    count_ = 0;
  }
}

template <typename T> std::optional<handle_entity_type<T>> pool<T>::request_entity_handle() {
  lock_guard guard(lock);
  if (empty()) {
    return std::nullopt;
  }
  count_--;
  entity_ptr val = entities.front();
  entities.pop_front();
  return entity_handle(val, releaser(*this));
}


template <typename T> void pool<T>::release(entity_ptr buf) {
  lock_guard guard(lock);
  entities.push_front(buf);
  count_++;
}

template <typename T> void pool<T>::output(std::ostream &out) {
  out << "count=" << count_.load() << " num=" << number << " size=" << size;
}

template <typename T> queue<T>::queue() : size_(0), count_(0) {}

template <typename T> void queue<T>::push(entity_handle val) {
  lock_guard guard(lock);
  entities.push_back(val);
  size_ += sizeof(val);
  ++count_;
}

template <typename T> handle_entity_type<T> queue<T>::pop_entity() {
  lock_guard guard(lock);
  entity_handle val = entities.front();
  entities.pop_front();
  --count_;
  return val;
}

template <typename T> void queue<T>::copy(entity &to) {
  lock_guard guard(lock);
  /*
   * If the `to` size is 0 copy all the available data
   */

  copy_unprotected(to, sizeof(*to));
}

template <typename T>
void queue<T>::copy(buffer_entity_ptr to, const size_t count) {
  lock_guard guard(lock);
  copy_unprotected(to, count);
}

template <typename T>
void queue<T>::copy_unprotected(entity to, const size_t count) {
  if (count > size_.load()) {
    throw error("not enough data in queue");
  }
  std::cout << "copy_unprotected" << std::endl;
  auto to_move = count;
  auto from_bi = entities.begin();
  while (to_move > 0 && from_bi != entities.end()) {
    auto from = *from_bi;
    if (to_move >= sizeof(*from)) {
      std::memcpy(&to, from, sizeof(*from) * sizeof(*to));
      to +=sizeof(*from);
      to_move -= sizeof(*from);
      size_ -= sizeof(*from);
      count_--;
      from->clear();
    } else {
      std::memcpy(&to, sizeof(*from), to_move);
      to += sizeof(*from);
      to_move = 0;
      size_ -= to_move;
    }
    from_bi++;
  }
  if (from_bi != entities.begin()) {
    entities.erase(entities.begin(), from_bi);
  }
}

template <typename T> void queue<T>::flush() {
  lock_guard guard(lock);
  entities.clear();
}

template <typename T> void queue<T>::output(std::ostream &out) {
  out << "count=" << count() << " size=" << size();
}

} // namespace buffer
} // namespace util

template <typename T>
std::ostream &operator<<(std::ostream &out, util::buffer::pool<T> &pool) {
  pool.output(out);
  return out;
}
template <typename T>
std::ostream &operator<<(std::ostream &out, util::buffer::queue<T> &queue) {
  queue.output(out);
  return out;
}

#endif // BUFFER_H
