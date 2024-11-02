// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include "WebRTCServerImpl.h"

#include <stdint.h>

#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

#include <fmt/format.h>
#include <rtc/common.hpp>
#include <rtc/configuration.hpp>
#include <rtc/datachannel.hpp>
#include <rtc/peerconnection.hpp>
#include <rtc/websocketserver.hpp>
#include <wpi/SmallString.h>
#include <wpi/SmallVector.h>
#include <wpi/StringExtras.h>
#include <wpi/json.h>
#include <wpi/static_circular_buffer.h>

#include "Instance.h"
#include "JpegUtil.h"
#include "Log.h"
#include "Notifier.h"
#include "SourceImpl.h"
#include "c_util.h"
#include "cscore_c.h"
#include "cscore_cpp.h"

using namespace cs;

class WebRTCServerImpl::ConnThread : public wpi::SafeThread {
 public:
  explicit ConnThread(std::string_view name, wpi::Logger& logger)
      : m_name(name), m_logger(logger) {}

  void Main() override;
  void ProcessRequest(wpi::json message);
  void SendStream();
  void SendJSON();
  void SetChannel(std::shared_ptr<rtc::DataChannel> channel);

  std::shared_ptr<SourceImpl> m_source;
  bool m_streaming = false;
  bool m_noStreaming = false;
  int m_width = 0;
  int m_height = 0;
  int m_compression = -1;
  int m_defaultCompression = 80;
  int m_fps = 0;
  int m_bandwidthLimit = 0;  // bits/sec
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
  wsConfig.port = port;
  wsConfig.bindAddress = listenAddress;
  m_signalingServer = std::make_shared<rtc::WebSocketServer>(wsConfig);
  m_signalingServer->onClient([this,
                               name](std::shared_ptr<rtc::WebSocket> socket) {
    socket->onOpen([this, name, socket] {
      rtc::Configuration config{.portRangeBegin = 1180, .portRangeEnd = 1190};
      auto connection = std::make_shared<rtc::PeerConnection>(config);
      rtc::DataChannelInit channelConfig{.reliability = {.unordered = true}};
      std::shared_ptr<rtc::DataChannel> channel =
          connection->createDataChannel(std::string{name}, channelConfig);

      wpi::json offer{{"type", connection->localDescription()->typeString()},
                      {"sdp", connection->localDescription().value()}};

      socket->onMessage([conn = std::weak_ptr<rtc::PeerConnection>(connection),
                         this](rtc::message_variant data) {
        auto message = std::get<std::string>(data);
        if (wpi::contains(message, "sdp")) {
          auto sdpAnswer = wpi::json::parse(message);
          if (auto peerConnection = conn.lock()) {
            peerConnection->setRemoteDescription(
                {sdpAnswer["sdp"].get<std::string>(),
                 sdpAnswer["type"].get<std::string>()});
          }
        }
      });
      auto source = GetSource();

      std::scoped_lock lock(m_mutex);
      // Find unoccupied worker thread, or create one if necessary
      auto it =
          std::find_if(m_connThreads.begin(), m_connThreads.end(),
                       [](const wpi::SafeThreadOwner<ConnThread>& owner) {
                         auto thr = owner.GetThread();
                         return !thr && (!thr->m_channel ||
                                         !thr->m_connection || !thr->m_socket);
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
      thr->m_bandwidthLimit = GetProperty(m_bandwidthLimitProp)->value;
      thr->m_connection = connection;
      thr->SetChannel(channel);
      thr->m_socket = socket;
      socket->send(offer.dump());
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

  m_bandwidthLimitProp = CreateProperty("bandwidth_limit", [] {
    return std::make_unique<PropertyImpl>("bandwidth_limit", CS_PROP_INTEGER, 1,
                                          4000000, 4000000);
  });
}

WebRTCServerImpl::~WebRTCServerImpl() {
  Stop();
}

void WebRTCServerImpl::Stop() {
  m_active = false;

  m_signalingServer->stop();

  // close streams
  for (auto& connThread : m_connThreads) {
    if (auto thr = connThread.GetThread()) {
      if (thr->m_connection) {
        thr->m_connection->close();
      }
      if (thr->m_channel) {
        thr->m_channel->close();
      }
      if (thr->m_socket) {
        thr->m_socket->close();
      }
    }
    connThread.Stop();
  }

  // wake up connection threads by forcing an empty frame to be sent
  if (auto source = GetSource()) {
    source->Wakeup();
  }
}

// worker thread for clients that connected to this server
void WebRTCServerImpl::ConnThread::Main() {
  std::unique_lock lock(m_mutex);
  while (m_active) {
    while (!m_channel || !m_channel->isOpen() || !m_socket ||
           !m_socket->isOpen()) {
      m_cond.wait(lock);
      if (!m_active) {
        return;
      }
    }
    m_socket->onMessage([this](rtc::message_variant data) {
      auto message = wpi::json::parse(std::get<std::string>(data));
      auto requestType = message["type"].get<std::string>();
      if (requestType == "settings") {
        SendJSON();
      } else if (requestType == "command") {
        ProcessRequest(message);
      } else if (requestType == "config") {
        CS_Status status = CS_OK;
        m_socket->send(m_source->GetConfigJson(&status));
      }
    });
    lock.unlock();
    SendStream();
    lock.lock();
    m_socket = nullptr;
    m_channel = nullptr;
    m_connection = nullptr;
  }
}

void WebRTCServerImpl::ConnThread::ProcessRequest(wpi::json message) {
  std::string response;
  if (!message.contains("options")) {
    response = "Malformed JSON payload: missing \"options\" object";
  } else {
    auto source = GetSource();
    // TODO: Use actual integers in the JSON?
    for (auto& [key, value] : message["options"].items()) {
      if (key == "resolution") {
        auto [widthStr, heightStr] = wpi::split(value, 'x');
        int width = wpi::parse_integer<int>(widthStr, 10).value_or(-1);
        int height = wpi::parse_integer<int>(heightStr, 10).value_or(-1);
        if (width < 0) {
          response += key + ": \"width is not an integer\"\r\n";
          SWARNING("Parameter \"{}\" width \"{}\" is not an integer",
                   key.c_str(), widthStr);
          continue;
        }
        if (height < 0) {
          response += key + ": \"height is not an integer\"\r\n";
          SWARNING("Parameter \"{}\" height \"{}\" is not an integer",
                   key.c_str(), heightStr);
          continue;
        }
        m_width = width;
        m_height = height;
        response += key + ": \"ok\"\r\n";
      } else if (key == "compression") {
        if (auto v = wpi::parse_integer<int>(value, 10)) {
          m_compression = v.value();
        } else {
          response += key + ": \"value is not an integer\"\r\n";
          SWARNING("Parameter \"{}\" value \"{}\" is not an integer",
                   key.c_str(), value.dump());
        }
      } else if (key == "fps") {
        if (auto v = wpi::parse_integer<int>(value, 10)) {
          m_fps = v.value();
          response += key + ": \"ok\"\r\n";
        } else {
          response += key + ": \"value is not an integer\"\r\n";
          SWARNING("Parameter \"{}\" value \"{}\" is not an integer",
                   key.c_str(), value.dump());
        }
      } else if (key == "bandwidth_limit") {
        if (auto v = wpi::parse_integer<int>(value, 10)) {
          if (v.value() > 7000000) {
            m_bandwidthLimit = 7000000;
            response += key + ": \"capped to 7 Mbit/s\"\r\n";
          } else {
            m_bandwidthLimit = v.value();
            response += key + ": \"ok\"\r\n";
          }
        } else {
          response += key + ": \"value is not an integer\"\r\n";
          SWARNING("Parameter \"{}\" value \"{}\" is not an integer",
                   key.c_str(), value.dump());
        }
      }
      // ignore name parameter
      if (message.contains("name")) {
        continue;
      }

      // try to assign parameter
      auto prop = m_source->GetPropertyIndex(key);
      if (!prop) {
        response += key + ": \"ignored\"\r\n";
        SWARNING("ignoring parameter \"{}\"", key.c_str());
        continue;
      }

      CS_Status status = 0;
      auto kind = m_source->GetPropertyKind(prop);
      switch (kind) {
        case CS_PROP_BOOLEAN:
        case CS_PROP_INTEGER:
        case CS_PROP_ENUM: {
          if (auto v = wpi::parse_integer<int>(value, 10)) {
            response += key + ": " + value.dump() + "\r\n";
            SDEBUG4("Parameter \"{}\" value {}", key.c_str(), value.dump());
            m_source->SetProperty(prop, v.value(), &status);
          } else {
            response += key + ": \"invalid integer\"\r\n";
            SWARNING("Parameter \"{}\" value \"{}\" is not an integer",
                     key.c_str(), value.dump());
          }
          break;
        }
        case CS_PROP_STRING: {
          response += key + ": \"ok\"\r\n";
          SDEBUG4("Parameter \"{}\" value \"{}\"", key.c_str(), value.dump());
          m_source->SetStringProperty(prop, value, &status);
          break;
        }
        default:
          break;
      }
    }
  }
  wpi::json jsonResponse{{"data", {"message", response}}};
  if (m_socket) {
    m_socket->send(jsonResponse.dump());
  }
}

void WebRTCServerImpl::ConnThread::SendStream() {
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
  uint64_t lastBandwidthSampleTime = 0;
  uint64_t lastBytesSent = 0;
  wpi::static_circular_buffer<int, 5> bandwidthBuffer;
  StartStream();
  while (m_active || m_channel->isOpen() || m_socket->isOpen()) {
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
    Frame::Time deltaTime = thisFrameTime - lastFrameTime;
    if (thisFrameTime != 0 && timePerFrame != 0 && lastFrameTime != 0) {
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
    // Sample bandwidth every 200 ms
    if (lastBandwidthSampleTime - thisFrameTime > 200000) {
      lastBandwidthSampleTime = thisFrameTime;
      uint64_t bytesSent = m_connection->bytesSent();
      uint64_t deltaBits = (bytesSent - lastBytesSent) * 8;
      if (deltaTime < 1000) {
        continue;
      }
      bandwidthBuffer.push_front(deltaBits / (deltaTime / 1000));
      lastBytesSent = bytesSent;

      int averageBandwidth = 0;
      for (int sample : bandwidthBuffer) {
        // 5 taps and 200 ms period create a rolling average over one second
        averageBandwidth += sample / 5;
      }
      if (averageBandwidth * 1000 > m_bandwidthLimit) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        continue;
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
    switch (image->pixelFormat) {
      case VideoMode::kMJPEG:
        // Determine if we need to add DHT to it, and allocate enough space
        // for adding it if required.
        addDHT = JpegNeedsDHT(data, &size, &locSOF);
        if (addDHT) {
          // Insert DHT data immediately before SOF
          wpi::SmallVector<char, 16000> buf;  // TODO: use something else?
          std::string_view before{data, locSOF};
          buf.append(before.begin(), before.end());
          buf.append(JpegGetDHT().begin(), JpegGetDHT().end());
          std::string_view sof{data + locSOF, image->size() - locSOF};
          buf.append(sof.begin(), sof.end());
          m_channel->sendBuffer(buf);
        } else {
          m_channel->sendBuffer(std::string_view{data, size});
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
    lastFrameTime = thisFrameTime;
  }
  StopStream();
}

// Send a JSON file which is contains information about the source parameters.
void WebRTCServerImpl::ConnThread::SendJSON() {
  wpi::json info;
  wpi::SmallVector<int, 32> properties_vec;
  CS_Status status = 0;
  for (auto prop : m_source->EnumerateProperties(properties_vec, &status)) {
    wpi::json property;
    wpi::SmallString<128> name_buf;
    auto name = m_source->GetPropertyName(prop, name_buf, &status);
    auto kind = m_source->GetPropertyKind(prop);
    property["name"] = name;
    property["id"] = prop;
    property["type"] = static_cast<int>(kind);
    property["min"] = m_source->GetPropertyMin(prop, &status);
    property["max"] = m_source->GetPropertyMax(prop, &status);
    property["step"] = m_source->GetPropertyStep(prop, &status);
    property["default"] = m_source->GetPropertyDefault(prop, &status);
    switch (kind) {
      case CS_PROP_BOOLEAN:
      case CS_PROP_INTEGER:
      case CS_PROP_ENUM:
        property["value"] = m_source->GetProperty(prop, &status);
        break;
      case CS_PROP_STRING: {
        wpi::SmallString<128> strval_buf;
        property["value"] =
            m_source->GetStringProperty(prop, strval_buf, &status);
        break;
      }
      default:
        break;
    }

    // append the menu object to the menu typecontrols
    if (m_source->GetPropertyKind(prop) == CS_PROP_ENUM) {
      wpi::json menu;
      auto choices = m_source->GetEnumPropertyChoices(prop, &status);
      int j = 0;
      for (auto choice = choices.begin(), end = choices.end(); choice != end;
           ++j, ++choice) {
        // replace any non-printable characters in name with spaces
        wpi::SmallString<128> ch_name;
        for (char ch : *choice) {
          ch_name.push_back(std::isprint(ch) ? ch : ' ');
        }
        menu[j] = ch_name.str();
      }
      property["menu"] = menu;
    }
    info["controls"].push_back(property);
  }
  wpi::json modes;
  for (auto mode : m_source->EnumerateVideoModes(&status)) {
    wpi::json videoMode;
    switch (mode.pixelFormat) {
      case VideoMode::kMJPEG:
        videoMode["pixelFormat"] = "MJPEG";
        break;
      case VideoMode::kYUYV:
        videoMode["pixelFormat"] = "YUYV";
        break;
      case VideoMode::kRGB565:
        videoMode["pixelFormat"] = "RGB565";
        break;
      case VideoMode::kBGR:
        videoMode["pixelFormat"] = "BGR";
        break;
      case VideoMode::kGray:
        videoMode["pixelFormat"] = "gray";
        break;
      case VideoMode::kY16:
        videoMode["pixelFormat"] = "Y16";
        break;
      case VideoMode::kUYVY:
        videoMode["pixelFormat"] = "UYVY";
        break;
      default:
        videoMode["pixelFormat"] = "unknown";
        break;
    }
    videoMode["width"] = mode.width;
    videoMode["height"] = mode.height;
    videoMode["fps"] = mode.fps;
    modes.push_back(videoMode);
  }
  info["modes"] = modes;
  if (m_socket) {
    m_socket->send(info.dump());
  }
}

void WebRTCServerImpl::ConnThread::SetChannel(
    std::shared_ptr<rtc::DataChannel> channel) {
  m_channel = channel;
  m_channel->onOpen([this] { m_cond.notify_one(); });
}

void WebRTCServerImpl::SetSourceImpl(std::shared_ptr<SourceImpl> source) {
  std::scoped_lock lock(m_mutex);
  for (auto& connThread : m_connThreads) {
    if (auto thr = connThread.GetThread()) {
      if (thr->m_source != source) {
        bool streaming = thr->m_streaming;
        if (thr->m_source && streaming) {
          thr->m_source->DisableSink();
        }
        thr->m_source = source;
        if (source && streaming) {
          thr->m_source->EnableSink();
        }
      }
    }
  }
}

namespace cs {
CS_Sink CreateWebRTCServer(std::string_view name,
                           std::string_view listenAddress, int port,
                           CS_Status* status) {
  auto& inst = Instance::GetInstance();
  return inst.CreateSink(
      CS_SINK_WEBRTC,
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
