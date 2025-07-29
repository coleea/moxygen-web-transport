/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "moxygen/MoQServer.h"
#include <proxygen/lib/http/webtransport/QuicWebTransport.h>

#include <utility>

using namespace quic::samples;
using namespace proxygen;

namespace moxygen {

MoQServer::MoQServer(
    uint16_t port,
    std::string cert,
    std::string key,
    std::string endpoint)
    : endpoint_(std::move(endpoint)) {


   std::cout << "in moqserver initializer" << std::endl ;

   
     std::cout << "🚫 port"     << port << std::endl;
     std::cout << "🚫 cert"     << cert << std::endl;
     std::cout << "🚫 key"      << key << std::endl;
     std::cout << "🚫 endpoint" << endpoint << std::endl;

  params_.localAddress.emplace();
  params_.localAddress->setFromLocalPort(port);
  params_.serverThreads = 255;
  params_.certificateFilePath = std::move(cert);
  params_.keyFilePath = std::move(key);
  params_.txnTimeout = std::chrono::seconds(60);
  
  //  moxygen/MoQServer.cpp:32에서 지원하는 ALPN(Application-Layer Protocol Negotiation) 프로토콜로 
  // "h3" (HTTP/3)를 설정합니다. 
  //  WebTransport는 HTTP/3 위에서 동작합니다.
  params_.supportedAlpns = {"h3", "moq-00"};

   
   std::cout << "before HQServerTransportFactory" << std::endl ;

  auto factory = std::make_unique<HQServerTransportFactory>(
      params_, [this](HTTPMessage*) { return new Handler(*this); }, nullptr);

   std::cout << "before factory->addAlpnHandler" << std::endl ;

  
  factory->addAlpnHandler(
      {"moq-00"},
      [this](
          std::shared_ptr<quic::QuicSocket> quicSocket,
          wangle::ConnectionManager*) {
   std::cout << "in factory->addAlpnHandler's lambda function" << std::endl ;

        createMoQQuicSession(std::move(quicSocket));
      });

   std::cout << "before create hqserver" << std::endl ;

  //  proxygen httpserver
  hqServer_ = std::make_unique<HQServer>(params_, std::move(factory));

   std::cout << "before  hqserver->start()" << std::endl ;

  hqServer_->start();
   std::cout << "after  hqserver->start()" << std::endl ;
   std::cout << "getAddress() : " <<  hqServer_->getAddress() << std::endl ;

   

}

void MoQServer::stop() {
  hqServer_->stop();
}

void MoQServer::createMoQQuicSession(
    std::shared_ptr<quic::QuicSocket> quicSocket) {

  XLOG(DBG1) << __func__;

  auto qevb = quicSocket->getEventBase();
  auto ts = quicSocket->getTransportSettings();
  // TODO make this configurable, also have a shared pacing timer per thread.
  ts.defaultCongestionController = quic::CongestionControlType::Copa;
  ts.copaDeltaParam = 0.05;
  ts.pacingEnabled = true;
  ts.experimentalPacer = true;
  auto quicWebTransport =
      std::make_shared<proxygen::QuicWebTransport>(std::move(quicSocket));
  auto qWtPtr = quicWebTransport.get();
  std::shared_ptr<proxygen::WebTransport> wt(std::move(quicWebTransport));
  folly::EventBase* evb{nullptr};
  if (qevb) {
    evb = qevb->getTypedEventBase<quic::FollyQuicEventBase>()
              ->getBackingEventBase();
  }
  auto moqSession = std::make_shared<MoQSession>(wt, *this, evb);
  qWtPtr->setHandler(moqSession.get());
  // the handleClientSession coro this session moqSession
  co_withExecutor(evb, handleClientSession(std::move(moqSession))).start();
}

folly::Try<ServerSetup> MoQServer::onClientSetup(ClientSetup setup) {


  XLOG(INFO) << "ClientSetup";
  uint64_t negotiatedVersion = 0;
  // Iterate over supported versions and set the highest version within the
  // range
  constexpr uint64_t kVersionMin = kVersionDraft08;
  constexpr uint64_t kVersionMax = kVersionDraft11;
  uint64_t highestVersion = 0;
  for (const auto& version : setup.supportedVersions) {
    if (version >= kVersionMin && version <= kVersionMax) {
      highestVersion = std::max(highestVersion, version);
    }
  }
  if (highestVersion == 0) {
    return folly::Try<ServerSetup>(std::runtime_error(
        "Client does not support versions in the range " +
        std::to_string(getDraftMajorVersion(kVersionMin)) + " to " +
        std::to_string(getDraftMajorVersion(kVersionMax))));
  }
  negotiatedVersion = highestVersion;

  // TODO: Make the default MAX_REQUEST_ID configurable and
  // take in the value from ClientSetup
  static constexpr size_t kDefaultMaxRequestID = 100;
  static constexpr size_t kMaxAuthTokenCacheSize = 1024;
  ServerSetup serverSetup = ServerSetup({
      negotiatedVersion,
      {{folly::to_underlying(SetupKey::MAX_REQUEST_ID),
        "",
        kDefaultMaxRequestID,
        {}},
       {folly::to_underlying(SetupKey::MAX_AUTH_TOKEN_CACHE_SIZE),
        "",
        kMaxAuthTokenCacheSize,
        {}}},
  });

  // Log Server Setup
  if (logger_) {
    logger_->logServerSetup(serverSetup);
  }

  return folly::Try<ServerSetup>(serverSetup);
}

folly::coro::Task<void> MoQServer::handleClientSession(std::shared_ptr<MoQSession> clientSession) {


  XLOG(DBG1) << __func__;


  onNewSession(clientSession);
  clientSession->start();


  // The clientSession will cancel this token when the app calls close() or
  // the underlying transport invokes onSessionEnd
  folly::coro::Baton baton;
  folly::CancellationCallback cb(
      clientSession->getCancelToken(), [&baton] { baton.post(); });
  co_await baton;
  terminateClientSession(std::move(clientSession));
}

void MoQServer::Handler::onHeadersComplete(std::unique_ptr<HTTPMessage> req) noexcept {

  std::cout << "MoQServer::Handler::onHeadersComplete" << std::endl ;

  XLOG(DBG1) << __func__;

  HTTPMessage resp;
  resp.setHTTPVersion(1, 1);

  if (req->getPathAsStringPiece() != server_.getEndpoint()) {
  std::cout << "1 req->getPathAsStringPiece() != server_.getEndpoint()" << std::endl ;

    XLOG(INFO) << req->getPathAsStringPiece();
    req->dumpMessage(0);
    resp.setStatusCode(404);
    txn_->sendHeadersWithEOM(resp);
    return;
  }
  if (req->getMethod() != HTTPMethod::CONNECT || !req->getUpgradeProtocol() ||
      *req->getUpgradeProtocol() != std::string("webtransport")) {
  std::cout << "2 resp.setStatusCode(400)" << std::endl ;

    resp.setStatusCode(400);
    txn_->sendHeadersWithEOM(resp);
    return;
  }
  resp.setStatusCode(200);
  std::cout << "3 add header" << std::endl ;

  resp.getHeaders().add("sec-webtransport-http3-draft", "draft02");
  txn_->sendHeaders(resp);
  std::cout << "4 txn_->sendHeaders" << std::endl ;

  auto wt = txn_->getWebTransport();
  if (!wt) {
    std::cout << "5 !wt" << std::endl ;

    XLOG(ERR) << "Failed to get WebTransport";
    txn_->sendAbort();
    return;
  }

  std::cout << "5 before folly::EventBaseManager::get()->getEventBase()" << std::endl ;

  auto evb = folly::EventBaseManager::get()->getEventBase();
  clientSession_ = std::make_shared<MoQSession>(wt, server_, evb);

  std::cout << "before co_withExecutor( handleClientSession)" << std::endl ;

  co_withExecutor(evb, server_.handleClientSession(clientSession_)).start();
}

void MoQServer::setLogger(std::shared_ptr<MLogger> logger) {

  XLOG(DBG1) << __func__;

  logger_ = std::move(logger);
}

} // namespace moxygen
