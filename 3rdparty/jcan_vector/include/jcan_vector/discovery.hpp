#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace jcan_vector {

struct DeviceInfo {
  std::string port;
  std::string friendly_name;
  std::string usb_path;
  uint8_t channel = 0;
  uint16_t pid = 0;
  int num_channels = 1;
};

std::vector<DeviceInfo> list_vector_devices();

}  // namespace jcan_vector
