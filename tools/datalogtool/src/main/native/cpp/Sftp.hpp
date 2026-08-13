// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#pragma once

#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <libssh2.h>
#include <libssh2_sftp.h>

namespace sftp {

struct Attributes {
  Attributes() = default;
  explicit Attributes(char* name, size_t len, LIBSSH2_SFTP_ATTRIBUTES&& attr);

  std::string name;
  uint32_t flags = 0;
  uint8_t type = 0;
  uint64_t size = 0;
};

/**
 * This is the exception that will be thrown if something goes wrong.
 */
class Exception : public std::runtime_error {
 public:
  explicit Exception(const std::string& msg) : std::runtime_error{msg} {}
  explicit Exception(LIBSSH2_SFTP* sftp);

  unsigned long err = 0;
};

class File {
 public:
  File() = default;
  explicit File(LIBSSH2_SFTP_HANDLE* handle) : m_handle{handle} {}
  ~File();

  Attributes Stat() const;

  size_t Read(char* buf, uint32_t count);
  size_t Write(std::span<const char> data);

  void Seek(uint64_t offset);
  uint64_t Tell() const;
  void Rewind();

  void Sync();

 private:
  LIBSSH2_SFTP_HANDLE* m_handle{nullptr};
};

/**
 * This class is a C++ implementation of the SshSessionController in
 * wpilibsuite/deploy-utils. It handles connecting to an SSH server, running
 * commands, and transferring files.
 */
class Session {
 public:
  /**
   * Constructs a new session controller.
   *
   * @param host The hostname of the server to connect to.
   * @param port The port that the sshd server is operating on.
   * @param user The username to login as.
   * @param pass The password for the given username.
   */
  Session(std::string_view host, int port, std::string_view user,
          std::string_view pass);

  /**
   * Destroys the controller object. This also disconnects the session from the
   * server.
   */
  ~Session();

  /**
   * Opens the SSH connection to the given host.
   */
  void Connect();

  /**
   * Disconnects the SSH connection.
   */
  void Disconnect();

  /**
   * Reads directory entries
   *
   * @param path remote path
   * @return vector of file attributes
   */
  std::vector<Attributes> ReadDir(const std::string& path);

  /**
   * Unlinks (deletes) a file.
   *
   * @param filename filename
   */
  void Unlink(const std::string& filename);

  /**
   * Opens a file.
   *
   * @param filename filename
   * @param accesstype O_RDONLY, O_WRONLY, or O_RDWR, combined with O_CREAT,
   *                   O_EXCL, or O_TRUNC
   * @param mode permissions to use if a new file is created
   * @return File
   */
  File Open(const std::string& filename, int accesstype, long mode);

 private:
  LIBSSH2_SESSION* m_session{nullptr};
  LIBSSH2_SFTP* m_sftp{nullptr};
  std::string m_host;

  int m_port;

  std::string m_username;
  std::string m_password;
};

}  // namespace sftp
