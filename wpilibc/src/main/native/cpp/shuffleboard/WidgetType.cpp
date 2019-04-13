/*----------------------------------------------------------------------------*/
/* Copyright (c) 2018 FIRST. All Rights Reserved.                             */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in the root directory of */
/* the project.                                                               */
/*----------------------------------------------------------------------------*/

#include "frc/shuffleboard/WidgetType.h"
#pragma warning(disable: 4244 4267 4146)

using namespace frc;

wpi::StringRef WidgetType::GetWidgetName() const { return m_widgetName; }
