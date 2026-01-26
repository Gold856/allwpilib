import pytest
import math

from wpimath import Rotation2d, SwerveModuleState


kEpsilon = 1e-9


def test_optimize():
    angle_a = Rotation2d.fromDegrees(45)
    ref_a = SwerveModuleState(-2, Rotation2d.fromDegrees(180))
    optimized_a = ref_a.optimize(angle_a)

    assert optimized_a.speed == pytest.approx(2.0, abs=kEpsilon)
    assert optimized_a.angle.degrees() == pytest.approx(0.0, abs=kEpsilon)

    angle_b = Rotation2d.fromDegrees(-50)
    ref_b = SwerveModuleState(4.7, Rotation2d.fromDegrees(41))
    optimized_b = ref_b.optimize(angle_b)

    assert optimized_b.speed == pytest.approx(-4.7, abs=kEpsilon)
    assert optimized_b.angle.degrees() == pytest.approx(-139.0, abs=kEpsilon)


def test_no_optimize():
    angle_a = Rotation2d.fromDegrees(0)
    ref_a = SwerveModuleState(2, Rotation2d.fromDegrees(89))
    optimized_a = ref_a.optimize(angle_a)

    assert optimized_a.speed == pytest.approx(2.0, abs=kEpsilon)
    assert optimized_a.angle.degrees() == pytest.approx(89.0, abs=kEpsilon)

    angle_b = Rotation2d.fromDegrees(0)
    ref_b = SwerveModuleState(-2, Rotation2d.fromDegrees(-2))
    optimized_b = ref_b.optimize(angle_b)

    assert optimized_b.speed == pytest.approx(-2.0, abs=kEpsilon)
    assert optimized_b.angle.degrees() == pytest.approx(-2.0, abs=kEpsilon)


def test_cosine_scaling():
    angle_a = Rotation2d.fromDegrees(0)
    ref_a = SwerveModuleState(2, Rotation2d.fromDegrees(45))
    optimized_a = ref_a.cosineScale(angle_a)

    assert optimized_a.speed == pytest.approx(math.sqrt(2.0), abs=kEpsilon)
    assert optimized_a.angle.degrees() == pytest.approx(45.0, abs=kEpsilon)

    angle_b = Rotation2d.fromDegrees(45)
    ref_b = SwerveModuleState(2, Rotation2d.fromDegrees(45))
    optimized_b = ref_b.cosineScale(angle_b)

    assert optimized_b.speed == pytest.approx(2.0, abs=kEpsilon)
    assert optimized_b.angle.degrees() == pytest.approx(45.0, abs=kEpsilon)

    angle_c = Rotation2d.fromDegrees(-45)
    ref_c = SwerveModuleState(2, Rotation2d.fromDegrees(45))
    optimized_c = ref_c.cosineScale(angle_c)

    assert optimized_c.speed == pytest.approx(0.0, abs=kEpsilon)
    assert optimized_c.angle.degrees() == pytest.approx(45.0, abs=kEpsilon)

    angle_d = Rotation2d.fromDegrees(135)
    ref_d = SwerveModuleState(2, Rotation2d.fromDegrees(45))
    optimized_d = ref_d.cosineScale(angle_d)

    assert optimized_d.speed == pytest.approx(0.0, abs=kEpsilon)
    assert optimized_d.angle.degrees() == pytest.approx(45.0, abs=kEpsilon)

    angle_e = Rotation2d.fromDegrees(-135)
    ref_e = SwerveModuleState(2, Rotation2d.fromDegrees(45))
    optimized_e = ref_e.cosineScale(angle_e)

    assert optimized_e.speed == pytest.approx(-2.0, abs=kEpsilon)
    assert optimized_e.angle.degrees() == pytest.approx(45.0, abs=kEpsilon)

    angle_f = Rotation2d.fromDegrees(180)
    ref_f = SwerveModuleState(2, Rotation2d.fromDegrees(45))
    optimized_f = ref_f.cosineScale(angle_f)

    assert optimized_f.speed == pytest.approx(-math.sqrt(2.0), abs=kEpsilon)
    assert optimized_f.angle.degrees() == pytest.approx(45.0, abs=kEpsilon)

    angle_g = Rotation2d.fromDegrees(0)
    ref_g = SwerveModuleState(-2, Rotation2d.fromDegrees(45))
    optimized_g = ref_g.cosineScale(angle_g)

    assert optimized_g.speed == pytest.approx(-math.sqrt(2.0), abs=kEpsilon)
    assert optimized_g.angle.degrees() == pytest.approx(45.0, abs=kEpsilon)

    angle_h = Rotation2d.fromDegrees(45)
    ref_h = SwerveModuleState(-2, Rotation2d.fromDegrees(45))
    optimized_h = ref_h.cosineScale(angle_h)

    assert optimized_h.speed == pytest.approx(-2.0, abs=kEpsilon)
    assert optimized_h.angle.degrees() == pytest.approx(45.0, abs=kEpsilon)

    angle_i = Rotation2d.fromDegrees(-45)
    ref_i = SwerveModuleState(-2, Rotation2d.fromDegrees(45))
    optimized_i = ref_i.cosineScale(angle_i)

    assert optimized_i.speed == pytest.approx(0.0, abs=kEpsilon)
    assert optimized_i.angle.degrees() == pytest.approx(45.0, abs=kEpsilon)

    angle_j = Rotation2d.fromDegrees(135)
    ref_j = SwerveModuleState(-2, Rotation2d.fromDegrees(45))
    optimized_j = ref_j.cosineScale(angle_j)

    assert optimized_j.speed == pytest.approx(0.0, abs=kEpsilon)
    assert optimized_j.angle.degrees() == pytest.approx(45.0, abs=kEpsilon)

    angle_k = Rotation2d.fromDegrees(-135)
    ref_k = SwerveModuleState(-2, Rotation2d.fromDegrees(45))
    optimized_k = ref_k.cosineScale(angle_k)

    assert optimized_k.speed == pytest.approx(2.0, abs=kEpsilon)
    assert optimized_k.angle.degrees() == pytest.approx(45.0, abs=kEpsilon)

    angle_l = Rotation2d.fromDegrees(180)
    ref_l = SwerveModuleState(-2, Rotation2d.fromDegrees(45))
    optimized_l = ref_l.cosineScale(angle_l)

    assert optimized_l.speed == pytest.approx(math.sqrt(2.0), abs=kEpsilon)
    assert optimized_l.angle.degrees() == pytest.approx(45.0, abs=kEpsilon)


def test_equality():
    state1 = SwerveModuleState(2, Rotation2d.fromDegrees(90))
    state2 = SwerveModuleState(2, Rotation2d.fromDegrees(90))

    assert state1 == state2


def test_inequality():
    state1 = SwerveModuleState(1, Rotation2d.fromDegrees(90))
    state2 = SwerveModuleState(2, Rotation2d.fromDegrees(90))
    state3 = SwerveModuleState(1, Rotation2d.fromDegrees(89))

    assert state1 != state2
    assert state1 != state3
