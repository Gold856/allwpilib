// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include <hal/SimDevice.h>

extern "C" {
HAL_SimDeviceHandle HAL_CreateSimDevice(const char* name) {
  return 0;
}
}  // extern "C"
