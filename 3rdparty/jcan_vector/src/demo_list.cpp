#include "jcan_vector/discovery.hpp"
#include "jcan_vector/vector_device.hpp"

#include <cstdio>

int main() {
  const auto devices = jcan_vector::list_vector_devices();
  std::printf("found %zu vector channel(s)\n", devices.size());
  for (const auto& d : devices) {
    std::printf("  %s  port=%s pid=0x%04x\n", d.friendly_name.c_str(), d.port.c_str(), d.pid);
  }
  if (devices.empty()) return 0;

  jcan_vector::VectorDevice dev;
  jcan_vector::OpenConfig cfg;
  cfg.port = devices.front().port;
  cfg.bitrate_arb = 500000;
  const auto err = dev.open(cfg);
  if (err != jcan_vector::Error::Ok) {
    std::printf("open failed: %s (%s)\n", jcan_vector::to_string(err), dev.last_error().c_str());
    return 1;
  }
  std::printf("opened %s\n", cfg.port.c_str());
  std::vector<jcan_vector::CanFrame> frames;
  for (int i = 0; i < 5; ++i) {
    dev.recv_many(frames, 100);
    std::printf("poll %d: %zu frame(s)\n", i, frames.size());
  }
  dev.close();
  return 0;
}
