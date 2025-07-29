/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "moxygen/MoQServer.h"
#include "moxygen/relay/MoQRelay.h"

#include <folly/init/Init.h>

using namespace proxygen;

DEFINE_string(cert, "", "Cert path");
DEFINE_string(key, "", "Key path");
DEFINE_string(endpoint, "/moq-relay", "End point");
DEFINE_int32(port, 9668, "Relay Server Port");
DEFINE_bool(enable_cache, false, "Enable relay cache");

namespace {
using namespace moxygen;

class MoQRelayServer : MoQServer {
 public:
  MoQRelayServer()
      : MoQServer(FLAGS_port, FLAGS_cert, FLAGS_key, FLAGS_endpoint) {
    // std::cout << "🚫 FLAGS_port" << FLAGS_port << std::endl;
    // std::cout << "🚫 FLAGS_cert" << FLAGS_cert << std::endl;
    // std::cout << "🚫 FLAGS_key" << FLAGS_key << std::endl;
    // std::cout << "🚫 FLAGS_endpoint" << FLAGS_endpoint << std::endl;

      }

  void onNewSession(std::shared_ptr<MoQSession> clientSession) override {
    std::cout << "🚫 onNewSession" << std::endl;
    // printf("onNewSession");
    clientSession->setPublishHandler(relay_);
    clientSession->setSubscribeHandler(relay_);
  }

  void terminateClientSession(std::shared_ptr<MoQSession> session) override {
    relay_->removeSession(session);
  }

 private:
  std::shared_ptr<MoQRelay> relay_{
      std::make_shared<MoQRelay>(FLAGS_enable_cache)};
};
} // namespace

int main(int argc, char* argv[]) {
    std::cout << "🚫 moq relay server : main start" << std::endl;

  folly::Init init(&argc, &argv, true);
  MoQRelayServer moqRelayServer;
  folly::EventBase evb;
    std::cout << "🚫 before startRelayClient" << std::endl;

    
    std::cout << "🚫 after startRelayClient" << std::endl;
  evb.loopForever();
    std::cout << "🚫 after loop Forever" << std::endl;
  return 0;
}
