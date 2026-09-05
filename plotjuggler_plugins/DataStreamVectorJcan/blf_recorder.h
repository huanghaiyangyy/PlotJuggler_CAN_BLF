#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include "blf_decoder.h"

namespace PJ::BLF
{

/** Thin wrapper around Vector::BLF::File for realtime CAN/CAN FD recording (GPL-3). */
class BlfRecorder
{
public:
  BlfRecorder();
  ~BlfRecorder();

  BlfRecorder(const BlfRecorder&) = delete;
  BlfRecorder& operator=(const BlfRecorder&) = delete;

  bool open(const std::filesystem::path& path, std::string* error = nullptr);
  void write(const NormalizedCanFrame& frame);
  void close();
  bool is_open() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace PJ::BLF
