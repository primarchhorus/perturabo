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

/** @file topic_manager.hpp
 * @brief Defines the object that encapsulates a "topic" that producers send to
 * and consumers get from via the populator and the notifier respectively.
 */

#include "event.hpp"
#include "topic.hpp"

#include <json/single_include/nlohmann/json.hpp>

#include <map>

#ifndef TOPIC_MANAGER_H
#define TOPIC_MANAGER_H

namespace message_bus {
template <class T> struct topic_manager {

  void create_topic(std::string topic_name, size_t topic_pool_size,
                    message_bus::run_mode topic_mode);

  std::shared_ptr<message_bus::topic<T>> get_topic(std::string topic_name);

private:
  std::map<std::string, std::shared_ptr<message_bus::topic<T>>> topics;
};

template <typename T>
void topic_manager<T>::create_topic(std::string topic_name,
                                    size_t topic_pool_size,
                                    message_bus::run_mode topic_mode) {
  std::shared_ptr<message_bus::topic<T>> new_topic =
      std::make_shared<message_bus::topic<T>>(topic_pool_size, topic_mode);
  topics.emplace(std::make_pair(topic_name, new_topic));
}

template <typename T>
std::shared_ptr<message_bus::topic<T>>
topic_manager<T>::get_topic(std::string topic_name) {
  auto search = topics.find(topic_name);
  if (search != topics.end()) {
    return search->second;
  } else {
    throw std::runtime_error("topic " + topic_name + " not found.");
  }
}

} // namespace message_bus

#endif // TOPIC_MANAGER_H