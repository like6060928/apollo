/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "modules/routing/routing_component.h"

#include <utility>

#include "cyber/cyber.h"

#include "modules/common/adapters/adapter_gflags.h"
#include "modules/routing/common/routing_gflags.h"

DECLARE_string(flagfile);

namespace apollo {
namespace routing {

bool RoutingComponent::Init() {
  apollo::cyber::proto::RoleAttributes attr;
  attr.set_channel_name(FLAGS_routing_response_topic);
  auto qos = attr.mutable_qos_profile();
  qos->set_history(
      apollo::cyber::proto::QosHistoryPolicy::HISTORY_KEEP_LAST);
  qos->set_reliability(
      apollo::cyber::proto::QosReliabilityPolicy::RELIABILITY_RELIABLE);
  qos->set_durability(apollo::cyber::proto::QosDurabilityPolicy::
                          DURABILITY_TRANSIENT_LOCAL);
  response_writer_ = node_->CreateWriter<RoutingResponse>(attr);

  apollo::cyber::proto::RoleAttributes attr_history;
  attr_history.set_channel_name(FLAGS_routing_response_topic + "_history");
  auto qos_history = attr_history.mutable_qos_profile();
  qos_history->set_history(
      apollo::cyber::proto::QosHistoryPolicy::HISTORY_KEEP_LAST);
  qos_history->set_reliability(
      apollo::cyber::proto::QosReliabilityPolicy::RELIABILITY_RELIABLE);
  qos_history->set_durability(apollo::cyber::proto::QosDurabilityPolicy::
                          DURABILITY_TRANSIENT_LOCAL);

  response_history_writer_ = node_->CreateWriter<RoutingResponse>(attr_history);
  std::weak_ptr<RoutingComponent> self =
     std::dynamic_pointer_cast<RoutingComponent>(shared_from_this());
  timer_.reset(new ::apollo::cyber::Timer(100, [self, this]() {
    auto ptr = self.lock();
    if (ptr) {
      std::lock_guard<std::mutex> guard(this->mutex_);
      if (this->response_.get() != nullptr) {
        this->response_writer_->Write(response_);
      }
    }
  }, false));
  timer_->Start();

  return routing_.Init().ok() && routing_.Start().ok();
}

bool RoutingComponent::Proc(const std::shared_ptr<RoutingRequest>& request) {
  auto response = std::make_shared<RoutingResponse>();
  if (!routing_.Process(request, response.get())) {
    return false;
  }
  common::util::FillHeader(node_->Name(), response.get());
  response_writer_->Write(response);
  {
    std::lock_guard<std::mutex> guard(mutex_);
    response_ = std::move(response);
  }
  return true;
}

}  // namespace routing
}  // namespace apollo
