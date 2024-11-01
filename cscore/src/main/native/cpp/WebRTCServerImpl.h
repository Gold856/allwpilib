// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#ifndef CSCORE_WEBRTCSERVERIMPL_H_
#define CSCORE_WEBRTCSERVERIMPL_H_

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <rtc/websocketserver.hpp>
#include <wpi/SafeThread.h>

#include "SinkImpl.h"

namespace cs {

class SourceImpl;

class WebRTCServerImpl : public SinkImpl {
 public:
  WebRTCServerImpl(std::string_view name, wpi::Logger& logger,
                   Notifier& notifier, Telemetry& telemetry,
                   std::string_view listenAddress, int port);
  ~WebRTCServerImpl() override;

  void Stop();
  std::string GetListenAddress() { return m_listenAddress; }
  int GetPort() { return m_port; }

 private:
  void SetSourceImpl(std::shared_ptr<SourceImpl> source) override;

  class ConnThread;

  // Never changed, so not protected by mutex
  std::string m_listenAddress;
  int m_port;

  std::atomic_bool m_active;  // set to false to terminate threads

  std::vector<wpi::SafeThreadOwner<ConnThread>> m_connThreads;

  // property indices
  int m_widthProp;
  int m_heightProp;
  int m_compressionProp;
  int m_defaultCompressionProp;
  int m_fpsProp;
  int m_bandwidthLimitProp;  // bits/sec
  std::shared_ptr<rtc::WebSocketServer> m_signalingServer;
};

}  // namespace cs

#endif  // CSCORE_WEBRTCSERVERIMPL_H_
