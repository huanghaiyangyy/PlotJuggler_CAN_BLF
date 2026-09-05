#pragma once

#include <QtPlugin>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <QString>
#include <QTimer>

#include "PlotJuggler/datastreamer_base.h"
#include "blf_config.h"
#include "blf_decoder.h"
#include "blf_recorder.h"
#include "dbc_manager.h"
#include "jcan_vector/vector_device.hpp"

class DataStreamVectorJcan : public PJ::DataStreamer
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "facontidavide.PlotJuggler3.DataStreamer")
  Q_INTERFACES(PJ::DataStreamer)

public:
  DataStreamVectorJcan();
  ~DataStreamVectorJcan() override;

  bool start(QStringList* pre_selected_sources) override;
  void shutdown() override;
  bool isRunning() const override;

  const char* name() const override { return "Vector VN1640A (jcan)"; }
  bool isDebugPlugin() override { return false; }

  bool xmlSaveState(QDomDocument& doc, QDomElement& parent_element) const override;
  bool xmlLoadState(const QDomElement& parent_element) override;

private slots:
  void onPoll();

private:
  struct ConnectResult
  {
    bool ok = false;
    bool demo_mode = false;
    QString port;
    int bitrate_arb = 500000;
    QString blf_path;
    bool record_blf = false;
    PJ::BLF::BlfPluginConfig blf_config;
  };

  ConnectResult runConnectDialog();
  void rxLoopHardware();
  void rxLoopDemo();
  void processFrames(const std::vector<jcan_vector::CanFrame>& frames);

  std::atomic<bool> _running{false};
  bool _demo_mode = false;
  QString _last_port;
  int _bitrate_arb = 500000;
  QString _last_blf_path;
  PJ::BLF::BlfPluginConfig _blf_config;

  std::unique_ptr<jcan_vector::VectorDevice> _device;
  std::unique_ptr<PJ::BLF::DbcManager> _dbc_manager;
  std::unique_ptr<PJ::BLF::BlfDecoderPipeline> _pipeline;
  std::unique_ptr<PJ::BLF::BlfRecorder> _recorder;
  std::unique_ptr<PJ::BLF::ISeriesWriter> _series_writer;

  std::thread _rx_thread;
  QTimer* _poll_timer = nullptr;

  std::mutex _queue_mutex;
  std::vector<jcan_vector::CanFrame> _queue;
  std::chrono::steady_clock::time_point _t0{};
  uint8_t _channel = 0;
};
