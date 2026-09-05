#include "blf_recorder.h"

#include <Vector/BLF.h>

#include <cstring>

namespace PJ::BLF
{
namespace
{
constexpr uint32_t kExtendedCanFlag = 0x80000000U;

uint32_t PackCanId(uint32_t id, bool extended)
{
  return extended ? (id | kExtendedCanFlag) : (id & 0x7FFU);
}
}  // namespace

struct BlfRecorder::Impl
{
  Vector::BLF::File file;
  bool open = false;
  uint64_t t0_ns = 0;
  bool t0_set = false;
};

BlfRecorder::BlfRecorder() : impl_(std::make_unique<Impl>()) {}
BlfRecorder::~BlfRecorder() { close(); }

bool BlfRecorder::open(const std::filesystem::path& path, std::string* error)
{
  close();
  try
  {
    impl_->file.open(path.string(), std::ios_base::out);
    if (!impl_->file.is_open())
    {
      if (error) *error = "Vector::BLF::File::open failed";
      return false;
    }
    impl_->open = true;
    impl_->t0_set = false;
    return true;
  }
  catch (const std::exception& ex)
  {
    if (error) *error = ex.what();
    return false;
  }
}

bool BlfRecorder::is_open() const { return impl_ && impl_->open; }

void BlfRecorder::write(const NormalizedCanFrame& frame)
{
  if (!impl_ || !impl_->open) return;

  const uint64_t stamp_ns = static_cast<uint64_t>(frame.timestamp * 1e9);
  if (!impl_->t0_set)
  {
    impl_->t0_ns = stamp_ns;
    impl_->t0_set = true;
  }
  const uint64_t rel_ns = stamp_ns >= impl_->t0_ns ? stamp_ns - impl_->t0_ns : 0;

  if (frame.is_fd)
  {
    auto* msg = new Vector::BLF::CanFdMessage();
    msg->objectFlags = Vector::BLF::ObjectHeader::ObjectFlags::TimeOneNans;
    msg->objectTimeStamp = rel_ns;
    msg->channel = static_cast<uint16_t>(frame.channel + 1);  // BLF channels are 1-based often
    msg->flags = 0;
    if (frame.extended) { /* id flag below */ }
    msg->dlc = frame.dlc;
    msg->id = PackCanId(frame.id, frame.extended);
    msg->frameLength = 0;
    msg->arbBitCount = 0;
    msg->canFdFlags = Vector::BLF::CanFdMessage::EDL;
    if (frame.is_brs) msg->canFdFlags |= Vector::BLF::CanFdMessage::BRS;
    if (frame.is_esi) msg->canFdFlags |= Vector::BLF::CanFdMessage::ESI;
    msg->validDataBytes = frame.size;
    std::memcpy(msg->data.data(), frame.data.data(), frame.size);
    impl_->file.write(msg);
  }
  else
  {
    auto* msg = new Vector::BLF::CanMessage();
    msg->objectFlags = Vector::BLF::ObjectHeader::ObjectFlags::TimeOneNans;
    msg->objectTimeStamp = rel_ns;
    msg->channel = static_cast<uint16_t>(frame.channel + 1);
    msg->flags = 0;
    msg->dlc = frame.dlc > 8 ? 8 : frame.dlc;
    msg->id = PackCanId(frame.id, frame.extended);
    std::memcpy(msg->data.data(), frame.data.data(), msg->dlc);
    impl_->file.write(msg);
  }
}

void BlfRecorder::close()
{
  if (!impl_ || !impl_->open) return;
  try
  {
    impl_->file.close();
  }
  catch (...)
  {
  }
  impl_->open = false;
}

}  // namespace PJ::BLF
