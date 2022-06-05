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

namespace message_bus {
    using topic_buffer_value = util::buffer::buffer_value_type<nlohmann::json>;
    using topic_buffer_value_ptr = util::buffer::buffer_value_ptr_type<nlohmann::json>;
    using topic_buffer = util::buffer::buffer_type<nlohmann::json>;
    using topic_value = util::buffer::handle_value_type<nlohmann::json>;
    using topic_handle = util::buffer::handle_type<nlohmann::json>;
    using topic_pool = util::buffer::pool<nlohmann::json>;
    using topic_queue = util::buffer::queue<nlohmann::json>;

    struct topic_base {}; 

    struct topic : topic_base{

        topic() : process_running(false), process_finished(true) {
            /*
            * add the size values to the construction later
            */
            topic_message_pool.create_val_pool(2048, 1024);
            start();
            
        };
        ~topic() {
            if (process_thread.joinable()) {
                process_thread.join();
            }
        };

        void                        run();
        void                        start();
        void                        stop();

        topic_value                 request_val();

        void                        push_event(topic_value event);

        void                        send_message(nlohmann::json j);

        topic_value                 pop_event();

        bool                        get_queue_state();

        topic_pool                  topic_message_pool;
        topic_queue                 topic_message_queue;
        std::condition_variable     cond;
        std::mutex                  mtx;

        protected:
            std::atomic_bool process_running;
            std::atomic_bool process_finished;
            std::thread process_thread;
        // std::thread                 process_thread;
        
    };

    void topic::start() {
        auto is_running = process_running.exchange(true);
        if (is_running || !process_finished.load()) {
            throw std::runtime_error("LM windowing consumer reading already running");
        }
        process_finished = false;
        process_running = true;
        process_thread = std::thread(&topic::run, this);
    }

    void topic::stop() {
        while(!get_queue_state()) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        process_running = false;
        std::cout << "stop called " << process_running.load() << std::endl;
    }

    void topic::run() {
        
        long sum = 0;
        do {
            if (!process_running.load()) {
                break;
            }
            if(!get_queue_state()) {
                topic_value handle = pop_event();
                sum = sum + handle->at("thingy").get<int>();
            } 
        }while(true);
        std::cout << "receive: " << sum << std::endl;
        process_finished = true;
    }

    topic_value topic::request_val() {
        return topic_message_pool.request_val_handle();
    }


    void topic::push_event(topic_value event) {
        topic_message_queue.push(event);
        cond.notify_one();
    }

    void topic::send_message(nlohmann::json j) {
        bool retry = false;
        message_bus::topic_value handle;
        do {
            try
            {
                std::cout << "insert " << j["thingy"] << std::endl;
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
                cond.notify_one();
            }
            catch(const std::exception &e)
            {
                std::cout << e.what() << std::endl;
            }
        }while(retry);
        
    }


    topic_value topic::pop_event() {
        std::unique_lock<std::mutex> L(mtx);
        std::cout << "pop " << process_running.load() << " " << get_queue_state() << std::endl;
        // possibly change to no timeout, but may cause an hang, tbd.
        cond.wait_for(L, std::chrono::milliseconds(3000), [this]{ return !get_queue_state() || !process_running.load(); });
        std::cout << "after cond " << (!process_running.load() && get_queue_state()) << std::endl;
        return topic_message_queue.pop_val();
    }


    bool topic::get_queue_state() {
        return topic_message_queue.empty();
    }
}  // namespace message
#endif  // TOPIC_H