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
        // template<typename T> using topic_buffer_value = util::buffer::buffer_value_type<T>;
        // template<typename T> using topic_buffer_value_ptr = util::buffer::buffer_value_ptr_type<T>;
        // template<typename T> using topic_buffer = util::buffer::buffer_type<T>;
        // template<typename T> using topic_value = util::buffer::handle_value_type<T>;
        // template<typename T> using topic_handle = util::buffer::handle_type<T>;
        // template<typename T> using topic_pool = util::buffer::pool<T>;
        // template<typename T> using topic_queue = util::buffer::queue<T>;

    int retry_max = 5;

    enum run_mode {
        stream,
        trigger
    };

    // template<typename T> using topic_map = std::map<std::string, std::shared_ptr<message_bus::topic<T>>>;
    
    struct topic_base {
        // virtual             ~topic_base() {};
        // // virtual void        run() = 0;
        // // virtual void        start() = 0;
        // virtual void        stop() = 0;
        // // virtual void        request_val() = 0;
        // // virtual void        push_event( ) = 0;
        // virtual void        send_message() = 0;
        // // virtual void pop_event() = 0;
        // // virtual bool        get_queue_state() = 0;
        // virtual void        trigger() = 0;

    }; 

    template<typename T>
    struct topic {

        topic(size_t buffer_size) : process_running(false), process_finished(true) {
            topic_message_pool.create_val_pool(buffer_size, buffer_size);
            start();
            
        };
        ~topic() {
            if (process_thread.joinable()) {
                process_thread.join();
            }
        };

        void                            run();
        void                            start();
        void                            stop();
        util::buffer::handle_value_type<T>                     request_val();
        void                            push_event(util::buffer::handle_value_type<T> event);
        void                            send_message(T j);
        util::buffer::handle_value_type<T>                     pop_event();
        bool                            get_queue_state();
        void                            trigger();

        protected:
            util::buffer::pool<T>                  topic_message_pool;
            util::buffer::queue<T>                 topic_message_queue;
            std::condition_variable     cond;
            std::mutex                  mtx;
            std::atomic_bool            process_running;
            std::atomic_bool            process_finished;
            std::thread                 process_thread;
            run_mode                    mode;
        
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
        int count[5] = {0,0,0,0,0};
        long sum = 0;
        do {
            if (!process_running.load()) {
                break;
            }
            if(!get_queue_state()) {
                util::buffer::handle_value_type<T> handle = pop_event();
                sum = sum + handle->at("thingy").template get<int>();
                int thread_id = handle->at("thread_id").template get<int>();
                count[thread_id] = sum;
                std::cout << thread_id << " recieve " << handle->at("thingy").template get<int>() << std::endl;
            } 
        }while(true);
        for(int i = 0; i < 5; i++) {
            std::cout << "final receive: " << count[i] << std::endl;
        }
        process_finished = true;

    }

    template<typename T>
    util::buffer::handle_value_type<T> topic<T>::request_val() {
        return topic_message_pool.request_val_handle();
    }

    template<typename T>
    void topic<T>::push_event(util::buffer::handle_value_type<T> event) {
        topic_message_queue.push(event);
        // cond.notify_one();
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
                std::this_thread::sleep_for(std::chrono::microseconds(j["timeout"]));
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

    // template<typename T>
    // struct is_int
    // {
    // static bool const val = false;
    // };

    // template<>
    // struct is_int<int>
    // {
    // static bool const val = true;
    // };

    // void fun(std::map<std::string, std::any> obj)
    // {
    //     std::map<std::string, std::any>::iterator it = obj.begin();
    //      while (it != obj.end())
    //     {
    //         if (it->second.type() == typeid(int)) {
    //             std::cout << "int" << std::endl;
    //         }

    //         if (it->second.type() == typeid(const char[11])) {
    //             std::cout << "char" << std::endl;
    //         }

    //         if (it->second.type() == typeid(std::string)) {
    //             std::cout << "string" << std::endl;
    //         }

    //         if (it->second.type() == typeid(double)) {
    //             std::cout << "double" << std::endl;
    //         }

    //         if (it->second.type() == typeid(float)) {
    //             std::cout << "float" << std::endl;
    //         }

    //         if (it->second.type() == typeid(bool)) {
    //             std::cout << "bool" << std::endl;
    //         }

    //         it++;
    //     }
    // // std::cout << (obj.type() == typeid(int)) << std::endl;
    // }

}  // namespace message
#endif  // TOPIC_H