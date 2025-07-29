/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <folly/coro/Sleep.h>
#include <moxygen/MoQLocation.h>
#include <moxygen/MoQServer.h>
#include <moxygen/MoQWebTransportClient.h>
#include <moxygen/relay/MoQForwarder.h>
#include <moxygen/relay/MoQRelayClient.h>
#include <iomanip>

using namespace quic::samples;
using namespace proxygen;

DEFINE_string(relay_url, "", "Use specified relay");
DEFINE_int32(relay_connect_timeout, 1000, "Connect timeout (ms)");
DEFINE_int32(relay_transaction_timeout, 120, "Transaction timeout (s)");
DEFINE_string(cert, "", "Cert path");
DEFINE_string(key, "", "Key path");
DEFINE_int32(port, 9667, "Server Port");
DEFINE_string(
    mode,
    "spg",
    "Transmission mode for track: stream-per-group (spg), "
    "stream-per-object(spo), datagram");
DEFINE_bool(quic_transport, false, "Use raw QUIC transport");
DEFINE_bool(v11Plus, true, "Negotiate versions 11 or higher");

namespace {
using namespace moxygen;

uint8_t extTestBuff[5] = {0x01, 0x02, 0x03, 0x04, 0x05};
static const Extensions kExtensions{
    {0xacedecade, 1977},
    {0xdeadbeef, folly::IOBuf::copyBuffer(extTestBuff, sizeof(extTestBuff))}};

class MoQDateServer : public MoQServer,
                      public Publisher,
                      public std::enable_shared_from_this<MoQDateServer> {
 public:
  enum class Mode { STREAM_PER_GROUP, STREAM_PER_OBJECT, DATAGRAM };

  explicit MoQDateServer(Mode mode)
      : MoQServer(FLAGS_port, FLAGS_cert, FLAGS_key, "/moq-date"),
        forwarder_(dateTrackName()),
        mode_(mode) {}

  // 원격 MoQ 릴레이 서버에 클라이언트로 연결을 시작하는 메소드입니다.
  // 성공적으로 시작되면 true, 실패하면 false를 반환합니다.
  bool startRelayClient() {

    std::cout << "startRelayClient()" << std::endl;

    // 1. 커맨드라인 인자로 받은 릴레이 서버의 URL (FLAGS_relay_url)을 Proxygen 라이브러리의 URL 객체로 파싱합니다.
    //    이 객체는 URL의 유효성을 검사하고, 호스트, 포트 등 구성 요소를 쉽게 추출할 수 있게 해줍니다.
    proxygen::URL url(FLAGS_relay_url);

    // 2. 파싱된 URL이 유효한 형식이 아니거나, 연결할 호스트(서버 주소) 정보가 없는지 확인합니다.
    //    문제가 있다면 에러 로그를 남기고 함수를 즉시 종료하며 false를 반환합니다.
    if (!url.isValid() || !url.hasHost()) {
      XLOG(ERR) << "Invalid url: " << FLAGS_relay_url;
      return false;
    }

    std::cout << "[FLAGS_quic_transport] " << FLAGS_quic_transport << std::endl;

    // 3. 서버가 운영하는 워커(worker) 스레드들의 이벤트 루프(EventBase) 목록을 가져와서 첫 번째 것을 선택합니다.
    //    비동기 작업을 처리할 스레드를 지정하기 위함입니다. (자세한 내용은 아래 '보충 설명' 참조)
    auto evb = getWorkerEvbs()[0];

    // 4. relayClient_ 멤버 변수(unique_ptr)에 MoQRelayClient 객체를 생성하여 할당합니다.
    //    std::make_unique를 사용하여 메모리를 안전하게 할당하고 관리합니다. (자세한 내용은 아래 '보충 설명' 참조)
    relayClient_ = std::make_unique<MoQRelayClient>(
        // FLAGS_quic_transport 플래그 값에 따라 사용할 전송 클라이언트를 동적으로 결정합니다.
        (FLAGS_quic_transport
             // true일 경우: Raw QUIC 전송을 위한 MoQClient 객체를 생성합니다.
             ? std::make_unique<MoQClient>(evb, url)
             // false일 경우: 웹 표준인 WebTransport를 위한 MoQWebTransportClient 객체를 생성합니다.
             : std::make_unique<MoQWebTransportClient>(evb, url)));

    // 5. C++20 코루틴을 특정 실행기(Executor)에서 실행하도록 스케줄링합니다. (자세한 내용은 아래 '보충 설명' 참조)
    // folly 라이브러리의 함수 co_withExecutor
    
    // 실제로 실행할 비동기 코루틴 함수인 relayClient_->run()을 호출합니다.
    // relayClient_->run(
    //     /*publisher=*/shared_from_this(), // 이 MoQDateServer 인스턴스가 발행자(publisher) 역할을 함을 알립니다.
    //     /*subscriber=*/nullptr, // 이 클라이언트는 구독(subscribe)은 하지 않으므로 nullptr을 전달합니다.
    //     {TrackNamespace({"moq-date"})}, // 이 클라이언트가 발행할 트랙의 네임스페이스를 정의합니다.
    //     std::chrono::milliseconds(FLAGS_relay_connect_timeout), // 릴레이 연결 시도 시의 타임아웃을 설정합니다.
    //     std::chrono::seconds(FLAGS_relay_transaction_timeout), // 연결 후 트랜잭션 처리 시의 타임아웃을 설정합니다.
    //     FLAGS_v11Plus)
      
    // error: use of deleted function ‘folly::coro::Task<T>::Task(const folly::coro::Task<T>&) [with T = void]’
    // folly::coro::Task<void> FollyCoroTask = relayClient_->run(
    //         /*publisher=*/shared_from_this(), // 이 MoQDateServer 인스턴스가 발행자(publisher) 역할을 함을 알립니다.
    //         /*subscriber=*/nullptr, // 이 클라이언트는 구독(subscribe)은 하지 않으므로 nullptr을 전달합니다.
    //         {TrackNamespace({"moq-date"})}, // 이 클라이언트가 발행할 트랙의 네임스페이스를 정의합니다.
    //         std::chrono::milliseconds(FLAGS_relay_connect_timeout), // 릴레이 연결 시도 시의 타임아웃을 설정합니다.
    //         std::chrono::seconds(FLAGS_relay_transaction_timeout), // 연결 후 트랜잭션 처리 시의 타임아웃을 설정합니다.
    //         FLAGS_v11Plus);

    std::cout << "before co_withExecutor" << std::endl;

            // 이 코루틴을 3번 단계에서 가져온 이벤트 베이스(evb)의 스레드에서 실행하도록 지정합니다.
            // 이를 통해 네트워킹 관련 작업이 특정 스레드에서 안전하게 처리되도록 보장합니다.
    co_withExecutor(evb,
    relayClient_->run(
            /*publisher=*/shared_from_this(), // 이 MoQDateServer 인스턴스가 발행자(publisher) 역할을 함을 알립니다.
            /*subscriber=*/nullptr, // 이 클라이언트는 구독(subscribe)은 하지 않으므로 nullptr을 전달합니다.
        /*std::vector<TrackNamespace>*/  {TrackNamespace({"moq-date"})}, // 이 클라이언트가 발행할 트랙의 네임스페이스를 정의합니다.
/* connectTimeout */  std::chrono::milliseconds(FLAGS_relay_connect_timeout), // 릴레이 연결 시도 시의 타임아웃을 설정합니다.
            std::chrono::seconds(FLAGS_relay_transaction_timeout), // 연결 후 트랜잭션 처리 시의 타임아웃을 설정합니다.
            FLAGS_v11Plus)
    ).start(); 

    std::cout << "after co_withExecutor" << std::endl;

    
          // MoQ 프로토콜 버전 11 이상을 협상할지 여부를 전달합니다.
        // .start()를 호출하여 코루틴을 "실행하고 잊어버리는(fire-and-forget)" 방식으로 시작합니다.
        // 즉, 이 코루틴이 완료될 때까지 기다리지 않고, 즉시 다음 코드로 넘어갑니다.
        
        
        // taskWithExecutorT.start();

    // 6. 코루틴의 시작이 성공적으로 스케줄링되었으므로 true를 반환합니다.
    //    (실제 연결 성공 여부는 이 함수가 아닌, 비동기적으로 실행되는 run 코루틴 내부에서 처리됩니다.)
    return true;
  }

  void onNewSession(std::shared_ptr<MoQSession> clientSession) override {
    clientSession->setPublishHandler(shared_from_this());
  }

  std::pair<uint64_t, uint64_t> now() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    // +1 is because object two objects are published at second=0
    return {uint64_t(in_time_t / 60), uint64_t(in_time_t % 60)};
  }

  AbsoluteLocation nowLocation() {
    auto [minute, second] = now();
    // +1 is because object two objects are published at second=0
    return {minute, second + 1};
  }

  AbsoluteLocation updateLargest() {
    if (!loopRunning_) {
      forwarder_.setLargest(nowLocation());
    }
    return *forwarder_.largest();
  }

  folly::coro::Task<TrackStatusResult> trackStatus(
      TrackStatusRequest trackStatusRequest) override {
    XLOG(DBG1) << __func__ << trackStatusRequest.fullTrackName;
    if (trackStatusRequest.fullTrackName != dateTrackName()) {
      co_return TrackStatus{
          trackStatusRequest.requestID,
          std::move(trackStatusRequest.fullTrackName),
          TrackStatusCode::TRACK_NOT_EXIST,
          folly::none};
    }
    // TODO: add other trackSTatus codes
    // TODO: unify this with subscribe. You can get the same information both
    // ways
    auto largest = updateLargest();
    co_return TrackStatus{
        trackStatusRequest.requestID,
        std::move(trackStatusRequest.fullTrackName),
        TrackStatusCode::IN_PROGRESS,
        largest};
  }

  folly::coro::Task<SubscribeResult> subscribe(
      SubscribeRequest subReq,
      std::shared_ptr<TrackConsumer> consumer) override {

    XLOG(INFO) << "[MoQDateServer.cpp subscribe method] SubscribeRequest track ns="
               << subReq.fullTrackName.trackNamespace
               << " name=" << subReq.fullTrackName.trackName
               << " requestID=" << subReq.requestID;

    if (subReq.fullTrackName != dateTrackName()) {
      co_return folly::makeUnexpected(SubscribeError{
          subReq.requestID,
          SubscribeErrorCode::TRACK_NOT_EXIST,
          "unexpected subscribe"});
    }

    auto largest = updateLargest();

    if (subReq.locType == LocationType::AbsoluteRange &&
        subReq.endGroup < largest.group) {
      co_return folly::makeUnexpected(SubscribeError{
          subReq.requestID,
          SubscribeErrorCode::INVALID_RANGE,
          "Range in the past, use FETCH"});
      // start may be in the past, it will get adjusted forward to largest
    }

    auto alias = subReq.trackAlias.value_or(TrackAlias(subReq.requestID.value));

    consumer->setTrackAlias(alias);

    auto session = MoQSession::getRequestSession();

    if (!loopRunning_) {
      XLOG(INFO) << "[subscribe function] !loopRunning_";
      loopRunning_ = true;
      XLOG(INFO) << "[subscribe function] before publishDateLoop()";
      co_withExecutor(session->getEventBase(), publishDateLoop()).start();
    }

      XLOG(INFO) << "before forwarder_.addSubscriber";

    co_return forwarder_.addSubscriber(
        std::move(session), subReq, std::move(consumer));
  }

  class FetchHandle : public Publisher::FetchHandle {
   public:
    explicit FetchHandle(FetchOk ok) : Publisher::FetchHandle(std::move(ok)) {}
    void fetchCancel() override {
      cancelSource.requestCancellation();
    }
    folly::CancellationSource cancelSource;
  };

  folly::coro::Task<FetchResult> fetch(
      Fetch fetch,
      std::shared_ptr<FetchConsumer> consumer) override {
    auto clientSession = MoQSession::getRequestSession();
    XLOG(INFO) << "Fetch track ns=" << fetch.fullTrackName.trackNamespace
               << " name=" << fetch.fullTrackName.trackName
               << " requestID=" << fetch.requestID;
    if (fetch.fullTrackName != dateTrackName()) {
      co_return folly::makeUnexpected(FetchError{
          fetch.requestID,
          FetchErrorCode::TRACK_NOT_EXIST,
          "unexpected fetch"});
    }
    auto largest = updateLargest();
    auto [standalone, joining] = fetchType(fetch);
    StandaloneFetch sf;
    if (joining) {
      auto res = forwarder_.resolveJoiningFetch(clientSession, *joining);
      if (res.hasError()) {
        XLOG(ERR) << "Bad joining fetch id=" << fetch.requestID
                  << " err=" << res.error().reasonPhrase;
        co_return folly::makeUnexpected(res.error());
      }
      sf = StandaloneFetch(res.value().start, res.value().end);
      standalone = &sf;
    } else if (standalone->end > largest) {
      standalone->end = largest;
      standalone->end.object++; // exclusive range, include largest
    }
    if (standalone->end < standalone->start &&
        !(standalone->start.group == standalone->end.group &&
          standalone->end.object == 0)) {
      co_return folly::makeUnexpected(FetchError{
          fetch.requestID, FetchErrorCode::INVALID_RANGE, "No objects"});
    }
    if (standalone->start > largest) {
      co_return folly::makeUnexpected(FetchError{
          fetch.requestID,
          FetchErrorCode::INVALID_RANGE,
          "fetch starts in future"});
    }
    XLOG(DBG1) << "Fetch {" << standalone->start.group << ","
               << standalone->start.object << "}.." << standalone->end.group
               << "," << standalone->end.object << "}";

    auto fetchHandle = std::make_shared<FetchHandle>(FetchOk{
        fetch.requestID,
        MoQSession::resolveGroupOrder(
            GroupOrder::OldestFirst, fetch.groupOrder),
        0, // not end of track
        largest,
        {}});
    co_withExecutor(
        clientSession->getEventBase(),
        folly::coro::co_withCancellation(
            fetchHandle->cancelSource.getToken(),
            catchup(std::move(consumer), {standalone->start, standalone->end})))
        .start();
    co_return fetchHandle;
  }

  void goaway(Goaway goaway) override {
    XLOG(INFO) << "Processing goaway uri=" << goaway.newSessionUri;
    auto session = MoQSession::getRequestSession();
    if (relayClient_ && relayClient_->getSession() == session) {
      // TODO: relay is going away
    } else {
      forwarder_.removeSession(session);
    }
  }

  Payload minutePayload(uint64_t group) {
    time_t in_time_t = group * 60;
    struct tm local_tm;
    auto lt = ::localtime_r(&in_time_t, &local_tm);
    std::stringstream ss;
    ss << std::put_time(lt, "%Y-%m-%d %H:%M:");
    XLOG(DBG1) << ss.str() << lt->tm_sec;
    return folly::IOBuf::copyBuffer(ss.str());
  }

  Payload secondPayload(uint64_t object) {
    XCHECK_GT(object, 0llu);
    auto secBuf = folly::to<std::string>(object - 1);
    XLOG(DBG1) << (object - 1);
    return folly::IOBuf::copyBuffer(secBuf);
  }

  folly::coro::Task<void> catchup(
      std::shared_ptr<FetchConsumer> fetchPub,
      SubscribeRange range) {
    if (range.start.object > 61) {
      co_return;
    }
    XLOG(ERR) << "Range: start=" << range.start.group << "."
              << range.start.object << " end=" << range.end.group << "."
              << range.end.object;
    auto token = co_await folly::coro::co_current_cancellation_token;
    while (!token.isCancellationRequested() && range.start < range.end) {
      uint64_t subgroup =
          mode_ == Mode::STREAM_PER_OBJECT ? range.start.object : 0;
      folly::Expected<folly::Unit, MoQPublishError> res{folly::unit};
      if (range.start.object == 0) {
        res = fetchPub->object(
            range.start.group,
            subgroup,
            range.start.object,
            minutePayload(range.start.group),
            kExtensions);
      } else if (range.start.object <= 60) {
        res = fetchPub->object(
            range.start.group,
            subgroup,
            range.start.object,
            secondPayload(range.start.object));
      } else {
        res = fetchPub->endOfGroup(
            range.start.group, subgroup, range.start.object);
      }
      if (!res) {
        XLOG(ERR) << "catchup error: " << res.error().what();
        if (res.error().code == MoQPublishError::BLOCKED) {
          XLOG(DBG1) << "Fetch blocked, waiting";
          auto awaitRes = fetchPub->awaitReadyToConsume();
          if (!awaitRes) {
            XLOG(ERR) << "awaitReadyToConsume error: "
                      << awaitRes.error().what();
            fetchPub->reset(ResetStreamErrorCode::INTERNAL_ERROR);
            co_return;
          }
          co_await std::move(awaitRes.value());
        } else {
          fetchPub->reset(ResetStreamErrorCode::INTERNAL_ERROR);
          co_return;
        }
      }
      range.start.object++;
      if (range.start.object > 61) {
        range.start.group++;
        range.start.object = 0;
      }
    }
    if (token.isCancellationRequested()) {
      fetchPub->reset(ResetStreamErrorCode::CANCELLED);
    } else {
      // TODO - empty range may log an error?
      XLOG(ERR) << "endOfFetch";
      XLOG(ERR) << "Range: start=" << range.start.group << "."
                << range.start.object << " end=" << range.end.group << "."
                << range.end.object;
      fetchPub->endOfFetch();
    }
  }

  folly::coro::Task<void> publishDateLoop() {

    std::cout << "[in publishDateLoop]" << std::endl;

    auto cancelToken = co_await folly::coro::co_current_cancellation_token;
    std::shared_ptr<SubgroupConsumer> subgroupPublisher;
    while (!cancelToken.isCancellationRequested()) {
  std::cout << "[publishDateLoop()] [in while (!cancelToken.isCancellationRequested())]" << std::endl;

      if (forwarder_.empty()) {
      std::cout << "in forwarder_.empty()" << std::endl;

        forwarder_.setLargest(nowLocation());
      } else {
  std::cout << "create current second" << std::endl;

        auto [minute, second] = now();
        switch (mode_) {
          case Mode::STREAM_PER_GROUP:
  std::cout << "Mode::STREAM_PER_GROUP" << std::endl;
            subgroupPublisher = publishDate(subgroupPublisher, minute, second);
            break;
          case Mode::STREAM_PER_OBJECT:
  std::cout << "Mode::STREAM_PER_OBJECT" << std::endl;
            publishDate(minute, second);
            break;
          case Mode::DATAGRAM:
  std::cout << "Mode::DATAGRAM" << std::endl;
            publishDategram(minute, second);
            break;
        }
      }
      co_await folly::coro::sleep(std::chrono::seconds(1));
    }
  }
// ========================================
  std::shared_ptr<SubgroupConsumer> publishDate(
      std::shared_ptr<SubgroupConsumer> subgroupPublisher,
      uint64_t group,
      uint64_t second) {
    uint64_t subgroup = 0;
    uint64_t object = second;

  std::cout << "[in publishDate] " << "subgroupPublisher : " << subgroupPublisher << std::endl;
  std::cout << "[in publishDate] " << "object : " << object << std::endl;

    if (!subgroupPublisher) {
    std::cout << "[in publishDate] " << " forwarder_.beginSubgroup" << std::endl;

      subgroupPublisher =
          forwarder_.beginSubgroup(group, subgroup, /*priority=*/0).value();
    }

    if (object == 0) {
  std::cout << "[in publishDate] " << "object == 0" << std::endl;

      subgroupPublisher->object(0, minutePayload(group), kExtensions, false);
    }

    object++;
  std::cout << "[in publishDate] " << "before subgroupPublisher->object" << std::endl;

    subgroupPublisher->object(
        object, secondPayload(object), noExtensions(), false);

  std::cout << "[in publishDate] " << "after subgroupPublisher->object" << std::endl;
    
    if (object >= 60) {
      object++;
  std::cout << "[in publishDate] " << "before subgroupPublisher->endOfGroup" << std::endl;

      subgroupPublisher->endOfGroup(object);
  std::cout << "[in publishDate] " << "before subgroupPublisher.reset()" << std::endl;

      subgroupPublisher.reset();
    }
    return subgroupPublisher;
  }
// ========================================

  void publishDate(uint64_t group, uint64_t second) {
    uint64_t subgroup = second;
    uint64_t object = second;
    ObjectHeader header{
        TrackAlias(0),
        group,
        subgroup,
        object,
        /*priorityIn=*/0,
        ObjectStatus::NORMAL,
        noExtensions(),
        folly::none};
    if (second == 0) {
      forwarder_.objectStream(header, minutePayload(group));
    }
    header.subgroup++;
    header.id++;
    forwarder_.objectStream(header, secondPayload(header.id));
    if (header.id >= 60) {
      header.subgroup++;
      header.id++;
      header.status = ObjectStatus::END_OF_GROUP;
      forwarder_.objectStream(header, nullptr);
    }
  }
// ========================================

  void publishDategram(uint64_t group, uint64_t second) {
    uint64_t object = second;
    ObjectHeader header{
        TrackAlias(0),
        group,
        0, // subgroup unused for datagrams
        object,
        /*priorityIn=*/0, // priority
        ObjectStatus::NORMAL,
        kExtensions,
        folly::none};
    if (second == 0) {
      forwarder_.datagram(header, minutePayload(group));
    }
    header.id++;
    header.extensions.clear();
    forwarder_.datagram(header, secondPayload(header.id));
    if (header.id >= 60) {
      header.id++;
      header.status = ObjectStatus::END_OF_GROUP;
      forwarder_.datagram(header, nullptr);
    }
  }
// ========================================

  void terminateClientSession(std::shared_ptr<MoQSession> session) override {
    XLOG(INFO) << __func__;
    forwarder_.removeSession(session);
  }

 private:
  static FullTrackName dateTrackName() {
    return FullTrackName({TrackNamespace({"moq-date"}), "date"});
  }
  MoQForwarder forwarder_;
  std::unique_ptr<MoQRelayClient> relayClient_;
  Mode mode_{Mode::STREAM_PER_GROUP};
  bool loopRunning_{false};
};
} // namespace


int main(int argc, char* argv[]) {

  std::cout << "main()" << std::endl;

  folly::Init init(&argc, &argv, true);
  folly::EventBase evb;
  MoQDateServer::Mode mode;

  std::cout << "FLAGS_mode" << FLAGS_mode << std::endl;

  // spg == STREAM_PER_GROUP
  if (FLAGS_mode == "spg") {
    mode = MoQDateServer::Mode::STREAM_PER_GROUP;
  } else if (FLAGS_mode == "spo") {
    mode = MoQDateServer::Mode::STREAM_PER_OBJECT;
  } else if (FLAGS_mode == "datagram") {
    mode = MoQDateServer::Mode::DATAGRAM;
  } else {
    XLOG(ERR) << "Invalid mode: " << FLAGS_mode;
    return 1;
  
  }


  // 서버 시작: main 함수에서 auto server = std::make_shared<MoQDateServer>(mode); 라인이 실행될 때, MoQDateServer의 생성자가 호출되고,
  // 이어서 부모 클래스인 MoQServer의 생성자가 호출됩니다.
  // MoQServer의 생성자는 내부적으로 HQServer를 생성하고 hqServer_->start()를 호출하여 네트워크 포트를 열고 클라이언트의 연결을 기다리는 상태가 됩니다. 
  // 이 서버는 별도의 스레드에서 동작합니다.
  // MoQDateServer는 릴레이에 연결하지 않는 대신, 직접 클라이언트의 접속을 받습니다.
  // 클라이언트가 이 서버에 접속하여 "moq-date" 트랙을 구독(subscribe)하면, MoQDateServer::subscribe 메소드가 호출됩니다.
  // 이 메소드 안에서 publishDateLoop() 코루틴이 시작되고(205번 라인), 1초마다 현재 시간 정보를 생성하여 접속한 클라이언트에게 전송하기 시작합니다.
  auto server = std::make_shared<MoQDateServer>(mode);
  
  std::cout << "[in main function][FLAGS_relay_url] " << FLAGS_relay_url << std::endl;

  bool isEmpty = FLAGS_relay_url.empty();
  std::cout << "[in main function] isEmpty : " << isEmpty << std::endl;

  if (!isEmpty) {

  std::cout << "[in main function]  before server->startRelayClient()" << std::endl;
    auto res =  server->startRelayClient();
  std::cout << "server->startRelayClient() response" << res << std::endl;
    
    if(!res) {
      return 1;

    }
  } else {
  std::cout << "[in main function]   server->startRelayClient() 를 호출하지 않습니다" << std::endl;

  }

  std::cout << "before loopForever" << std::endl;

  evb.loopForever();
  return 0;
}