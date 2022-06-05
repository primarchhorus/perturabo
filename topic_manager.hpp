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
 * @brief Defines the object that encapsulates a "topic" that producers send to and consumers get from via the populator and the notifier respectively.
 */

#include "topic.hpp"

#include <json/single_include/nlohmann/json.hpp>

#include <map>

#ifndef TOPIC_MANAGER_H
#define TOPIC_MANAGER_H

namespace message_bus
{
    struct topic_manager {

        void create_topic(std::string topic_name, size_t topic_pool_size);

        std::shared_ptr<message_bus::topic> get_topic(std::string topic_name);

        private:
            std::map<std::string, std::shared_ptr<message_bus::topic>> topics;

    };
  
    void topic_manager::create_topic(std::string topic_name, size_t topic_pool_size) {
        std::shared_ptr<message_bus::topic> new_topic = std::make_shared<message_bus::topic>();
        topics.emplace(std::make_pair(topic_name, new_topic));
    }
   
    std::shared_ptr<message_bus::topic> topic_manager::get_topic(std::string topic_name) {
        auto search = topics.find(topic_name);
        if (search != topics.end()) {
            return search->second;
        } else {
            throw std::runtime_error("topic " + topic_name + " not found.");
        }
    }


    
} // namespace name


#endif  // TOPIC_MANAGER_H