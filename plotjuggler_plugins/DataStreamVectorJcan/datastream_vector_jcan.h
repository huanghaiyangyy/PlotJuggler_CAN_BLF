#pragma once

#include <QtPlugin>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

#include <QAction>
#include <QString>
#include <QStringList>
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

  std::pair<QAction*, int> notificationAction() override
  {
    return { _notification_action, _notifications_count };
  }

private slots:
  void onPoll();

private:
  struct ConnectResult
  {
    bool ok = false;
    bool demo_mode = false;
    bool auto_reconnect = true;
    QString port;
    std::vector<uint8_t> channels;
    int bitrate_arb = 500000;
    QString blf_path;
    bool record_blf = false;
    PJ::BLF::BlfPluginConfig blf_config;
  };

  ConnectResult runConnectDialog();
  void rxLoopHardware();
  void rxLoopDemo();
  void processFrames(const std::vector<jcan_vector::CanFrame>& frames);
  void pushNotification(const QString& msg);
  bool channelSelected(uint8_t ch) const;

  std::atomic<bool> _running{false};
  bool _demo_mode = false;
  bool _auto_reconnect = true;
  bool _record_blf_pref = false;
  QString _last_port;
  int _bitrate_arb = 500000;
  QString _last_blf_path;
  PJ::BLF::BlfPluginConfig _blf_config;
  std::vector<uint8_t> _channels{0};
  std::unordered_set<uint8_t> _channel_mask{0};
  uint8_t _primary_channel = 0;
  jcan_vector::OpenConfig _open_cfg;

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

  QAction* _notification_action = nullptr;
  int _notifications_count = 0;
  QStringList _notification_messages;
};
