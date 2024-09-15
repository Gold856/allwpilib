// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include <cstdio>
#include <rtc/global.hpp>

#include <wpi/print.h>

#include "cscore.h"
#include "cscore_cpp.h"

int main() {
  rtc::InitLogger(rtc::LogLevel::Debug);
  wpi::print("hostname: {}\n", cs::GetHostname());
  std::puts("IPv4 network addresses:");
  for (const auto& addr : cs::GetNetworkInterfaces()) {
    wpi::print("  {}\n", addr);
  }
  cs::UsbCamera camera{"usbcam", cs::UsbCamera::EnumerateUsbCameras()[0].dev};
  // camera.SetVideoMode(cs::VideoMode::kMJPEG, 320, 240, 30);
  cs::WebRTCServer webrtcServer{"httpserver", 8081};
  webrtcServer.SetSource(camera);
  cs::SetDefaultLogger(0);
  CS_Status status = 0;
  cs::AddListener(
      [&](const cs::RawEvent& event) {
        wpi::print("FPS={} MBPS={}\n", camera.GetActualFPS(),
                   (camera.GetActualDataRate() / 1000000.0));
      },
      cs::RawEvent::kTelemetryUpdated, false, &status);
  cs::SetTelemetryPeriod(1.0);

  std::getchar();
}
