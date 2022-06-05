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
 * @brief Defines functions and data structures for creating threaded data buffers
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
template<typename T> using buffer_value_type = T;
template<typename T> using buffer_value_ptr_type = buffer_value_type<T>*;
template<typename T> using buffer_type = std::vector<T>;
template<typename T> using buffer_ptr_type = buffer_type<T>*;
template<typename T> using handle_type = std::shared_ptr<buffer_type<T>>;
template<typename T> using handle_value_type = std::shared_ptr<buffer_value_type<T>>;

/**
 * @brief The buffer pool to manage the buffer workers.
 */
template<typename T>
struct pool {
    using buffer = buffer_type<T>;
    using value = buffer_value_type<T>;
    using value_ptr = buffer_value_ptr_type<T>;
    using buffer_ptr = buffer_ptr_type<T>;
    using handle = handle_type<T>;
    using value_handle = handle_value_type<T>;

    pool();
    ~pool();

    void create(const size_t number, const size_t size);
    void create_val_pool(const size_t number_, const size_t size_);
    void destroy();

    handle request();
    value_handle request_val_handle();

    bool valid() const {
        return number != 0;
    }

    bool empty() const {
        return count_.load() == 0;
    }

    bool full() const {
        return number != 0 && count_.load() == number;
    }

    size_t count() const {
        return count_.load();
    }

    size_t number;
    size_t size;

    void output(std::ostream& out);
    std::condition_variable ready;
    std::mutex ready_mtx;
    std::atomic_bool message_ready;

private:
    struct releaser {
        pool& pool_;
        releaser(pool& pool__) : pool_(pool__) {}
        void operator()(buffer* buf) const {
            pool_.release(buf);
        }
        void operator()(value* buf) const {
            pool_.release(buf);
        }
    };

    void release(buffer_ptr buf);
    void release(value_ptr val);

    std::atomic_size_t count_;

    std::forward_list<buffer_ptr> buffers;
    std::forward_list<value_ptr> values;

    lock_type lock;
    
};

/**
 * @brief Buffer queue for allocating work to the workers.
 */
template<typename T>
struct queue {
    using buffer_value_ptr = buffer_value_ptr_type<T>;
    using value = buffer_value_type<T>;
    using buffer = buffer_type<T>;
    using buffer_ptr = buffer_ptr_type<T>;
    using handle = handle_type<T>;
    using value_handle = handle_value_type<T>;

    using handles = std::list<handle>;
    using value_handles = std::list<value_handle>;

    std::condition_variable     pool_condition_active;
    std::mutex                  mtx;

    queue();
    queue(const queue& que);

    void push(handle buf);
    void push(value_handle val);
    handle pop();
    value_handle pop_val();

    void copy(buffer& to);
    void copy(buffer_value_ptr to, const size_t count);

    void compact();

    bool empty() const {
        return count_.load() == 0;
    }

    size_t size() const {
        return size_.load();
    }
    size_t count() const {
        return count_.load();
    }

    void flush();

    void output(std::ostream& out);

private:
    void copy_unprotected(buffer_value_ptr to, const size_t count);

    handles buffers;
    value_handles values;
    lock_type lock;
    std::atomic_size_t size_;
    std::atomic_size_t count_;
};

template<typename T>
pool<T>::pool() : number(0), size(0), count_(0) {}

template<typename T>
pool<T>::~pool() {
    try {
        destroy();
    } catch (...) {
        /* any error will be logged */
    }
}

template<typename T>
void pool<T>::create(const size_t number_, const size_t size_) {
    lock_guard guard(lock);
    if (valid()) {
        throw error("pool is already created");
    }
    number = number_;
    size = size_;
    for (size_t n = 0; n < number; ++n) {
        buffer_ptr buf = new buffer;
        buf->reserve(size);
        buffers.push_front(buf);
    }
    count_ = number;
}

template<typename T>
void pool<T>::create_val_pool(const size_t number_, const size_t size_) {
    lock_guard guard(lock);
    if (valid()) {
        throw error("pool is already created");
    }
    number = number_;
    size = size_;
    for (size_t n = 0; n < number; ++n) {
        value_ptr buf = new value;
        values.push_front(buf);
    }
    count_ = number;
    message_ready = true;
    ready.notify_one();
}

template<typename T>
void pool<T>::destroy() {
    lock_guard guard(lock);
    if (number > 0) {
        if (count_.load() != number) {
            throw error("pool destroy made while busy");
        }
        while (!buffers.empty()) {
            buffer_ptr buf = buffers.front();
            delete buf;
            buffers.pop_front();
        }
        while (!values.empty()) {
            value_ptr buf = values.front();
            delete buf;
            values.pop_front();
        }
        number = 0;
        size = 0;
        count_ = 0;
    }
}

template<typename T>
handle_type<T> pool<T>::request() {
    lock_guard guard(lock);
    if (empty()) {
        throw error("no buffers available");
    }
    count_--;
    buffer_ptr buf = buffers.front();
    buffers.pop_front();
    return handle(buf, releaser(*this));
}

template<typename T>
handle_value_type<T> pool<T>::request_val_handle() {
    lock_guard guard(lock);
    std::unique_lock<std::mutex> pool_ready(ready_mtx);
    ready.wait_for(pool_ready, std::chrono::milliseconds(100), [this]{ return message_ready.load(); });

    if (empty()) {
        message_ready = false;
        throw error("no message object available");
    }
    count_--;
    if (count_ < 10) {
        message_ready = false;
    }
    
    value_ptr buf = values.front();
    values.pop_front();
    return value_handle(buf, releaser(*this));
}

template<typename T>
void pool<T>::release(buffer_ptr buf) {
    buf->clear();
    lock_guard guard(lock);
    buffers.push_front(buf);
    count_++;
}

template<typename T>
void pool<T>::release(value_ptr buf) {
    lock_guard guard(lock);
    values.push_front(buf);
    message_ready = true;
    ready.notify_one();
    count_++;
}

template<typename T>
void pool<T>::output(std::ostream& out) {
    out << "count=" << count_.load() << " num=" << number << " size=" << size;
}

template<typename T>
queue<T>::queue() : size_(0), count_(0) {}

template<typename T>
queue<T>::queue(const queue& que) {
    buffers = que.buffers;
    size_ = que.size_.load();
    count_ = que.count_.load();

}

template<typename T>
void queue<T>::push(handle buf) {
    if (buf->size() > 0) {
        lock_guard guard(lock);
        buffers.push_back(buf);
        size_ += buf->size();
        ++count_;
    }
}

template<typename T>
void queue<T>::push(value_handle val) {
    lock_guard guard(lock);
    values.push_back(val);
    pool_condition_active.notify_all();
    size_ += sizeof(val);
    ++count_;
}

template<typename T>
handle_type<T> queue<T>::pop() {
    lock_guard guard(lock);
    handle buf = buffers.front();
    buffers.pop_front();
    size_ -= buf->size();
    --count_;
    return buf;
}

template<typename T>
handle_value_type<T> queue<T>::pop_val() {
    lock_guard guard(lock);
    value_handle val = values.front();
    values.pop_front();
    --count_;
    return val;
}

template<typename T>
void queue<T>::copy(buffer& to) {
    lock_guard guard(lock);
    /*
     * If the `to` size is 0 copy all the available data
     */
    size_t count = to.size();
    if (count == 0) {
        count = size_.load();
        to.resize(count);
    }
    copy_unprotected(to.data(), count);
}

template<typename T>
void queue<T>::copy(buffer_value_ptr to, const size_t count) {
    lock_guard guard(lock);
    copy_unprotected(to, count);
}

template<typename T>
void queue<T>::copy_unprotected(buffer_value_ptr to, const size_t count) {
    if (count > size_.load()) {
        throw error("not enough data in queue");
    }
    auto to_move = count;
    auto from_bi = buffers.begin();
    while (to_move > 0 && from_bi != buffers.end()) {
        auto from = *from_bi;
        if (to_move >= from->size()) {
            std::memcpy(to, from->data(), from->size() * sizeof(*to));
            to += from->size();
            to_move -= from->size();
            size_ -= from->size();
            count_--;
            from->clear();
        } else {
            std::memcpy(to, from->data(), to_move);
            std::move(from->begin() + to_move, from->end(), from->begin());
            from->resize(from->size() - to_move);
            to += from->size();
            to_move = 0;
            size_ -= to_move;
        }
        from_bi++;
    }
    if (from_bi != buffers.begin()) {
        buffers.erase(buffers.begin(), from_bi);
    }
}

template<typename T>
void queue<T>::compact() {
    lock_guard guard(lock);
    /*
     * Erasing elements from the queue's container invalidates the
     * iterators. After moving one or more buffers into another buffer and
     * removing them we start again, so we have valid iterators.
     */
    bool rerun = true;
    while (rerun) {
        rerun = false;
        auto to_bi = buffers.begin();
        while (to_bi != buffers.end()) {
            auto& to = *to_bi;
            if (to->capacity() - to->size() > 0) {
                auto erase_from = buffers.end();
                auto erase_to = buffers.end();
                auto to_move = to->capacity() - to->size();
                auto from_bi = to_bi;
                ++from_bi;
                while (to_move > 0 && from_bi != buffers.end()) {
                    auto from = *from_bi;
                    if (to_move >= from->size()) {
                        to->insert(to->end(), from->begin(), from->end());
                        to_move -= from->size();
                        if (erase_from == buffers.end()) {
                            erase_from = from_bi;
                        }
                        from_bi++;
                        erase_to = from_bi;
                        count_--;
                        from->clear();
                    } else {
                        to->insert(to->end(), from->begin(), from->begin() + to_move);
                        from->erase(from->begin(), from->begin() + to_move);
                        to_move = 0;
                    }
                }
                if (erase_from != buffers.end()) {
                    buffers.erase(erase_from, erase_to);
                    rerun = true;
                    break;
                }
            }
            to_bi++;
        }
    }
}

template<typename T>
void queue<T>::flush() {
    lock_guard guard(lock);
    buffers.clear();
}

template<typename T>
void queue<T>::output(std::ostream& out) {
    out << "count=" << count() << " size=" << size();
}

}  // namespace buffer
}  // namespace util

template<typename T>
std::ostream& operator<<(std::ostream& out, util::buffer::pool<T>& pool) {
    pool.output(out);
    return out;
}
template<typename T>
std::ostream& operator<<(std::ostream& out, util::buffer::queue<T>& queue) {
    queue.output(out);
    return out;
}

#endif  // BUFFER_H
