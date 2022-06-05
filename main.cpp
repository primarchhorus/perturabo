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


struct test_type {
    int thingy;
};

message_bus::topic_manager manager;
std::atomic<bool> data_ready(false);
std::atomic<bool> stop(false);


void insert(int timeout) {
    

    std::string t_name = "test_topic";
    auto topic = manager.get_topic(t_name);

    long sum = 0;
    for (int i = 0; i < 100000; i++) {
        nlohmann::json j;
        j["thingy"] = i;
        j["message"] = "message thingy";
        j["state"] = true;
        j["timeout"] = timeout;
        topic->send_message(j);
        sum = sum + i;
        usleep(10);
    }
    std::cout << "insert thread " << std::this_thread::get_id() << ": " << sum << std::endl;
}

void run() {
    manager.create_topic("test_topic", 8096);
    auto topic = manager.get_topic("test_topic");
    std::thread th1(insert, 1); 
    std::thread th2(insert, 2); 
    std::thread th3(insert, 3); 
    std::thread th4(insert, 4); 
    std::thread th5(insert, 5); 
    th1.join();
    th2.join();
    th3.join();
    th4.join();
    th5.join();
    topic->stop();
    
}

int main(int argc, char *argv[]) {
    run();
    return 0;
}
