// Copyright (c) FIRST and other WPILib contributors.
// Open Source Software; you can modify and/or share it under the terms of
// the WPILib BSD license file in the root directory of this project.

#include "Sftp.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <Ws2tcpip.h>
#endif
#include <string>
#include <utility>
#include <vector>

#include <libssh2.h>
#include <libssh2_sftp.h>

using namespace sftp;

Attributes::Attributes(char* name, size_t len, LIBSSH2_SFTP_ATTRIBUTES&& attr)
    : name{name, len}, flags{attr.flags}, size{attr.filesize} {
  // sftp_attributes_free(attr);
}

static std::string GetError(LIBSSH2_SFTP* sftp) {
  switch (libssh2_sftp_last_error(sftp)) {
    // case SSH_FX_EOF:
    //   return "end of file";
    // case SSH_FX_NO_SUCH_FILE:
    //   return "no such file";
    // case SSH_FX_PERMISSION_DENIED:
    //   return "permission denied";
    // case SSH_FX_FAILURE:
    //   return "SFTP failure";
    // case SSH_FX_BAD_MESSAGE:
    //   return "SFTP bad message";
    // case SSH_FX_NO_CONNECTION:
    //   return "SFTP no connection";
    // case SSH_FX_CONNECTION_LOST:
    //   return "SFTP connection lost";
    // case SSH_FX_OP_UNSUPPORTED:
    //   return "SFTP operation unsupported";
    // case SSH_FX_INVALID_HANDLE:
    //   return "SFTP invalid handle";
    // case SSH_FX_NO_SUCH_PATH:
    //   return "no such path";
    // case SSH_FX_FILE_ALREADY_EXISTS:
    //   return "file already exists";
    // case SSH_FX_WRITE_PROTECT:
    //   return "write protected filesystem";
    // case SSH_FX_NO_MEDIA:
    //   return "no media inserted";
    default:
      // libssh2_session_last_error(sftp);
      return ;
  }
}

Exception::Exception(LIBSSH2_SFTP* sftp)
    : runtime_error{GetError(sftp)}, err{libssh2_sftp_last_error(sftp)} {}

File::~File() {
  if (m_handle) {
    libssh2_sftp_close_handle(m_handle);
  }
}

Attributes File::Stat() const {
  LIBSSH2_SFTP_ATTRIBUTES attr;
  if (!libssh2_sftp_fstat(m_handle, &attr)) {
    throw Exception{m_handle->sftp};
  }
  return Attributes{&attr};
}

size_t File::Read(char* buf, uint32_t count) {
  auto rv = libssh2_sftp_read(m_handle, buf, count);
  if (rv < 0) {
    throw Exception{m_handle->sftp};
  }
  return rv;
}

size_t File::Write(std::span<const char> data) {
  auto rv = libssh2_sftp_write(m_handle, data.data(), data.size());
  if (rv < 0) {
    throw Exception{m_handle->sftp};
  }
  return rv;
}

void File::Seek(uint64_t offset) {
  libssh2_sftp_seek64(m_handle, offset);
}

uint64_t File::Tell() const {
  return libssh2_sftp_tell64(m_handle);
}

void File::Rewind() {
  libssh2_sftp_rewind(m_handle);
}

void File::Sync() {
  if (libssh2_sftp_fsync(m_handle) < 0) {
    throw Exception{m_handle->sftp};
  }
}

Session::Session(std::string_view host, int port, std::string_view user,
                 std::string_view pass)
    : m_host{host}, m_port{port}, m_username{user}, m_password{pass} {
  // Create a new SSH session.
  m_session = libssh2_session_init();
  if (!m_session) {
    throw Exception{"The SSH session could not be allocated."};
  }
#ifdef _WIN32
  WSAData wsaData;
  WORD wVersionRequested = MAKEWORD(2, 2);
  WSAStartup(wVersionRequested, &wsaData);
#endif
  // Set timeout to 3 seconds.
  int64_t timeout = 3L;
  libssh2_session_set_timeout(m_session, timeout);

  // // Set other miscellaneous options.
  // ssh_options_set(m_session, SSH_OPTIONS_STRICTHOSTKEYCHECK, "no");
}

Session::~Session() {
  if (m_sftp) {
    libssh2_sftp_shutdown(m_sftp);
  }
  if (m_session) {
    libssh2_session_free(m_session);
  }
}

void Session::Connect() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  struct addrinfo* info;
  int status = getaddrinfo(m_host.c_str(), std::to_string(m_port).c_str(),
                           nullptr, &info);
  int conn = connect(sock, info[0].ai_addr, info[0].ai_addrlen);
  // Connect to the server.
  int rc = libssh2_session_handshake(m_session, sock);
  if (rc != 0) {
    char* msg;
    int len;
    libssh2_session_last_error(m_session, &msg, &len, 0);
    throw Exception{{msg, (size_t)len}};
  }

  // Authenticate with password.
  libssh2_userauth_password_ex(m_session, m_username.c_str(),
                               m_username.length(), m_password.c_str(),
                               m_password.length(), nullptr);
  if (rc != 0) {
    char* msg;
    int len;
    libssh2_session_last_error(m_session, &msg, &len, 0);
    throw Exception{{msg, (size_t)len}};
  }

  // Initialize the SFTP session.
  m_sftp = libssh2_sftp_init(m_session);
  if (rc != 0) {
    libssh2_sftp_shutdown(m_sftp);
    m_sftp = nullptr;
    char* msg;
    int len;
    libssh2_session_last_error(m_session, &msg, &len, 0);
    throw Exception{{msg, (size_t)len}};
  }
}

void Session::Disconnect() {
  if (m_sftp) {
    libssh2_sftp_shutdown(m_sftp);
    m_sftp = nullptr;
  }
  // TODO: null safe?
  libssh2_session_disconnect(m_session, nullptr);
}

std::vector<Attributes> Session::ReadDir(const std::string& path) {
  auto dir = libssh2_sftp_opendir(m_sftp, path.c_str());
  if (!dir) {
    throw Exception{m_sftp};
  }

  std::vector<Attributes> rv;
  char buf[1000];
  LIBSSH2_SFTP_ATTRIBUTES attr;
  while (auto len = libssh2_sftp_readdir(dir, buf, 1000, &attr)) {
    rv.emplace_back(buf, (size_t)len, std::move(attr));
  }

  libssh2_sftp_closedir(dir);
  return rv;
}

void Session::Unlink(const std::string& filename) {
  if (libssh2_sftp_unlink(m_sftp, filename.c_str()) < 0) {
    throw Exception{m_sftp};
  }
}

File Session::Open(const std::string& filename, int accesstype, long mode) {
  LIBSSH2_SFTP_HANDLE* f =
      libssh2_sftp_open(m_sftp, filename.c_str(), accesstype, mode);
  if (!f) {
    throw Exception{m_sftp};
  }
  return File{f};
}
