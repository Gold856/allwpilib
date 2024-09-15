// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include "WebRTCServerImpl.h"

#include <chrono>
#include <string_view>
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

WebRTCServerImpl::WebRTCServerImpl(
    std::string_view name, wpi::Logger& logger, Notifier& notifier,
    Telemetry& telemetry, std::string_view listenAddress, int port,
    std::unique_ptr<wpi::NetworkAcceptor> acceptor)
    : SinkImpl{name, logger, notifier, telemetry},
      m_listenAddress(listenAddress),
      m_port(port),
      m_acceptor{std::move(acceptor)},
      m_connection(std::make_shared<rtc::PeerConnection>()) {
  m_active = true;

  rtc::DataChannelInit config;
  config.negotiated = true;
  config.id = 100;
  config.reliability = {.unordered = true};
  m_dataChannel = m_connection->createDataChannel("test", config);
  m_connection->setLocalDescription(rtc::Description::Type::Offer);
  wpi::json offer{{"type", m_connection->localDescription()->typeString()},
                  {"sdp", m_connection->localDescription().value()}};
  std::cout << offer;
  wpi::json answer;
  std::cin >> answer;
  rtc::Description desc(answer["sdp"].template get<std::string>(),
                        answer["type"].template get<std::string>());
  m_connection->setRemoteDescription(desc);
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

  m_serverThread = std::thread(&WebRTCServerImpl::ServerThreadMain, this);
}

WebRTCServerImpl::~WebRTCServerImpl() {
  Stop();
}

void WebRTCServerImpl::Stop() {
  m_active = false;

  // wake up server thread by shutting down the socket
  m_acceptor->shutdown();

  // join server thread
  if (m_serverThread.joinable()) {
    m_serverThread.join();
  }

  // close streams

  // wake up connection threads by forcing an empty frame to be sent
  if (auto source = GetSource()) {
    source->Wakeup();
  }
}

// Main server thread
void WebRTCServerImpl::ServerThreadMain() {
  GetSource()->EnableSink();
  while (!m_dataChannel->isClosed()) {
    if (!m_dataChannel->isOpen()) {
      continue;
    }
    Frame::Time lastFrameTime = 0;
    Frame::Time timePerFrame = 0;
    if (GetProperty(m_fpsProp)->value != 0) {
      timePerFrame = 1000000.0 / GetProperty(m_fpsProp)->value;
    }
    Frame::Time averageFrameTime = 0;
    Frame::Time averagePeriod = 1000000;  // 1 second window
    if (averagePeriod < timePerFrame) {
      averagePeriod = timePerFrame * 10;
    }
    while (true) {
      auto source = GetSource();
      if (!source) {
        // Source disconnected; sleep so we don't consume all processor time.
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        continue;
      }
      SDEBUG4("waiting for frame");
      Frame frame = source->GetNextFrame(0.225);  // blocks

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
          averageFrameTime = averageFrameTime * (averagePeriod - timePerFrame) /
                                 averagePeriod +
                             deltaTime * timePerFrame / averagePeriod;
        } else {
          averageFrameTime = deltaTime;
        }
      }

      int width = GetProperty(m_widthProp)->value != 0
                      ? GetProperty(m_widthProp)->value
                      : frame.GetOriginalWidth();
      int height = GetProperty(m_heightProp)->value != 0
                       ? GetProperty(m_heightProp)->value
                       : frame.GetOriginalHeight();
      Image* image = frame.GetImageMJPEG(
          width, height, GetProperty(m_compressionProp)->value,
          GetProperty(m_compressionProp)->value == -1
              ? GetProperty(m_defaultCompressionProp)->value
              : GetProperty(m_compressionProp)->value);
      if (!image) {
        // Shouldn't happen, but just in case...
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        continue;
      }

      const char* data = image->data();
      size_t size = image->size();
      bool addDHT = false;
      size_t locSOF = size;
      wpi::SmallVector<char, 16000> buf;
      switch (image->pixelFormat) {
        case VideoMode::kMJPEG:
          // Determine if we need to add DHT to it, and allocate enough space
          // for adding it if required.
          addDHT = JpegNeedsDHT(data, &size, &locSOF);
          SDEBUG4("sending frame size={} addDHT={}", size, addDHT);
          if (addDHT) {
            // Insert DHT data immediately before SOF
            std::string_view before(data, locSOF);
            buf.append(before.begin(), before.end());
            buf.append(JpegGetDHT().begin(), JpegGetDHT().end());
            std::string_view sof(data + locSOF, image->size() - locSOF);
            buf.append(sof.begin(), sof.end());
            m_dataChannel->sendBuffer(buf);
          } else {
            std::string_view imageData(data, size);
            m_dataChannel->sendBuffer(imageData);
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

      // print the individual mimetype and the length
      // sending the content-length fixes random stream disruption observed
      // with firefox
      lastFrameTime = thisFrameTime;
      double timestamp = lastFrameTime / 1000000.0;
    }
  }
  GetSource()->DisableSink();

  SDEBUG("leaving server thread");
}

void WebRTCServerImpl::SetSourceImpl(std::shared_ptr<SourceImpl> source) {}

namespace cs {

CS_Sink CreateWebRTCServer(std::string_view name,
                           std::string_view listenAddress, int port,
                           CS_Status* status) {
  auto& inst = Instance::GetInstance();
  return inst.CreateSink(
      CS_SINK_MJPEG,
      std::make_shared<WebRTCServerImpl>(
          name, inst.logger, inst.notifier, inst.telemetry, listenAddress, port,
          std::unique_ptr<wpi::NetworkAcceptor>(
              new wpi::TCPAcceptor(port, listenAddress, inst.logger))));
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
