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

/** @file message.h
 * @brief Defines the object that encapsulates a "event" is inteneded to be a
 * base class that users can inherit from to add there own member variables or
 * methods.
 */

#ifndef EVENT_H
#define EVENT_H

namespace message_bus {
struct alignas(64) event_base {
  char message[16];
  int event_id;
  int incr;
  bool handled;
};
} // namespace message_bus
#endif // TOPIC_H