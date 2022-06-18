/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Copyright 2021 XIA LLC, All rights reserved.
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

/** @file log.cpp
 * @brief Implements logging infrastructure components.
 */


#include "util/buffer.hpp"
#include "util/log.hpp"
#include "topic.hpp"
#include "topic_manager.hpp"

#include <json/single_include/nlohmann/json.hpp>

#include <iostream>
#include <unistd.h>
#include <memory>
#include <thread>
#include <chrono>

template<typename T> using tmap = std::map<std::string, T>;
struct test_type {
    int thingy;
};

message_bus::topic_manager<nlohmann::json> manager;
std::atomic<bool> data_ready(false);
std::atomic<bool> stop(false);


void insert(int timeout) {
    

    std::string t_name = "test_topic";
    auto topic = manager.get_topic(t_name);

    long sum = 0;
    while (sum < 200000) {
        nlohmann::json j;
        j["thingy"] = sum;
        j["message"] = "message thingy";
        j["thread_id"] = timeout;
        j["timeout"] = timeout;
        topic->send_message(j);
        sum = sum + 1;
         std::this_thread::sleep_for(std::chrono::microseconds(16660));
    }
    std::cout << "insert thread " << std::this_thread::get_id() << ": " << sum << std::endl;
}

void game_loop() {
    std::string t_name = "test_topic";
    auto topic = manager.get_topic(t_name);
    int frame_count = 0;
    while(frame_count < 200000) {
        std::this_thread::sleep_for(std::chrono::microseconds(16660));
        topic->trigger();
        std::cout << "frame " << frame_count << std::endl;
        frame_count +=1;
    }
    
}

void run() {
    manager.create_topic("test_topic", 8096);
    auto topic = manager.get_topic("test_topic");
    std::thread frame(game_loop);
    std::thread th1(insert, 0); 
    std::thread th2(insert, 1); 
    std::thread th3(insert, 2); 
    std::thread th4(insert, 3); 
    std::thread th5(insert, 4); 

    th1.join();
    th2.join();
    th3.join();
    th4.join();
    th5.join();
    frame.join();
    topic->stop();
    
}

int main(int argc, char *argv[]) {
    run();
 
}
