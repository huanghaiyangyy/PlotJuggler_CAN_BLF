#include "blf_recorder.h"
#include "blf_reader.h"

#include <cstdio>
#include <cstring>

#include <QString>

int main(int argc, char** argv)
{
  const char* path = argc > 1 ? argv[1] : "/tmp/vector_jcan_smoke.blf";
  PJ::BLF::BlfRecorder rec;
  std::string err;
  if (!rec.open(path, &err))
  {
    std::fprintf(stderr, "open write failed: %s\n", err.c_str());
    return 1;
  }
  for (int i = 0; i < 20; ++i)
  {
    PJ::BLF::NormalizedCanFrame f;
    f.timestamp = i * 0.01;
    f.channel = 0;
    f.id = 0x100;
    f.is_fd = true;
    f.is_brs = true;
    f.dlc = 8;
    f.size = 8;
    f.data[0] = static_cast<uint8_t>((i >> 8) & 0xff);
    f.data[1] = static_cast<uint8_t>(i & 0xff);
    f.data[2] = static_cast<uint8_t>(i % 100);
    rec.write(f);
  }
  rec.close();

  int count = 0;
  QString qerr;
  PJ::BLF::BlfReader reader;
  if (!reader.ReadFrames(
          QString::fromUtf8(path),
          [&](const PJ::BLF::NormalizedCanFrame& f) {
            if (f.id == 0x100) ++count;
          },
          qerr))
  {
    std::fprintf(stderr, "read failed: %s\n", qerr.toUtf8().constData());
    return 2;
  }
  std::printf("wrote and read back %d frames from %s\n", count, path);
  return count >= 20 ? 0 : 3;
}
