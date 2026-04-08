#include "blf_decoder.h"

#include "blf_series_naming.h"

#include <algorithm>
#include <cstdio>
#include <exception>

namespace PJ::BLF
{
namespace
{
constexpr uint32_t kExtendedCanIdFlag = 0x80000000U;

uint64_t RawSeriesCacheKey(const NormalizedCanFrame& frame)
{
  const uint32_t id_with_kind = frame.id | (frame.extended ? kExtendedCanIdFlag : 0U);
  return (static_cast<uint64_t>(frame.channel) << 32U) | static_cast<uint64_t>(id_with_kind);
}

std::string RawByteSeriesNameFromPrefix(const std::string& prefix, std::size_t byte_index)
{
  char suffix[10] = {};
  std::snprintf(suffix, sizeof(suffix), "/data_%02u", static_cast<unsigned>(byte_index));
  std::string full_name;
  full_name.reserve(prefix.size() + sizeof(suffix) - 1U);
  full_name = prefix;
  full_name += suffix;
  return full_name;
}
}  // namespace

BlfDecoderPipeline::BlfDecoderPipeline(const BlfPluginConfig& config, DbcManager* dbc_manager,
                                       ISeriesWriter* series_writer)
  : config_(config), dbc_manager_(dbc_manager), series_writer_(series_writer)
{
}

bool BlfDecoderPipeline::ProcessFrame(const NormalizedCanFrame& frame)
{
  ++stats_.frames_processed;
  if (!series_writer_)
  {
    ++stats_.decode_errors;
    return false;
  }

  const double timestamp = ResolveTimestamp(frame);
  bool wrote_raw = false;

  if (config_.emit_raw)
  {
    EmitRaw(frame, timestamp);
    wrote_raw = true;
  }

  if (config_.emit_decoded && dbc_manager_)
  {
    try
    {
      const bool decoded = EmitDecoded(frame, timestamp);
      // When a frame is not decodable by DBC (unknown channel/ID/signals), fall back to raw bytes.
      if (!decoded && !wrote_raw)
      {
        EmitRaw(frame, timestamp);
        wrote_raw = true;
      }
    }
    catch (const std::exception&)
    {
      ++stats_.decode_errors;
      if (!wrote_raw)
      {
        EmitRaw(frame, timestamp);
      }
      return false;
    }
  }

  return true;
}

BlfDecodeStats BlfDecoderPipeline::stats() const
{
  return stats_;
}

const BlfDecoderPipeline::RawSeriesNames& BlfDecoderPipeline::GetOrCreateRawSeriesNames(
    const NormalizedCanFrame& frame)
{
  const uint64_t cache_key = RawSeriesCacheKey(frame);
  const auto cache_it = raw_series_names_cache_.find(cache_key);
  if (cache_it != raw_series_names_cache_.end())
  {
    return cache_it->second;
  }

  const BlfFrameKey key{frame.channel, frame.id, frame.extended};
  RawSeriesNames names;
  const std::string prefix = RawSeriesPrefix(key);

  names.dlc = prefix + "/dlc";
  names.is_fd = prefix + "/is_fd";
  names.is_brs = prefix + "/is_brs";
  names.is_esi = prefix + "/is_esi";
  for (std::size_t i = 0; i < names.data.size(); ++i)
  {
    names.data[i] = RawByteSeriesNameFromPrefix(prefix, i);
  }

  const auto inserted = raw_series_names_cache_.emplace(cache_key, std::move(names));
  return inserted.first->second;
}

const std::string& BlfDecoderPipeline::GetOrCreateDecodedSeriesName(uint32_t channel,
                                                                    const std::string& message,
                                                                    const std::string& signal)
{
  auto& message_map = decoded_series_names_cache_[channel];
  auto message_it = message_map.find(message);
  if (message_it == message_map.end())
  {
    message_it = message_map.emplace(message, DecodedSignalMap{}).first;
  }

  auto& signal_map = message_it->second;
  auto signal_it = signal_map.find(signal);
  if (signal_it == signal_map.end())
  {
    signal_it = signal_map.emplace(signal, DecodedSeriesName(channel, message, signal)).first;
  }
  return signal_it->second;
}

double BlfDecoderPipeline::ResolveTimestamp(const NormalizedCanFrame& frame) const
{
  if (config_.use_source_timestamp)
  {
    return frame.timestamp;
  }
  return static_cast<double>(stats_.frames_processed - 1U);
}

void BlfDecoderPipeline::EmitRaw(const NormalizedCanFrame& frame, double timestamp)
{
  const auto& names = GetOrCreateRawSeriesNames(frame);

  series_writer_->WriteSample(names.dlc, timestamp, static_cast<double>(frame.dlc));
  series_writer_->WriteSample(names.is_fd, timestamp, frame.is_fd ? 1.0 : 0.0);
  series_writer_->WriteSample(names.is_brs, timestamp, frame.is_brs ? 1.0 : 0.0);
  series_writer_->WriteSample(names.is_esi, timestamp, frame.is_esi ? 1.0 : 0.0);
  stats_.raw_samples_written += 4;

  const std::size_t payload_size =
      std::min<std::size_t>(frame.size, static_cast<std::size_t>(frame.data.size()));
  for (std::size_t i = 0; i < payload_size; ++i)
  {
    series_writer_->WriteSample(names.data[i], timestamp, static_cast<double>(frame.data[i]));
    ++stats_.raw_samples_written;
  }
}

bool BlfDecoderPipeline::EmitDecoded(const NormalizedCanFrame& frame, double timestamp)
{
  const std::size_t payload_size =
      std::min<std::size_t>(frame.size, static_cast<std::size_t>(frame.data.size()));
  const auto decoded_signals = dbc_manager_->DecodeForChannel(
      frame.channel, frame.id, frame.extended, frame.is_fd, frame.dlc, frame.data.data(),
      payload_size);

  for (const auto& signal : decoded_signals)
  {
    series_writer_->WriteSample(
        GetOrCreateDecodedSeriesName(frame.channel, signal.message, signal.signal), timestamp,
        signal.value);
    ++stats_.decoded_samples_written;
  }
  return !decoded_signals.empty();
}

}  // namespace PJ::BLF
