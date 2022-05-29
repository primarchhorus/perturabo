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
 * @brief Implements a "topic".
 */

#include "topic.hpp"

namespace message_bus {
    template <class T = int>
    topic::topic()
    {
        // auto value_type = message;
        // topic_buffer_value = util::buffer::buffer_value_type<T>;
        // topic_buffer_value_ptr = util::buffer::buffer_value_ptr_type<T>;
        // topic_buffer = util::buffer::buffer_type<T>;
        // topic_handle = util::buffer::handle_type<T>; 
        // topic_pool = util::buffer::pool<T>; 
        // topic_queue = util::buffer::queue<T>;
    }
} 