// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#ifndef CSCORE_WEBRTCCAMERAIMPL_H_
#define CSCORE_WEBRTCCAMERAIMPL_H_

#include <atomic>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <rtc/datachannel.hpp>
#include <rtc/peerconnection.hpp>
#include <rtc/websocket.hpp>
#include <wpi/SmallString.h>
#include <wpi/StringMap.h>
#include <wpi/condition_variable.h>

#include "SourceImpl.h"
#include "cscore_c.h"
#include "cscore_cpp.h"

namespace cs {

class WebRTCCameraImpl : public SourceImpl {
 public:
  WebRTCCameraImpl(std::string_view name, CS_WebRTCCameraKind kind,
                   wpi::Logger& logger, Notifier& notifier,
                   Telemetry& telemetry);
  ~WebRTCCameraImpl() override;

  void Start() override;

  // Property functions
  void SetProperty(int property, int value, CS_Status* status) override;
  void SetStringProperty(int property, std::string_view value,
                         CS_Status* status) override;

  // Standard common camera properties
  void SetBrightness(int brightness, CS_Status* status) override;
  int GetBrightness(CS_Status* status) const override;
  void SetWhiteBalanceAuto(CS_Status* status) override;
  void SetWhiteBalanceHoldCurrent(CS_Status* status) override;
  void SetWhiteBalanceManual(int value, CS_Status* status) override;
  void SetExposureAuto(CS_Status* status) override;
  void SetExposureHoldCurrent(CS_Status* status) override;
  void SetExposureManual(int value, CS_Status* status) override;

  bool SetVideoMode(const VideoMode& mode, CS_Status* status) override;

  void NumSinksChanged() override;
  void NumSinksEnabledChanged() override;

  CS_WebRTCCameraKind GetKind() const;
  bool SetAddresses(std::span<const std::string> addresses, CS_Status* status);
  std::vector<std::string> GetAddresses() const;

  // Property data
  class PropertyData : public PropertyImpl {
   public:
    PropertyData() = default;
    explicit PropertyData(std::string_view name_) : PropertyImpl{name_} {}
    PropertyData(std::string_view name_, std::string_view httpParam_,
                 bool viaSettings_, CS_PropertyKind kind_, int minimum_,
                 int maximum_, int step_, int defaultValue_, int value_)
        : PropertyImpl(name_, kind_, step_, defaultValue_, value_),
          viaSettings(viaSettings_),
          httpParam(httpParam_) {
      hasMinimum = true;
      minimum = minimum_;
      hasMaximum = true;
      maximum = maximum_;
    }
    ~PropertyData() override = default;

    bool viaSettings{false};
    std::string httpParam;
  };

 protected:
  std::unique_ptr<PropertyImpl> CreateEmptyProperty(
      std::string_view name) const override;

  bool CacheProperties(CS_Status* status) const override;

  void CreateProperty(std::string_view name, std::string_view httpParam,
                      bool viaSettings, CS_PropertyKind kind, int minimum,
                      int maximum, int step, int defaultValue, int value) const;

  template <typename T>
  void CreateEnumProperty(std::string_view name, std::string_view httpParam,
                          bool viaSettings, int defaultValue, int value,
                          std::initializer_list<T> choices) const;

 private:
  // The camera streaming thread
  void StreamThreadMain();

  // Functions used by StreamThreadMain()
  bool DeviceStreamConnect();

  void DeviceSendSettings();

  // The monitor thread
  void MonitorThreadMain();

  std::atomic_bool m_connected{false};
  std::atomic_bool m_active{true};  // set to false to terminate thread
  std::thread m_streamThread;
  std::thread m_monitorThread;

  //
  // Variables protected by m_mutex
  //

  // The camera connections
  std::shared_ptr<rtc::WebSocket> m_socket;
  std::shared_ptr<rtc::PeerConnection> m_connection;
  std::shared_ptr<rtc::DataChannel> m_channel;

  CS_WebRTCCameraKind m_kind;

  std::vector<std::string> m_locations;
  size_t m_nextLocation{0};
  int m_prefLocation{-1};  // preferred location

  std::atomic_int m_frameCount{0};

  wpi::condition_variable m_sinkEnabledCond;

  wpi::StringMap<std::string> m_settings;

  wpi::StringMap<std::string> m_streamSettings;
  std::atomic_bool m_streamSettingsUpdated{false};

  wpi::condition_variable m_monitorCond;
  wpi::condition_variable m_reconnectStreamCond;
};

}  // namespace cs

#endif  // CSCORE_WEBRTCCAMERAIMPL_H_
