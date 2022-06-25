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

#include <json/single_include/nlohmann/json.hpp>

#include <mutex>
#include <tuple>
#include <memory>
#include <thread>
#include <type_traits>

namespace message_bus {

    int retry_max = 100;

    enum run_mode {
        stream,
        trigger
    };

    template<typename T>
    struct topic {

        topic(size_t buffer_size, run_mode mode_) : process_running(false), process_finished(true), mode(mode_) {
            topic_message_pool.create_val_pool(buffer_size, buffer_size);
            start();
            
        };
        ~topic() {
            if (process_thread.joinable()) {
                process_thread.join();
            }
        };

        void                                        run();
        void                                        start();
        void                                        stop();
        util::buffer::handle_value_type<T>          request_val();
        void                                        push_event(util::buffer::handle_value_type<T> event);
        void                                        send_message(T j);
        util::buffer::handle_value_type<T>          pop_event();
        bool                                        get_queue_state();
        void                                        trigger();
        void                                        register_handler(std::function<void(util::buffer::handle_value_type<T>  handler)>);

        protected:
            util::buffer::pool<T>                   topic_message_pool;
            util::buffer::queue<T>                  topic_message_queue;
            std::condition_variable                 cond;
            std::mutex                              mtx;
            std::atomic_bool                        process_running;
            std::atomic_bool                        process_finished;
            std::thread                             process_thread;
            run_mode                                mode;
            std::vector<std::function<void(util::buffer::handle_value_type<T> )>> func_list;
        
    };

    template<typename T>
    void topic<T>::start() {
        auto is_running = process_running.exchange(true);
        if (is_running || !process_finished.load()) {
            throw std::runtime_error("topic all ready running");
        }
        process_finished = false;
        process_running = true;
        process_thread = std::thread(&topic::run, this);
    }

    template<typename T>
    void topic<T>::stop() {
        while(!get_queue_state()) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        process_running = false;
        std::cout << "stop called " << process_running.load() << std::endl;
    }

    template<class T>
    void topic<T>::run() {
        do {
            if (!process_running.load()) {
                break;
            }
            if(!get_queue_state()) {
                util::buffer::handle_value_type<T> handle = pop_event();
                for(auto func: func_list) {
                    func(handle);
                }
            } 
        }while(true);
        process_finished = true;
    }

    template<typename T>
    util::buffer::handle_value_type<T> topic<T>::request_val() {
        return topic_message_pool.request_val_handle();
    }

    template<typename T>
    void topic<T>::push_event(util::buffer::handle_value_type<T> event) {
        topic_message_queue.push(event);
        if(mode == run_mode::stream) {
            cond.notify_one();
        }
    }

    template<typename T>
    void topic<T>::send_message(T j) {
        bool retry = false;
        int retry_counter = 0;
        util::buffer::handle_value_type<T> handle;
        do {
            try
            {
                handle = request_val();
                retry = false;
                *handle = j;
                push_event(handle);
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
            catch(const std::runtime_error &e)
            {
                std::cout << e.what() << std::endl;
                retry = true;
                retry_counter += 1;
                if (retry_counter > retry_max) 
                    throw std::runtime_error("retry on message send exceeded max");
                cond.notify_one();
            }
            catch(const std::exception &e)
            {
                std::cout << e.what() << std::endl;
            }
        }while(retry);
        
    }

    template<class T>
    util::buffer::handle_value_type<T> topic<T>::pop_event() {
        std::unique_lock<std::mutex> L(mtx);
        // possibly change to no timeout, but may cause a hang, tbd.
        cond.wait_for(L, std::chrono::milliseconds(3000), [this]{ return !get_queue_state() || !process_running.load(); });
        return topic_message_queue.pop_val();
    }

    template<class T>
    bool topic<T>::get_queue_state() {
        return topic_message_queue.empty();
    }

    template<class T>
    void topic<T>::trigger() {
        cond.notify_one();
    }

    template<class T>
    void topic<T>::register_handler(std::function<void(util::buffer::handle_value_type<T> )> handler) {
        func_list.push_back(std::function<void(util::buffer::handle_value_type<T>)>(handler));
    }

}  // namespace message
#endif  // TOPIC_H