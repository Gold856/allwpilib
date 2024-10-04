// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include "WebRTCCameraImpl.h"

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <rtc/common.hpp>
#include <rtc/datachannel.hpp>
#include <rtc/description.hpp>
#include <rtc/peerconnection.hpp>
#include <wpi/StringExtras.h>
#include <wpi/json.h>
#include <wpi/timestamp.h>

#include "Image.h"
#include "Instance.h"
#include "JpegUtil.h"
#include "Log.h"
#include "Notifier.h"
#include "Telemetry.h"
#include "c_util.h"
#include "cscore_cpp.h"

using namespace cs;

WebRTCCameraImpl::WebRTCCameraImpl(std::string_view name,
                                   CS_WebRTCCameraKind kind,
                                   wpi::Logger& logger, Notifier& notifier,
                                   Telemetry& telemetry)
    : SourceImpl{name, logger, notifier, telemetry}, m_kind{kind} {}

WebRTCCameraImpl::~WebRTCCameraImpl() {
  m_active = false;

  // force wakeup of monitor thread
  m_monitorCond.notify_one();

  // join monitor thread
  if (m_monitorThread.joinable()) {
    m_monitorThread.join();
  }

  // Destroy connections if they weren't already
  {
    std::scoped_lock lock(m_mutex);
    if (m_socket) {
      m_socket = nullptr;
    }
    if (m_channel) {
      m_channel = nullptr;
    }
    if (m_connection) {
      m_connection = nullptr;
    }
  }

  // force wakeup of camera thread in case it's waiting on cv
  m_sinkEnabledCond.notify_one();

  // join camera thread
  if (m_streamThread.joinable()) {
    m_streamThread.join();
  }
}

void WebRTCCameraImpl::Start() {
  // Kick off the stream and settings threads
  m_streamThread = std::thread(&WebRTCCameraImpl::StreamThreadMain, this);
  m_monitorThread = std::thread(&WebRTCCameraImpl::MonitorThreadMain, this);
}

void WebRTCCameraImpl::MonitorThreadMain() {
  while (m_active) {
    std::unique_lock lock(m_mutex);
    // sleep for 1 second between checks
    m_monitorCond.wait_for(lock, std::chrono::seconds(1),
                           [=, this] { return !m_active; });

    if (!m_active) {
      break;
    }

    // check to see if we got any frames, and close the stream if not
    // (this will result in an error at the read point, and ultimately
    // a reconnect attempt)
    if (m_channel && m_frameCount == 0) {
      SWARNING("Monitor detected stream hung, disconnecting");
      m_reconnectStreamCond.notify_one();
    }

    // reset the frame counter
    m_frameCount = 0;
  }

  SDEBUG("Monitor Thread exiting");
}

void WebRTCCameraImpl::StreamThreadMain() {
  while (m_active) {
    SetConnected(false);

    // sleep between retries
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // disconnect if not enabled
    if (!IsEnabled()) {
      std::unique_lock lock(m_mutex);
      if (m_channel) {
        m_channel = nullptr;
      }
      if (m_connection) {
        m_connection = nullptr;
      }
      // Wait for enable
      m_sinkEnabledCond.wait(lock,
                             [=, this] { return !m_active || IsEnabled(); });
      if (!m_active) {
        return;
      }
    }

    // connect
    bool connected = DeviceStreamConnect();

    if (!m_active) {
      break;
    }

    // keep retrying
    if (!connected) {
      continue;
    }

    // update connected since we're actually connected
    SetConnected(true);

    // Wait for a reason to reconnect
    {
      std::unique_lock lock(m_mutex);
      m_reconnectStreamCond.wait(lock);
    }
  }

  SDEBUG("Camera Thread exiting");
  SetConnected(false);
}

bool WebRTCCameraImpl::DeviceStreamConnect() {
  {
    std::scoped_lock lock(m_mutex);
    if (m_locations.empty()) {
      SERROR("locations array is empty!?");
      std::this_thread::sleep_for(std::chrono::seconds(1));
      return false;
    }
    if (m_nextLocation >= m_locations.size()) {
      m_nextLocation = 0;
    }
    m_socket = std::make_shared<rtc::WebSocket>();
    rtc::Configuration config{.portRangeBegin = 1180, .portRangeEnd = 1190};
    m_connection = std::make_shared<rtc::PeerConnection>(config);
    m_connection->onDataChannel(
        [this](std::shared_ptr<rtc::DataChannel> channel) {
          std::scoped_lock lock(m_mutex);
          m_channel = channel;
          m_channel->onClosed([this] { m_reconnectStreamCond.notify_one(); });
          m_channel->onMessage([this](rtc::message_variant msg) {
            auto data = std::get<std::vector<std::byte>>(msg);
            // We know how big it is!  Just get a frame of the right size and
            // read the data directly into it.
            auto image = std::make_unique<Image>(data);
            if (!m_active) {
              return;
            }
            int width, height;
            if (!GetJpegSize(image->str(), &width, &height)) {
              SWARNING("did not receive a JPEG image");
              PutError("did not receive a JPEG image", wpi::Now());
              return;
            }
            image->width = width;
            image->height = height;
            image->pixelFormat = VideoMode::PixelFormat::kMJPEG;
            PutFrame(std::move(image), wpi::Now());
            ++m_frameCount;
            // update video mode if not set
            std::scoped_lock lock(m_mutex);
            if (m_mode.pixelFormat != VideoMode::PixelFormat::kMJPEG ||
                m_mode.width == 0 || m_mode.height == 0) {
              m_mode.pixelFormat = VideoMode::PixelFormat::kMJPEG;
              m_mode.width = width;
              m_mode.height = height;
            }
          });
        });
    m_socket->onMessage([this](rtc::message_variant msg) {
      auto message = wpi::json::parse(std::get<std::string>(msg));
      if (message["type"] == "offer") {
        m_connection->setRemoteDescription(
            {message["sdp"].get<std::string>(),
             message["type"].get<std::string>()});
        wpi::json answer{
            {"type", m_connection->localDescription()->typeString()},
            {"sdp", m_connection->localDescription().value()}};
        m_socket->send(answer.dump());
      }
    });
    auto url = "ws://" + m_locations[m_nextLocation++];
    m_socket->open(url);
    m_socket->onClosed([this] { m_reconnectStreamCond.notify_one(); });
    using State = rtc::PeerConnection::State;
    m_connection->onStateChange([this](State state) {
      if (state == State::Closed || state == State::Disconnected ||
          state == State::Failed) {
        m_reconnectStreamCond.notify_one();
      }
    });
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  if (!m_active || !m_channel || !m_channel->isOpen() || !m_socket ||
      !m_socket->isOpen()) {
    return false;
  }
  // Push settings now that we're connected
  DeviceSendSettings();
  {
    std::scoped_lock lock(m_mutex);
    m_frameCount = 1;  // avoid a race with monitor thread
  }
  return true;
}

void WebRTCCameraImpl::DeviceSendSettings() {
  wpi::json settings{{"type", "command"}};
  for (auto& [key, val] : m_settings) {
    settings["options"][key] = val;
  }
  for (auto& [key, val] : m_streamSettings) {
    settings["options"][key] = val;
  }
  if (m_socket && m_socket->isOpen()) {
    m_socket->send(settings.dump());
  }
}

CS_WebRTCCameraKind WebRTCCameraImpl::GetKind() const {
  std::scoped_lock lock(m_mutex);
  return m_kind;
}

bool WebRTCCameraImpl::SetAddresses(std::span<const std::string> addresses,
                                    CS_Status* status) {
  std::scoped_lock lock(m_mutex);
  for (const auto& address : addresses) {
    m_locations.push_back(address);
  }

  m_nextLocation = 0;
  m_streamSettingsUpdated = true;
  m_reconnectStreamCond.notify_one();
  return true;
}

std::vector<std::string> WebRTCCameraImpl::GetAddresses() const {
  std::scoped_lock lock(m_mutex);
  std::vector<std::string> addresses;
  for (const auto& loc : m_locations) {
    addresses.push_back(loc);
  }
  return addresses;
}

void WebRTCCameraImpl::CreateProperty(std::string_view name,
                                      std::string_view httpParam,
                                      bool viaSettings, CS_PropertyKind kind,
                                      int minimum, int maximum, int step,
                                      int defaultValue, int value) const {
  std::scoped_lock lock(m_mutex);
  m_propertyData.emplace_back(std::make_unique<PropertyData>(
      name, httpParam, viaSettings, kind, minimum, maximum, step, defaultValue,
      value));

  m_notifier.NotifySourceProperty(*this, CS_SOURCE_PROPERTY_CREATED, name,
                                  m_propertyData.size() + 1, kind, value, {});
}

template <typename T>
void WebRTCCameraImpl::CreateEnumProperty(
    std::string_view name, std::string_view httpParam, bool viaSettings,
    int defaultValue, int value, std::initializer_list<T> choices) const {
  std::scoped_lock lock(m_mutex);
  m_propertyData.emplace_back(std::make_unique<PropertyData>(
      name, httpParam, viaSettings, CS_PROP_ENUM, 0, choices.size() - 1, 1,
      defaultValue, value));

  auto& enumChoices = m_propertyData.back()->enumChoices;
  enumChoices.clear();
  for (const auto& choice : choices) {
    enumChoices.emplace_back(choice);
  }

  m_notifier.NotifySourceProperty(*this, CS_SOURCE_PROPERTY_CREATED, name,
                                  m_propertyData.size() + 1, CS_PROP_ENUM,
                                  value, {});
  m_notifier.NotifySourceProperty(*this, CS_SOURCE_PROPERTY_CHOICES_UPDATED,
                                  name, m_propertyData.size() + 1, CS_PROP_ENUM,
                                  value, {});
}

std::unique_ptr<PropertyImpl> WebRTCCameraImpl::CreateEmptyProperty(
    std::string_view name) const {
  return std::make_unique<PropertyData>(name);
}

bool WebRTCCameraImpl::CacheProperties(CS_Status* status) const {
  std::scoped_lock lock(m_mutex);

  // Pretty typical set of video modes
  m_videoModes.clear();
  m_videoModes.emplace_back(VideoMode::kMJPEG, 640, 480, 30);
  m_videoModes.emplace_back(VideoMode::kMJPEG, 320, 240, 30);
  m_videoModes.emplace_back(VideoMode::kMJPEG, 160, 120, 30);

  m_properties_cached = true;
  return true;
}

void WebRTCCameraImpl::SetProperty(int property, int value, CS_Status* status) {
  // TODO
}

void WebRTCCameraImpl::SetStringProperty(int property, std::string_view value,
                                         CS_Status* status) {
  // TODO
}

void WebRTCCameraImpl::SetBrightness(int brightness, CS_Status* status) {
  // TODO
}

int WebRTCCameraImpl::GetBrightness(CS_Status* status) const {
  // TODO
  return 0;
}

void WebRTCCameraImpl::SetWhiteBalanceAuto(CS_Status* status) {
  // TODO
}

void WebRTCCameraImpl::SetWhiteBalanceHoldCurrent(CS_Status* status) {
  // TODO
}

void WebRTCCameraImpl::SetWhiteBalanceManual(int value, CS_Status* status) {
  // TODO
}

void WebRTCCameraImpl::SetExposureAuto(CS_Status* status) {
  // TODO
}

void WebRTCCameraImpl::SetExposureHoldCurrent(CS_Status* status) {
  // TODO
}

void WebRTCCameraImpl::SetExposureManual(int value, CS_Status* status) {
  // TODO
}

bool WebRTCCameraImpl::SetVideoMode(const VideoMode& mode, CS_Status* status) {
  if (mode.pixelFormat != VideoMode::kMJPEG) {
    return false;
  }
  std::scoped_lock lock(m_mutex);
  m_mode = mode;
  m_streamSettings.clear();
  if (mode.width != 0 && mode.height != 0) {
    m_streamSettings["resolution"] =
        fmt::format("{}x{}", mode.width, mode.height);
  }
  if (mode.fps != 0) {
    m_streamSettings["fps"] = fmt::format("{}", mode.fps);
  }
  DeviceSendSettings();
  return true;
}

void WebRTCCameraImpl::NumSinksChanged() {
  // ignore
}

void WebRTCCameraImpl::NumSinksEnabledChanged() {
  m_sinkEnabledCond.notify_one();
}

namespace cs {

CS_Source CreateWebRTCCamera(std::string_view name, std::string_view address,
                             CS_WebRTCCameraKind kind, CS_Status* status) {
  auto& inst = Instance::GetInstance();
  std::shared_ptr<WebRTCCameraImpl> source = std::make_shared<WebRTCCameraImpl>(
      name, kind, inst.logger, inst.notifier, inst.telemetry);
  std::string addressStr{address};
  if (!source->SetAddresses(std::span{&addressStr, 1}, status)) {
    return 0;
  }
  return inst.CreateSource(CS_SOURCE_WEBRTC, source);
}

CS_Source CreateWebRTCCamera(std::string_view name,
                             std::span<const std::string> addresses,
                             CS_WebRTCCameraKind kind, CS_Status* status) {
  auto& inst = Instance::GetInstance();
  if (addresses.empty()) {
    *status = CS_EMPTY_VALUE;
    return 0;
  }
  auto source = std::make_shared<WebRTCCameraImpl>(
      name, kind, inst.logger, inst.notifier, inst.telemetry);
  if (!source->SetAddresses(addresses, status)) {
    return 0;
  }
  return inst.CreateSource(CS_SOURCE_WEBRTC, source);
}

CS_WebRTCCameraKind GetWebRTCCameraKind(CS_Source source, CS_Status* status) {
  auto data = Instance::GetInstance().GetSource(source);
  if (!data || data->kind != CS_SOURCE_HTTP) {
    *status = CS_INVALID_HANDLE;
    return CS_WEBRTC_UNKNOWN;
  }
  return static_cast<WebRTCCameraImpl&>(*data->source).GetKind();
}

void SetWebRTCCameraAddresses(CS_Source source,
                              std::span<const std::string> addresses,
                              CS_Status* status) {
  if (addresses.empty()) {
    *status = CS_EMPTY_VALUE;
    return;
  }
  auto data = Instance::GetInstance().GetSource(source);
  if (!data || data->kind != CS_SOURCE_WEBRTC) {
    *status = CS_INVALID_HANDLE;
    return;
  }
  static_cast<WebRTCCameraImpl&>(*data->source).SetAddresses(addresses, status);
}

std::vector<std::string> GetWebRTCCameraAddresses(CS_Source source,
                                                  CS_Status* status) {
  auto data = Instance::GetInstance().GetSource(source);
  if (!data || data->kind != CS_SOURCE_WEBRTC) {
    *status = CS_INVALID_HANDLE;
    return std::vector<std::string>{};
  }
  return static_cast<WebRTCCameraImpl&>(*data->source).GetAddresses();
}

}  // namespace cs

extern "C" {

CS_Source CS_CreateWebRTCCamera(const struct WPI_String* name,
                                const struct WPI_String* address,
                                CS_WebRTCCameraKind kind, CS_Status* status) {
  return cs::CreateWebRTCCamera(wpi::to_string_view(name),
                                wpi::to_string_view(address), kind, status);
}

CS_Source CS_CreateWebRTCCameraMulti(const struct WPI_String* name,
                                     const struct WPI_String* addresses,
                                     int count, CS_WebRTCCameraKind kind,
                                     CS_Status* status) {
  wpi::SmallVector<std::string, 4> vec;
  vec.reserve(count);
  for (int i = 0; i < count; ++i) {
    vec.emplace_back(wpi::to_string_view(&addresses[i]));
  }
  return cs::CreateWebRTCCamera(wpi::to_string_view(name), vec, kind, status);
}

CS_WebRTCCameraKind CS_GetWebRTCKind(CS_Source source, CS_Status* status) {
  return cs::GetWebRTCCameraKind(source, status);
}

void CS_SetWebRTCCameraUrls(CS_Source source,
                            const struct WPI_String* addresses, int count,
                            CS_Status* status) {
  wpi::SmallVector<std::string, 4> vec;
  vec.reserve(count);
  for (int i = 0; i < count; ++i) {
    vec.emplace_back(wpi::to_string_view(&addresses[i]));
  }
  cs::SetWebRTCCameraAddresses(source, vec, status);
}

WPI_String* CS_GetWebRTCCameraUrls(CS_Source source, int* count,
                                   CS_Status* status) {
  auto addresses = cs::GetWebRTCCameraAddresses(source, status);
  WPI_String* out = WPI_AllocateStringArray(addresses.size());
  *count = addresses.size();
  for (size_t i = 0; i < addresses.size(); ++i) {
    cs::ConvertToC(&out[i], addresses[i]);
  }
  return out;
}

}  // extern "C"
