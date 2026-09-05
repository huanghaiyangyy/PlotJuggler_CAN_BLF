#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "jcan_vector/discovery.hpp"

namespace jcan_vector {

enum class Error : uint8_t {
  Ok = 0,
  NotOpen,
  AlreadyOpen,
  OpenFailed,
  PermissionDenied,
  IoError,
  Unavailable,
  Unknown,
};

inline const char* to_string(Error e) {
  switch (e) {
    case Error::Ok: return "ok";
    case Error::NotOpen: return "not_open";
    case Error::AlreadyOpen: return "already_open";
    case Error::OpenFailed: return "open_failed";
    case Error::PermissionDenied: return "permission_denied";
    case Error::IoError: return "io_error";
    case Error::Unavailable: return "unavailable";
    case Error::Unknown: return "unknown";
  }
  return "unknown";
}

struct CanFrame {
  uint32_t id = 0;
  bool extended = false;
  bool rtr = false;
  bool error = false;
  uint8_t dlc = 0;
  std::array<uint8_t, 64> data{};
  bool fd = false;
  bool brs = false;
  bool tx = false;
  uint8_t source = 0xff;
  double timestamp_sec = 0.0;  // host steady clock seconds from open epoch
};

struct OpenConfig {
  std::string port;  // from DeviceInfo::port (path:primary_channel)
  uint32_t bitrate_arb = 500000;
  uint32_t bitrate_data = 2000000;  // reserved for later; open maps arb via slcan table
  bool listen_only = false;
  /** Extra channels to activate after primary (from port). Empty = primary only. */
  std::vector<uint8_t> channels;
};

/** C++17-safe facade over jcan vector_xl (implementation may use C++23). */
class VectorDevice {
 public:
  VectorDevice();
  ~VectorDevice();
  VectorDevice(const VectorDevice&) = delete;
  VectorDevice& operator=(const VectorDevice&) = delete;

  Error open(const OpenConfig& cfg);
  Error close();
  bool is_open() const;

  Error send(const CanFrame& frame);
  Error recv_many(std::vector<CanFrame>& out, unsigned timeout_ms);

  std::string last_error() const;

 private:
  struct Impl;
  Impl* impl_;
};

}  // namespace jcan_vector
