// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include "WebRTCServerImpl.h"

#include <chrono>
#include <memory>
#include <rtc/common.hpp>
#include <rtc/datachannel.hpp>
#include <rtc/peerconnection.hpp>
#include <rtc/websocketserver.hpp>
#include <string_view>
#include <thread>
#include <wpi/json.h>

#include <wpi/SmallString.h>
#include <wpi/StringExtras.h>
#include <wpi/fmt/raw_ostream.h>
#include <wpi/print.h>
#include <wpinet/HttpUtil.h>
#include <wpinet/TCPAcceptor.h>
#include <wpinet/raw_socket_istream.h>
#include <wpinet/raw_socket_ostream.h>

#include "Handle.h"
#include "Instance.h"
#include "JpegUtil.h"
#include "Log.h"
#include "Notifier.h"
#include "SourceImpl.h"
#include "c_util.h"
#include "cscore_cpp.h"
#include "wpi/SmallVector.h"

using namespace cs;

class WebRTCServerImpl::ConnThread : public wpi::SafeThread {
 public:
  explicit ConnThread(std::string_view name, wpi::Logger& logger)
      : m_name(name), m_logger(logger) {}

  void Main() override;

  std::shared_ptr<SourceImpl> m_source;
  bool m_streaming = false;
  bool m_noStreaming = false;
  int m_width = 0;
  int m_height = 0;
  int m_compression = -1;
  int m_defaultCompression = 80;
  int m_fps = 0;
  std::shared_ptr<rtc::PeerConnection> m_connection;
  std::shared_ptr<rtc::DataChannel> m_channel;
  std::shared_ptr<rtc::WebSocket> m_socket;

 private:
  std::string m_name;
  wpi::Logger& m_logger;

  std::string_view GetName() { return m_name; }

  std::shared_ptr<SourceImpl> GetSource() {
    std::scoped_lock lock(m_mutex);
    return m_source;
  }

  void StartStream() {
    std::scoped_lock lock(m_mutex);
    if (m_source) {
      m_source->EnableSink();
    }
    m_streaming = true;
  }

  void StopStream() {
    std::scoped_lock lock(m_mutex);
    if (m_source) {
      m_source->DisableSink();
    }
    m_streaming = false;
  }
};

WebRTCServerImpl::WebRTCServerImpl(std::string_view name, wpi::Logger& logger,
                                   Notifier& notifier, Telemetry& telemetry,
                                   std::string_view listenAddress, int port)
    : SinkImpl{name, logger, notifier, telemetry},
      m_listenAddress(listenAddress),
      m_port(port) {
  m_active = true;

  rtc::WebSocketServerConfiguration wsConfig;
  wsConfig.port = 1180;  // FIXME: Casting issue
  m_signalingServer = std::make_shared<rtc::WebSocketServer>(wsConfig);
  m_signalingServer->onClient([this](std::shared_ptr<rtc::WebSocket> socket) {
    socket->onOpen([socket, this] {
      auto connection = std::make_shared<rtc::PeerConnection>();
      std::shared_ptr<rtc::DataChannel> channel =
          connection->createDataChannel("test");

      connection->setLocalDescription(rtc::Description::Type::Offer);
      wpi::json offer{{"type", connection->localDescription()->typeString()},
                      {"sdp", connection->localDescription().value()}};

      socket->onMessage([socket, connection](rtc::message_variant data) {
        auto sdpAnswer = std::get<std::string>(data);
        if (wpi::contains(sdpAnswer, "sdp")) {
          auto answer = wpi::json::parse(sdpAnswer);
          rtc::Description desc(answer["sdp"].template get<std::string>(),
                                answer["type"].template get<std::string>());
          connection->setRemoteDescription(desc);
        }
      });
      socket->send(offer.dump());

      auto source = GetSource();

      std::scoped_lock lock(m_mutex);
      // Find unoccupied worker thread, or create one if necessary
      auto it = std::find_if(
          m_connThreads.begin(), m_connThreads.end(),
          [](const wpi::SafeThreadOwner<ConnThread>& owner) {
            auto thr = owner.GetThread();
            return !thr &&
                   (!thr->m_channel ||
                    !thr->m_channel
                         ->isOpen());  // TODO: Check more WebRTC objects?
          });
      if (it == m_connThreads.end()) {
        m_connThreads.emplace_back();
        it = std::prev(m_connThreads.end());
      }

      // Start it if not already started
      it->Start(GetName(), m_logger);

      auto nstreams =
          std::count_if(m_connThreads.begin(), m_connThreads.end(),
                        [](const wpi::SafeThreadOwner<ConnThread>& owner) {
                          auto thr = owner.GetThread();
                          return thr && thr->m_streaming;
                        });

      // Hand off connection to it
      auto thr = it->GetThread();
      thr->m_source = source;
      thr->m_noStreaming = nstreams >= 10;
      thr->m_width = GetProperty(m_widthProp)->value;
      thr->m_height = GetProperty(m_heightProp)->value;
      thr->m_compression = GetProperty(m_compressionProp)->value;
      thr->m_defaultCompression = GetProperty(m_defaultCompressionProp)->value;
      thr->m_fps = GetProperty(m_fpsProp)->value;
      thr->m_connection = connection;
      thr->m_channel = channel;
      thr->m_socket = socket;
      thr->m_cond.notify_one();
    });
  });

  SetDescription(fmt::format("HTTP Server on port {}", port));

  // Create properties
  m_widthProp = CreateProperty("width", [] {
    return std::make_unique<PropertyImpl>("width", CS_PROP_INTEGER, 1, 0, 0);
  });
  m_heightProp = CreateProperty("height", [] {
    return std::make_unique<PropertyImpl>("height", CS_PROP_INTEGER, 1, 0, 0);
  });
  m_compressionProp = CreateProperty("compression", [] {
    return std::make_unique<PropertyImpl>("compression", CS_PROP_INTEGER, -1,
                                          100, 1, -1, -1);
  });
  m_defaultCompressionProp = CreateProperty("default_compression", [] {
    return std::make_unique<PropertyImpl>("default_compression",
                                          CS_PROP_INTEGER, 0, 100, 1, 80, 80);
  });
  m_fpsProp = CreateProperty("fps", [] {
    return std::make_unique<PropertyImpl>("fps", CS_PROP_INTEGER, 1, 0, 0);
  });
}

WebRTCServerImpl::~WebRTCServerImpl() {
  Stop();
}

void WebRTCServerImpl::Stop() {
  m_active = false;

  // close streams
  for (auto& connThread : m_connThreads) {
    if (auto thr = connThread.GetThread()) {
      if (thr->m_channel) {  // TODO: Check all WebRTC objects
        thr->m_channel->close();
      }
    }
    connThread.Stop();
  }

  // wake up connection threads by forcing an empty frame to be sent
  if (auto source = GetSource()) {
    source->Wakeup();
  }
}

// Main server thread
void WebRTCServerImpl::ConnThread::Main() {
  Frame::Time lastFrameTime = 0;
  Frame::Time timePerFrame = 0;
  if (m_fps != 0) {
    timePerFrame = 1000000.0 / m_fps;
  }
  Frame::Time averageFrameTime = 0;
  Frame::Time averagePeriod = 1000000;  // 1 second window
  if (averagePeriod < timePerFrame) {
    averagePeriod = timePerFrame * 10;
  }

  StartStream();
  while (m_active && m_channel->isOpen() || m_socket->isOpen()) {
    auto source = GetSource();
    if (!source) {
      // Source disconnected; sleep so we don't consume all processor time.
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      continue;
    }
    SDEBUG4("waiting for frame");
    Frame frame = source->GetNextFrame(0.225);  // blocks
    if (!m_active || !m_channel->isOpen() || !m_socket->isOpen()) {
      break;
    }
    if (!frame) {
      // Bad frame; sleep for 20 ms so we don't consume all processor time.
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      continue;
    }

    auto thisFrameTime = frame.GetTime();
    if (thisFrameTime != 0 && timePerFrame != 0 && lastFrameTime != 0) {
      Frame::Time deltaTime = thisFrameTime - lastFrameTime;

      // drop frame if it is early compared to the desired frame rate AND
      // the current average is higher than the desired average
      if (deltaTime < timePerFrame && averageFrameTime < timePerFrame) {
        // sleep for 1 ms so we don't consume all processor time
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }

      // update average
      if (averageFrameTime != 0) {
        averageFrameTime =
            averageFrameTime * (averagePeriod - timePerFrame) / averagePeriod +
            deltaTime * timePerFrame / averagePeriod;
      } else {
        averageFrameTime = deltaTime;
      }
    }

    int width = m_width != 0 ? m_width : frame.GetOriginalWidth();
    int height = m_height != 0 ? m_height : frame.GetOriginalHeight();
    Image* image = frame.GetImageMJPEG(
        width, height, m_compression,
        m_compression == -1 ? m_defaultCompression : m_compression);
    if (!image) {
      // Shouldn't happen, but just in case...
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      continue;
    }

    const char* data = image->data();
    size_t size = image->size();
    bool addDHT = false;
    size_t locSOF = size;
    wpi::SmallVector<char, 16000> buf;  // TODO: fix
    switch (image->pixelFormat) {
      case VideoMode::kMJPEG:
        // Determine if we need to add DHT to it, and allocate enough space
        // for adding it if required.
        addDHT = JpegNeedsDHT(data, &size, &locSOF);
        if (addDHT) {
          // Insert DHT data immediately before SOF
          std::string_view before(data, locSOF);
          buf.append(before.begin(), before.end());
          buf.append(JpegGetDHT().begin(), JpegGetDHT().end());
          std::string_view sof(data + locSOF, image->size() - locSOF);
          buf.append(sof.begin(), sof.end());
          m_channel->sendBuffer(buf);
        } else {
          std::string_view imageData(data, size);
          m_channel->sendBuffer(imageData);
        }
        break;
      case VideoMode::kUYVY:
      case VideoMode::kRGB565:
      case VideoMode::kYUYV:
      case VideoMode::kY16:
      default:
        // Bad frame; sleep for 10 ms so we don't consume all processor time.
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        continue;
    }

    SDEBUG4("sending frame size={} addDHT={}", size, addDHT);

    // print the individual mimetype and the length
    // sending the content-length fixes random stream disruption observed
    // with firefox
    lastFrameTime = thisFrameTime;
    double timestamp = lastFrameTime / 1000000.0;
  }
  StopStream();
}

void WebRTCServerImpl::SetSourceImpl(std::shared_ptr<SourceImpl> source) {}

namespace cs {
CS_Sink CreateWebRTCServer(std::string_view name,
                           std::string_view listenAddress, int port,
                           CS_Status* status) {
  auto& inst = Instance::GetInstance();
  return inst.CreateSink(
      CS_SINK_MJPEG,
      std::make_shared<WebRTCServerImpl>(name, inst.logger, inst.notifier,
                                         inst.telemetry, listenAddress, port));
}

std::string GetWebRTCServerListenAddress(CS_Sink sink, CS_Status* status) {
  auto data = Instance::GetInstance().GetSink(sink);
  if (!data || data->kind != CS_SINK_MJPEG) {
    *status = CS_INVALID_HANDLE;
    return std::string{};
  }
  return static_cast<WebRTCServerImpl&>(*data->sink).GetListenAddress();
}

int GetWebRTCServerPort(CS_Sink sink, CS_Status* status) {
  auto data = Instance::GetInstance().GetSink(sink);
  if (!data || data->kind != CS_SINK_MJPEG) {
    *status = CS_INVALID_HANDLE;
    return 0;
  }
  return static_cast<WebRTCServerImpl&>(*data->sink).GetPort();
}

}  // namespace cs

extern "C" {
CS_Sink CS_CreateWebRTCServer(const struct WPI_String* name,
                              const struct WPI_String* listenAddress, int port,
                              CS_Status* status) {
  return cs::CreateWebRTCServer(wpi::to_string_view(name),
                                wpi::to_string_view(listenAddress), port,
                                status);
}

void CS_GetWebRTCServerListenAddress(CS_Sink sink, WPI_String* listenAddress,
                                     CS_Status* status) {
  cs::ConvertToC(listenAddress, cs::GetWebRTCServerListenAddress(sink, status));
}

int CS_GetWebRTCServerPort(CS_Sink sink, CS_Status* status) {
  return cs::GetWebRTCServerPort(sink, status);
}

}  // extern "C"
