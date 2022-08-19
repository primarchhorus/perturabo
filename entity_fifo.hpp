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

/** @file entity_fifo.h
 * @brief Memory pool for message entities.
 */

#ifndef ENTITY_FIFO_H
#define ENTITY_FIFO_H

#include <memory>
#include <mutex>
#include <thread>
#include <optional>
#include <atomic>
#include <deque>
#include <list>

namespace message_bus
{
    template <class T>
    class TrackingAllocator
    {
    public:
        using value_type = T;

        using pointer = T*;
        using const_pointer = const T*;

        using size_type = size_t;

        TrackingAllocator() = default;

        template <class U>
        TrackingAllocator(const TrackingAllocator<U> &other) {}

        ~TrackingAllocator() = default;

        pointer allocate(size_type numObjects)
        {
            mAllocations += numObjects;
            return static_cast<pointer>(operator new(sizeof(T) * numObjects));
        }

        void deallocate(pointer p, size_type numObjects)
        {
            operator delete(p);
        }

        size_type get_allocations() const
        {
            return mAllocations;
        }

    private:
        static size_type mAllocations;
    };

    template <class T>
    typename TrackingAllocator<T>::size_type TrackingAllocator<T>::mAllocations = 0;

    template <typename T>
    class entity_fifo
    {
    private:
  
        std::deque<T *, TrackingAllocator<T>> data_{};
        std::atomic<size_t> count{0};
        std::mutex mtx;

    public:
        struct releaser {
            entity_fifo &pool_;
            releaser(entity_fifo &pool__) : pool_(pool__) {}
            void operator()(T *buf) const { pool_.release(buf); }
        };
        entity_fifo(size_t capacity) : data_(capacity)
        {
            for (size_t i = 0; i < capacity; i++)
            {
                T *nt = new T;
                push(nt);
            }
        }

        bool push(T *val)
        {
            data_.insert(data_.begin(), val);
            count++;
            return true;
        }

        T *pop()
        {
            count--;
            return data_.front();
            ;
        }

        bool empty()
        {
            return (count.load() == 0);
        }

        std::optional<std::shared_ptr<T>> request_entity_handle()
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (empty())
            {
                return std::nullopt;
            }
            T *val = pop();
            data_.pop_front();
            return std::shared_ptr<T>(val, releaser(*this));
        }

        void release(T *entity)
        {
            std::lock_guard<std::mutex> lock(mtx);
            push(entity);
        }
    };

} // namespace message
#endif // ENTITY_FIFO_H