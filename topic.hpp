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
 * @brief Defines the object that encapsulates a "topic" that producers send to and consumers get from via the populator and the notifier respectively.
 */

#ifndef TOPIC_H
#define TOPIC_H

#include "util/buffer.hpp"

template<typename T> using topic_buffer_value = util::buffer::buffer_value_type<T>;
template<typename T> using topic_buffer_value_ptr = util::buffer::buffer_value_ptr_type<T>;
template<typename T> using topic_buffer = util::buffer::buffer_type<T>;
template<typename T> using topic_handle = util::buffer::handle_type<T>;

namespace message_bus {
template <class T>
struct topic {

    topic() {};
    ~topic() {};

    void create_pool(int buffer_len, int pool_size);

    topic_handle<T> request_buffer();

    void push_buffer(topic_handle<T> handle);

    void push_event(std::shared_ptr<T> event);

    std::shared_ptr<T> pop_event();

    bool get_queue_state();

    private:
        util::buffer::pool<T> topic_pool;
        util::buffer::queue<T> topic_queue;
};

template<typename T>
void topic<T>::create_pool(int buffer_len, int pool_size) {
    topic_pool.create(buffer_len, pool_size);
}

template<typename T>
topic_handle<T> topic<T>::request_buffer() {
    return topic_pool.request();
}

template<typename T>
void topic<T>::push_buffer(topic_handle<T> handle) {
    topic_queue.push(handle);
}

template<typename T>
void topic<T>::push_event(std::shared_ptr<T> event) {
    topic_queue.push(event);
}

template<typename T>
std::shared_ptr<T> topic<T>::pop_event() {
    return topic_queue.pop_val();
}

template<typename T>
bool topic<T>::get_queue_state() {
    return topic_queue.empty();
}
}  // namespace message
#endif  // TOPIC_H