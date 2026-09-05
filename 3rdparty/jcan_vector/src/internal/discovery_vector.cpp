#include "jcan_vector/discovery.hpp"

#include <cstdio>
#include <libusb.h>

namespace jcan_vector {
namespace {

struct Known {
  uint16_t pid;
  const char* label;
  int num_channels;
};

constexpr Known kDevices[] = {
    {0x1073, "VN1640A", 4},
    {0x1072, "VN1630A", 2},
    {0x1074, "VN1610", 2},
};

const Known* find_pid(uint16_t pid) {
  for (const auto& k : kDevices) {
    if (k.pid == pid) return &k;
  }
  return nullptr;
}

std::string usb_path_for(libusb_device* dev) {
  uint8_t bus = libusb_get_bus_number(dev);
  uint8_t ports[8] = {};
  int n = libusb_get_port_numbers(dev, ports, 8);
  char buf[64];
  if (n <= 0) {
    std::snprintf(buf, sizeof(buf), "%u", bus);
    return buf;
  }
  int off = std::snprintf(buf, sizeof(buf), "%u-", bus);
  for (int i = 0; i < n && off < (int)sizeof(buf) - 4; ++i) {
    off += std::snprintf(buf + off, sizeof(buf) - off, "%s%u", (i ? "." : ""), ports[i]);
  }
  return buf;
}

}  // namespace

std::vector<DeviceInfo> list_vector_devices() {
  std::vector<DeviceInfo> out;
  libusb_context* ctx = nullptr;
  if (libusb_init(&ctx) != 0) return out;

  libusb_device** list = nullptr;
  const ssize_t cnt = libusb_get_device_list(ctx, &list);
  if (cnt < 0) {
    libusb_exit(ctx);
    return out;
  }

  constexpr uint16_t kVid = 0x1248;
  for (ssize_t i = 0; i < cnt; ++i) {
    libusb_device* dev = list[i];
    libusb_device_descriptor desc{};
    if (libusb_get_device_descriptor(dev, &desc) != 0) continue;
    if (desc.idVendor != kVid) continue;
    const Known* known = find_pid(desc.idProduct);
    if (!known) continue;

    const std::string path = usb_path_for(dev);
    for (int ch = 0; ch < known->num_channels; ++ch) {
      DeviceInfo info;
      info.usb_path = path;
      info.channel = static_cast<uint8_t>(ch);
      info.pid = known->pid;
      info.num_channels = known->num_channels;
      char port[96];
      std::snprintf(port, sizeof(port), "%s:%d", path.c_str(), ch);
      info.port = port;
      char name[128];
      std::snprintf(name, sizeof(name), "%s ch%d (%s)", known->label, ch, path.c_str());
      info.friendly_name = name;
      out.push_back(info);
    }
  }

  libusb_free_device_list(list, 1);
  libusb_exit(ctx);
  return out;
}

}  // namespace jcan_vector
