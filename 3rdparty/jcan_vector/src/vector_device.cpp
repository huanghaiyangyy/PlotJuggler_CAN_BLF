#include "jcan_vector/vector_device.hpp"

#include "internal/firmware_blobs.hpp"
#include "internal/hardware_vector.hpp"
#include "internal/types.hpp"

#include <chrono>
#include <cstring>
#include <memory>
#include <string>

namespace jcan_vector {
namespace {

jcan::slcan_bitrate map_bitrate(uint32_t bps) {
  using jcan::slcan_bitrate;
  if (bps <= 10000) return slcan_bitrate::s0;
  if (bps <= 20000) return slcan_bitrate::s1;
  if (bps <= 50000) return slcan_bitrate::s2;
  if (bps <= 100000) return slcan_bitrate::s3;
  if (bps <= 125000) return slcan_bitrate::s4;
  if (bps <= 250000) return slcan_bitrate::s5;
  if (bps <= 500000) return slcan_bitrate::s6;
  if (bps <= 800000) return slcan_bitrate::s7;
  return slcan_bitrate::s8;
}

Error map_error(jcan::error_code ec) {
  using jcan::error_code;
  switch (ec) {
    case error_code::ok: return Error::Ok;
    case error_code::not_open: return Error::NotOpen;
    case error_code::already_open: return Error::AlreadyOpen;
    case error_code::permission_denied: return Error::PermissionDenied;
    case error_code::port_open_failed:
    case error_code::port_not_found:
    case error_code::port_config_failed:
    case error_code::interface_not_found:
      return Error::OpenFailed;
    case error_code::write_error:
    case error_code::read_error:
    case error_code::socket_error:
    case error_code::frame_parse_error:
      return Error::IoError;
    case error_code::read_timeout:
      return Error::Ok;
    default:
      return Error::Unknown;
  }
}

CanFrame from_jcan(const jcan::can_frame& f, std::chrono::steady_clock::time_point t0) {
  CanFrame o;
  o.id = f.id;
  o.extended = f.extended;
  o.rtr = f.rtr;
  o.error = f.error;
  o.dlc = f.dlc;
  o.data = f.data;
  o.fd = f.fd;
  o.brs = f.brs;
  o.tx = f.tx;
  o.source = f.source;
  o.timestamp_sec = std::chrono::duration<double>(f.timestamp - t0).count();
  return o;
}

jcan::can_frame to_jcan(const CanFrame& f, std::chrono::steady_clock::time_point t0) {
  jcan::can_frame o;
  o.id = f.id;
  o.extended = f.extended;
  o.rtr = f.rtr;
  o.error = f.error;
  o.dlc = f.dlc;
  o.data = f.data;
  o.fd = f.fd;
  o.brs = f.brs;
  o.tx = f.tx;
  o.source = f.source;
  o.timestamp = t0 + std::chrono::duration_cast<jcan::can_frame::clock::duration>(
                         std::chrono::duration<double>(f.timestamp_sec));
  return o;
}

}  // namespace

struct VectorDevice::Impl {
  jcan::vector_xl dev;
  bool open = false;
  std::string last_error;
  std::chrono::steady_clock::time_point t0{};
};

VectorDevice::VectorDevice() : impl_(new Impl) {}
VectorDevice::~VectorDevice() {
  close();
  delete impl_;
  impl_ = nullptr;
}

Error VectorDevice::open(const OpenConfig& cfg) {
  if (impl_->open) return Error::AlreadyOpen;
  if (cfg.port.empty()) {
    impl_->last_error = "empty port";
    return Error::OpenFailed;
  }
  auto r = impl_->dev.open(cfg.port, map_bitrate(cfg.bitrate_arb), 0);
  if (!r) {
    impl_->last_error = jcan::to_string(r.error());
    return map_error(r.error());
  }
  impl_->t0 = std::chrono::steady_clock::now();
  impl_->open = true;
  impl_->last_error.clear();
  return Error::Ok;
}

Error VectorDevice::close() {
  if (!impl_->open) return Error::NotOpen;
  auto r = impl_->dev.close();
  impl_->open = false;
  if (!r) {
    impl_->last_error = jcan::to_string(r.error());
    return map_error(r.error());
  }
  return Error::Ok;
}

bool VectorDevice::is_open() const { return impl_ && impl_->open; }

Error VectorDevice::send(const CanFrame& frame) {
  if (!impl_->open) return Error::NotOpen;
  auto r = impl_->dev.send(to_jcan(frame, impl_->t0));
  if (!r) {
    impl_->last_error = jcan::to_string(r.error());
    return map_error(r.error());
  }
  return Error::Ok;
}

Error VectorDevice::recv_many(std::vector<CanFrame>& out, unsigned timeout_ms) {
  out.clear();
  if (!impl_->open) return Error::NotOpen;
  auto r = impl_->dev.recv_many(timeout_ms);
  if (!r) {
    impl_->last_error = jcan::to_string(r.error());
    return map_error(r.error());
  }
  out.reserve(r->size());
  for (const auto& f : *r) out.push_back(from_jcan(f, impl_->t0));
  return Error::Ok;
}

std::string VectorDevice::last_error() const {
  return impl_ ? impl_->last_error : std::string();
}

}  // namespace jcan_vector
