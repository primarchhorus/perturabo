/* SPDX-License-Identifier: Apache-2.0 */

/*
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

/** @file topic.h
 * @brief Defines the object that encapsulates a "topic" that producers send to
 * and consumers get from via the populator and the notifier respectively.
 */

#ifndef TOPIC_H
#define TOPIC_H

#include "util/buffer.hpp"

#include <json/single_include/nlohmann/json.hpp>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <tuple>
#include <type_traits>
#include <optional>

namespace message_bus {

int retry_max = 100;

enum run_mode { stream, trigger };

template <typename T> struct topic {

  topic(size_t buffer_size, run_mode mode_)
      : process_running(false), process_finished(true), mode(mode_) {
    topic_message_pool.create_entity_pool(buffer_size, buffer_size);
    start();
  };
  topic(const topic &) = delete;
  ~topic() {
    if (process_thread.joinable()) {
      process_thread.join();
    }
    topic_message_queue.flush();
    topic_message_pool.destroy();
  };

  void run();
  void start();
  void stop();
  std::optional<util::buffer::handle_entity_type<T>> request_entity();
  void push_entity(util::buffer::handle_entity_type<T> entity);
  void push_entity(util::buffer::handle_entity_type<T> entity, run_mode mode);
  util::buffer::handle_entity_type<T> get_topic_entity_handle();
  void send_entity(util::buffer::handle_entity_type<T> j);
  util::buffer::handle_entity_type<T> pop_entity();
  bool get_queue_state();
  void trigger();
  void register_handler(
      std::function<void(util::buffer::handle_entity_type<T> handler)>);

protected:
  util::buffer::pool<T> topic_message_pool;
  util::buffer::queue<T> topic_message_queue;
  std::condition_variable object_in_queue;
  std::condition_variable object_in_pool;
  std::mutex mtx;
  std::atomic_bool process_running;
  std::atomic_bool process_finished;
  std::thread process_thread;
  run_mode mode;
  std::vector<std::function<void(util::buffer::handle_entity_type<T>)>>
      func_list;
};

template <typename T> void topic<T>::start() {
  auto is_running = process_running.exchange(true);
  if (is_running || !process_finished.load()) {
    throw std::runtime_error("topic all ready running");
  }
  process_finished = false;
  process_running = true;
  process_thread = std::thread(&topic::run, this);
}

template <typename T> void topic<T>::stop() {
  while (!get_queue_state()) {
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }
  process_running = false;
}

template <class T> void topic<T>::run() {
  std::unique_lock<std::mutex> L(mtx);
  while (!process_finished) {
    object_in_queue.wait_for(L, std::chrono::milliseconds(100),
                             [this] { return !topic_message_queue.empty(); });
    if (!process_running.load()) { // This sucks evaluating this each iteration
                                   // but not sure of a better way out, blah
                                   // blah compiler complier, still though.
      break;
    }
    util::buffer::handle_entity_type<T> handle = pop_entity();
    for (auto func : func_list) {
      func(handle);
    }
  };
  process_finished = true;
}

template <typename T>
std::optional<util::buffer::handle_entity_type<T>> topic<T>::request_entity() {
  return topic_message_pool.request_entity_handle();
}

template <typename T>
void topic<T>::push_entity(util::buffer::handle_entity_type<T> event) {
  topic_message_queue.push(event);
  if (mode == run_mode::stream) {
    object_in_queue.notify_one();
  }
}

template <typename T>
void topic<T>::push_entity(util::buffer::handle_entity_type<T> event,
                          run_mode mode) {
  topic_message_queue.push(event);
  if (mode == run_mode::stream) {
    object_in_queue.notify_one();
  }
}

template <typename T>
util::buffer::handle_entity_type<T> topic<T>::get_topic_entity_handle() {
  bool retry = false;
  int retry_counter = 0;
  do {
      auto handle = request_entity();
      if (handle)
      {
        return handle.value() ;
      }
      else
      {
        std::unique_lock<std::mutex> pool_ready(mtx);
        object_in_pool.wait_for(pool_ready, std::chrono::microseconds(1000),
                                [this]
                                { return !topic_message_pool.empty(); });
        retry = true;
        retry_counter += 1;
      }
  }while(retry);
}

template <typename T>
void topic<T>::send_entity(util::buffer::handle_entity_type<T> j) {
  push_entity(j);
  // if (mode == run_mode::stream) {
  //   std::this_thread::sleep_for(std::chrono::microseconds(1));
  // }
}

template <class T> util::buffer::handle_entity_type<T> topic<T>::pop_entity() {
  return topic_message_queue.pop_entity();
}

template <class T> bool topic<T>::get_queue_state() {
  return topic_message_queue.empty();
}

template <class T> void topic<T>::trigger() { object_in_queue.notify_one(); }

template <class T>
void topic<T>::register_handler(
    std::function<void(util::buffer::handle_entity_type<T>)> handler) {
  func_list.push_back(
      std::function<void(util::buffer::handle_entity_type<T>)>(handler));
}

} // namespace message_bus
#endif // TOPIC_H