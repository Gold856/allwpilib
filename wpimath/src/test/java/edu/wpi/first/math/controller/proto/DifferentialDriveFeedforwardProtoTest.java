// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

package edu.wpi.first.math.controller.proto;

import static org.junit.jupiter.api.Assertions.assertEquals;

import edu.wpi.first.math.controller.DifferentialDriveFeedforward;
import edu.wpi.first.math.proto.Controller.ProtobufDifferentialDriveFeedforward;
import edu.wpi.first.wpilibj.ProtoTestBase;

@SuppressWarnings("PMD.TestClassWithoutTestCases")
class DifferentialDriveFeedforwardProtoTest
    extends ProtoTestBase<DifferentialDriveFeedforward, ProtobufDifferentialDriveFeedforward> {
  DifferentialDriveFeedforwardProtoTest() {
    super(
        new DifferentialDriveFeedforward(0.174, 0.229, 4.4, 4.5),
        DifferentialDriveFeedforward.proto);
  }

  @Override
  public void checkEquals(
      DifferentialDriveFeedforward testData, DifferentialDriveFeedforward data) {
    assertEquals(testData.kVLinear, data.kVLinear);
    assertEquals(testData.kALinear, data.kALinear);
    assertEquals(testData.kVAngular, data.kVAngular);
    assertEquals(testData.kAAngular, data.kAAngular);
  }
}
