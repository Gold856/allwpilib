// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#ifndef CSCORE_MJPEGSERVERIMPL_H_
#define CSCORE_MJPEGSERVERIMPL_H_

#include <atomic>
#include <memory>
#include <rtc/datachannel.hpp>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <rtc/rtc.hpp>
#include <wpi/SafeThread.h>
#include <wpi/SmallVector.h>
#include <wpi/raw_istream.h>
#include <wpi/raw_ostream.h>
#include <wpinet/NetworkAcceptor.h>
#include <wpinet/NetworkStream.h>
#include <wpinet/raw_socket_ostream.h>

#include "SinkImpl.h"

namespace cs {

class SourceImpl;

class WebRTCServerImpl : public SinkImpl {
 public:
  WebRTCServerImpl(std::string_view name, wpi::Logger& logger,
                   Notifier& notifier, Telemetry& telemetry,
                   std::string_view listenAddress, int port,
                   std::unique_ptr<wpi::NetworkAcceptor> acceptor);
  ~WebRTCServerImpl() override;

  void Stop();
  std::string GetListenAddress() { return m_listenAddress; }
  int GetPort() { return m_port; }

 private:
  void SetSourceImpl(std::shared_ptr<SourceImpl> source) override;

  void ServerThreadMain();

  class ConnThread;

  // Never changed, so not protected by mutex
  std::string m_listenAddress;
  int m_port;

  std::unique_ptr<wpi::NetworkAcceptor> m_acceptor;
  std::atomic_bool m_active;  // set to false to terminate threads
  std::thread m_serverThread;

  std::vector<wpi::SafeThreadOwner<ConnThread>> m_connThreads;

  // property indices
  int m_widthProp;
  int m_heightProp;
  int m_compressionProp;
  int m_defaultCompressionProp;
  int m_fpsProp;
  std::shared_ptr<rtc::PeerConnection> m_connection;
  std::shared_ptr<rtc::DataChannel> m_dataChannel;
};

}  // namespace cs

#endif  // CSCORE_MJPEGSERVERIMPL_H_
