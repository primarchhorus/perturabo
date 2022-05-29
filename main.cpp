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


#include <util/buffer.hpp>
#include <util/log.hpp>
#include <topic.hpp>
#include <iostream>
#include <unistd.h>
#include <memory>
#include <thread>

struct test_type {
    int thingy;
};

message_bus::topic<test_type> new_topic;

void insert() {
    for (int i = 0; i < 1000000; i++) {
        std::shared_ptr<test_type> n = std::make_unique<test_type>();
        n->thingy = i;
        std::cout << "insert " << n->thingy << std::endl;
        new_topic.push_event(n);
    }
}

void recieve() {
    usleep(10000);
    while (!new_topic.get_queue_state()) {
        auto recv_handle = new_topic.pop_event();
        std::cout << "recieve " << recv_handle->thingy << std::endl;
        usleep(100);
    }
}

int main(int argc, char *argv[]) {

    
    std::thread th1(insert); 
    std::thread th2(recieve);  
    th1.join();
  
    // Wait for thread t2 to finish
    th2.join();
    return 0;
}
