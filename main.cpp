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

#include "event.hpp"
#include "topic.hpp"
#include "topic_manager.hpp"
#include "util/buffer.hpp"
#include "util/log.hpp"

#include <json/single_include/nlohmann/json.hpp>

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <unistd.h>

message_bus::topic_manager<message_bus::event_base> manager;

std::atomic<long long> final_sum{0};
std::atomic<long long> check{0};
int loop_count = 100000;

void insert(int timeout) {
  std::string t_name = "test_topic";
  auto topic = manager.get_topic(t_name);

  long sum = 0;
  long counter = 0;
  while (sum <= loop_count) {
    check.fetch_add(sum);
    message_bus::event_base j;
    strcpy(j.message, "message thingy");
    j.event_id = timeout;
    j.incr = sum;
    topic->send_message(j);
    sum = sum + 1;
    counter = counter + timeout;
  }
}

void trigger_loop() {
  std::string t_name = "test_topic";
  auto topic = manager.get_topic(t_name);
  int frame_count = 0;
  while (frame_count <= loop_count) {
    std::this_thread::sleep_for(std::chrono::microseconds(16660));
    topic->trigger();
    std::cout << "frame " << frame_count << std::endl;
    frame_count += 1;
  }
}

void handler(std::shared_ptr<message_bus::event_base> message) {
  final_sum.fetch_add(message->incr);
  int thread_id = message->event_id;
}

void run() {
  manager.create_topic("test_topic", 65536, message_bus::run_mode::stream);
  auto topic = manager.get_topic("test_topic");
  std::function<void(std::shared_ptr<message_bus::event_base>)> f = handler;
  topic->register_handler(f);

  std::thread th1(insert, 1);
  std::thread th2(insert, 2);
  std::thread th3(insert, 3);
  std::thread th4(insert, 4);
  std::thread th5(insert, 5);
  std::thread th6(insert, 6);
  std::thread th7(insert, 7);
  std::thread th8(insert, 8);
  std::thread th9(insert, 9);

  th1.join();
  th2.join();
  th3.join();
  th4.join();
  th5.join();
  th6.join();
  th7.join();
  th8.join();
  th9.join();

  topic->stop();
  std::cout << "receive sum " << final_sum.load() << " send sum "
            << check.load() << " percent receieved: " << (100 * (  check.load() / final_sum.load())) << "%" <<std::endl;
}

int main(int argc, char *argv[]) { run(); }
