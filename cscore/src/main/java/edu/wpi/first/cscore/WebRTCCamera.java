// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

package edu.wpi.first.cscore;

/** A source that represents a WebRTC camera. */
public class WebRTCCamera extends VideoCamera {
  /** WebRTC camera kind. */
  public enum WebRTCCameraKind {
    /** Unknown camera kind. */
    kUnknown(0),
    /** MJPG camera. */
    kMJPG(1);

    private final int value;

    WebRTCCameraKind(int value) {
      this.value = value;
    }

    /**
     * Returns WebRTCCameraKind value.
     *
     * @return WebRTCCameraKind value.
     */
    public int getValue() {
      return value;
    }
  }

  /**
   * Convert from the numerical representation of kind to an enum type.
   *
   * @param kind The numerical representation of kind
   * @return The kind
   */
  public static WebRTCCameraKind getWebRTCCameraKindFromInt(int kind) {
    return switch (kind) {
      case 1 -> WebRTCCameraKind.kMJPG;
      default -> WebRTCCameraKind.kUnknown;
    };
  }

  /**
   * Create a source for a WebRTC camera.
   *
   * @param name Source name (arbitrary unique identifier)
   * @param address Camera address (e.g. "http://10.x.y.11/video/stream.mjpg")
   */
  public WebRTCCamera(String name, String address) {
    super(CameraServerJNI.createWebRTCCamera(name, address, WebRTCCameraKind.kUnknown.getValue()));
  }

  /**
   * Create a source for a WebRTC camera.
   *
   * @param name Source name (arbitrary unique identifier)
   * @param address Camera address (e.g. "http://10.x.y.11/video/stream.mjpg")
   * @param kind Camera kind (e.g. kAxis)
   */
  public WebRTCCamera(String name, String address, WebRTCCameraKind kind) {
    super(CameraServerJNI.createWebRTCCamera(name, address, kind.getValue()));
  }

  /**
   * Create a source for a WebRTC camera.
   *
   * @param name Source name (arbitrary unique identifier)
   * @param addresses Array of camera addresses
   */
  public WebRTCCamera(String name, String[] addresses) {
    super(
        CameraServerJNI.createWebRTCCameraMulti(
            name, addresses, WebRTCCameraKind.kUnknown.getValue()));
  }

  /**
   * Create a source for a WebRTC camera.
   *
   * @param name Source name (arbitrary unique identifier)
   * @param addresses Array of Camera addresses
   * @param kind Camera kind (e.g. kAxis)
   */
  public WebRTCCamera(String name, String[] addresses, WebRTCCameraKind kind) {
    super(CameraServerJNI.createWebRTCCameraMulti(name, addresses, kind.getValue()));
  }

  /**
   * Get the kind of WebRTC camera.
   *
   * <p>Autodetection can result in returning a different value than the camera was created with.
   *
   * @return The kind of WebRTC camera.
   */
  public WebRTCCameraKind getWebRTCCameraKind() {
    return getWebRTCCameraKindFromInt(CameraServerJNI.getWebRTCCameraKind(m_handle));
  }

  /**
   * Change the addresses used to connect to the camera.
   *
   * @param addresses Array of camera addresses
   */
  public void setUrls(String[] addresses) {
    CameraServerJNI.setWebRTCCameraAddresses(m_handle, addresses);
  }

  /**
   * Get the addresses used to connect to the camera.
   *
   * @return Array of camera addresses.
   */
  public String[] getUrls() {
    return CameraServerJNI.getWebRTCCameraAddresses(m_handle);
  }
}
