#include "datastream_vector_jcan.h"

#include <QCheckBox>
#include <QDateTime>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <set>

#include "dbcppp_decoder.h"
#include "jcan_vector/discovery.hpp"

using namespace PJ;

namespace
{

uint8_t PayloadLen(const jcan_vector::CanFrame& f)
{
  static constexpr uint8_t map[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64 };
  if (f.fd)
  {
    return (f.dlc < 16) ? map[f.dlc] : 64;
  }
  return std::min<uint8_t>(f.dlc, 8);
}

QString UsbPathFromPort(const QString& port)
{
  const int colon = port.lastIndexOf(':');
  if (colon < 0) return port;
  return port.left(colon);
}

QString ChannelsToCsv(const std::vector<uint8_t>& channels)
{
  QStringList parts;
  for (uint8_t ch : channels)
  {
    parts << QString::number(ch);
  }
  return parts.join(',');
}

std::vector<uint8_t> ChannelsFromCsv(const QString& csv)
{
  std::vector<uint8_t> out;
  for (const QString& part : csv.split(',', PJ::SkipEmptyParts))
  {
    bool ok = false;
    const int v = part.trimmed().toInt(&ok);
    if (ok && v >= 0 && v <= 255)
    {
      out.push_back(static_cast<uint8_t>(v));
    }
  }
  return out;
}

class PlotMapSeriesWriter : public BLF::ISeriesWriter
{
public:
  explicit PlotMapSeriesWriter(DataStreamer* owner) : owner_(owner) {}

  void WriteSample(const std::string& series_name, double timestamp, double value) override
  {
    auto& series = owner_->dataMap().getOrCreateNumeric(series_name);
    series.pushBack({ timestamp, value });
  }

private:
  DataStreamer* owner_ = nullptr;
};

class ConnectDialog : public QDialog
{
public:
  QComboBox* device_combo = nullptr;
  QSpinBox* bitrate_spin = nullptr;
  QCheckBox* demo_check = nullptr;
  QCheckBox* auto_reconnect_check = nullptr;
  QCheckBox* raw_check = nullptr;
  QCheckBox* decoded_check = nullptr;
  QCheckBox* record_check = nullptr;
  QLineEdit* dbc_edit = nullptr;
  QLineEdit* blf_edit = nullptr;
  QCheckBox* channel_checks[4] = {};
  std::vector<jcan_vector::DeviceInfo> devices;       // unique usb devices (ch0 entry)
  std::vector<jcan_vector::DeviceInfo> all_channels;  // full discovery list

  explicit ConnectDialog(QWidget* parent = nullptr) : QDialog(parent)
  {
    setWindowTitle(tr("Vector VN1640A (jcan)"));
    resize(540, 400);
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    device_combo = new QComboBox(this);
    bitrate_spin = new QSpinBox(this);
    bitrate_spin->setRange(10000, 1000000);
    bitrate_spin->setValue(500000);
    bitrate_spin->setSuffix(" bit/s");

    demo_check = new QCheckBox(tr("Demo mode (synthetic frames, no hardware)"), this);
    auto_reconnect_check = new QCheckBox(tr("Auto-reconnect on disconnect"), this);
    auto_reconnect_check->setChecked(true);
    raw_check = new QCheckBox(tr("Emit raw byte series"), this);
    decoded_check = new QCheckBox(tr("Emit DBC-decoded signals"), this);
    decoded_check->setChecked(true);
    record_check = new QCheckBox(tr("Record BLF"), this);

    auto* ch_row = new QHBoxLayout();
    for (int i = 0; i < 4; ++i)
    {
      channel_checks[i] = new QCheckBox(tr("ch%1").arg(i), this);
      if (i == 0) channel_checks[i]->setChecked(true);
      ch_row->addWidget(channel_checks[i]);
    }
    ch_row->addStretch(1);

    dbc_edit = new QLineEdit(this);
    auto* dbc_row = new QHBoxLayout();
    dbc_row->addWidget(dbc_edit);
    auto* dbc_btn = new QPushButton(tr("Browse…"), this);
    dbc_row->addWidget(dbc_btn);
    connect(dbc_btn, &QPushButton::clicked, this, [this]() {
      const auto path = QFileDialog::getOpenFileName(this, tr("Select DBC"), QString(),
                                                     tr("DBC files (*.dbc);;All (*)"));
      if (!path.isEmpty()) dbc_edit->setText(path);
    });

    blf_edit = new QLineEdit(this);
    auto* blf_row = new QHBoxLayout();
    blf_row->addWidget(blf_edit);
    auto* blf_btn = new QPushButton(tr("Browse…"), this);
    blf_row->addWidget(blf_btn);
    connect(blf_btn, &QPushButton::clicked, this, [this]() {
      const auto path = QFileDialog::getSaveFileName(this, tr("BLF output"), QString("capture.blf"),
                                                     tr("BLF files (*.blf);;All (*)"));
      if (!path.isEmpty()) blf_edit->setText(path);
    });

    form->addRow(tr("Device"), device_combo);
    form->addRow(tr("Channels"), ch_row);
    form->addRow(tr("Arbitration bitrate"), bitrate_spin);
    form->addRow(QString(), demo_check);
    form->addRow(QString(), auto_reconnect_check);
    form->addRow(tr("DBC (all selected channels)"), dbc_row);
    form->addRow(QString(), raw_check);
    form->addRow(QString(), decoded_check);
    form->addRow(QString(), record_check);
    form->addRow(tr("BLF path"), blf_row);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    refreshDevices();
    connect(device_combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) { updateChannelAvailability(); });
    connect(demo_check, &QCheckBox::toggled, this, [this](bool on) {
      device_combo->setEnabled(!on);
      bitrate_spin->setEnabled(!on);
      auto_reconnect_check->setEnabled(!on);
      updateChannelAvailability();
    });
  }

  void refreshDevices()
  {
    device_combo->clear();
    devices.clear();
    all_channels = jcan_vector::list_vector_devices();
    std::set<std::string> seen;
    for (const auto& d : all_channels)
    {
      if (!seen.insert(d.usb_path).second) continue;
      devices.push_back(d);
      QString nice = QString::fromStdString(d.friendly_name);
      const int ch_pos = nice.indexOf(" ch");
      if (ch_pos > 0) nice = nice.left(ch_pos);
      device_combo->addItem(QString("%1 (%2) — %3 ch")
                                .arg(nice)
                                .arg(QString::fromStdString(d.usb_path))
                                .arg(d.num_channels),
                            QString::fromStdString(d.usb_path));
    }
    if (devices.empty())
    {
      device_combo->addItem(tr("(no Vector USB device found)"), QString());
    }
    updateChannelAvailability();
  }

  void updateChannelAvailability()
  {
    int num = 4;
    if (!demo_check->isChecked())
    {
      const QString usb = device_combo->currentData().toString();
      num = 1;
      for (const auto& d : devices)
      {
        if (QString::fromStdString(d.usb_path) == usb)
        {
          num = d.num_channels;
          break;
        }
      }
    }
    for (int i = 0; i < 4; ++i)
    {
      const bool enable = (i < num);
      channel_checks[i]->setEnabled(enable);
      if (!enable) channel_checks[i]->setChecked(false);
      if (enable && i == 0 && !anyChannelChecked()) channel_checks[0]->setChecked(true);
    }
  }

  bool anyChannelChecked() const
  {
    for (auto* cb : channel_checks)
    {
      if (cb && cb->isChecked()) return true;
    }
    return false;
  }

  std::vector<uint8_t> selectedChannels() const
  {
    std::vector<uint8_t> out;
    for (int i = 0; i < 4; ++i)
    {
      if (channel_checks[i] && channel_checks[i]->isEnabled() && channel_checks[i]->isChecked())
      {
        out.push_back(static_cast<uint8_t>(i));
      }
    }
    if (out.empty()) out.push_back(0);
    return out;
  }

  void setSelectedChannels(const std::vector<uint8_t>& channels)
  {
    for (int i = 0; i < 4; ++i) channel_checks[i]->setChecked(false);
    for (uint8_t ch : channels)
    {
      if (ch < 4) channel_checks[ch]->setChecked(true);
    }
    if (!anyChannelChecked()) channel_checks[0]->setChecked(true);
  }

  QString selectedUsbPath() const { return device_combo->currentData().toString(); }

  /** Port for first selected channel on the chosen device. */
  QString selectedPort() const
  {
    const auto chs = selectedChannels();
    const uint8_t primary = chs.empty() ? 0 : chs.front();
    const QString usb = selectedUsbPath();
    if (usb.isEmpty()) return QString();
    return QString("%1:%2").arg(usb).arg(primary);
  }
};

}  // namespace

DataStreamVectorJcan::DataStreamVectorJcan()
{
  _poll_timer = new QTimer(this);
  _poll_timer->setInterval(20);
  connect(_poll_timer, &QTimer::timeout, this, &DataStreamVectorJcan::onPoll);

  _notification_action = new QAction(this);
  connect(_notification_action, &QAction::triggered, this, [this]() {
    const QString body =
        _notification_messages.isEmpty() ?
            tr("No notifications.") :
            _notification_messages.join("\n");
    QMessageBox::information(nullptr, tr("Vector VN1640A notifications"), body, QMessageBox::Ok);
    _notifications_count = 0;
    _notification_messages.clear();
    emit notificationsChanged(0);
  });
}

DataStreamVectorJcan::~DataStreamVectorJcan()
{
  shutdown();
}

void DataStreamVectorJcan::pushNotification(const QString& msg)
{
  QMetaObject::invokeMethod(
      this,
      [this, msg]() {
        _notification_messages.append(
            QString("[%1] %2")
                .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                .arg(msg));
        while (_notification_messages.size() > 20)
        {
          _notification_messages.removeFirst();
        }
        ++_notifications_count;
        emit notificationsChanged(_notifications_count);
      },
      Qt::QueuedConnection);
}

bool DataStreamVectorJcan::channelSelected(uint8_t ch) const
{
  return _channel_mask.find(ch) != _channel_mask.end();
}

DataStreamVectorJcan::ConnectResult DataStreamVectorJcan::runConnectDialog()
{
  ConnectResult out;
  ConnectDialog dialog;
  if (!_last_port.isEmpty())
  {
    const QString usb = UsbPathFromPort(_last_port);
    const int idx = dialog.device_combo->findData(usb);
    if (idx >= 0) dialog.device_combo->setCurrentIndex(idx);
  }
  dialog.bitrate_spin->setValue(_bitrate_arb);
  dialog.demo_check->setChecked(_demo_mode);
  dialog.auto_reconnect_check->setChecked(_auto_reconnect);
  dialog.setSelectedChannels(_channels);
  dialog.raw_check->setChecked(_blf_config.emit_raw);
  dialog.decoded_check->setChecked(_blf_config.emit_decoded);
  if (!_blf_config.dbc_files.empty())
  {
    dialog.dbc_edit->setText(QString::fromStdString(_blf_config.dbc_files.front()));
  }
  if (!_last_blf_path.isEmpty())
  {
    dialog.record_check->setChecked(true);
    dialog.blf_edit->setText(_last_blf_path);
  }
  else if (_record_blf_pref)
  {
    dialog.record_check->setChecked(true);
  }
  dialog.updateChannelAvailability();

  if (dialog.exec() != QDialog::Accepted)
  {
    return out;
  }

  out.demo_mode = dialog.demo_check->isChecked();
  out.auto_reconnect = dialog.auto_reconnect_check->isChecked();
  out.channels = dialog.selectedChannels();
  out.port = dialog.selectedPort();
  if (out.demo_mode && out.port.isEmpty())
  {
    // Demo does not need USB; synthesize a logical port for primary channel.
    out.port = QString("demo:%1").arg(out.channels.empty() ? 0 : out.channels.front());
  }
  out.bitrate_arb = dialog.bitrate_spin->value();
  out.record_blf = dialog.record_check->isChecked();
  out.blf_path = dialog.blf_edit->text();
  out.blf_config.emit_raw = dialog.raw_check->isChecked();
  out.blf_config.emit_decoded = dialog.decoded_check->isChecked();
  out.blf_config.use_source_timestamp = true;
  const QString dbc = dialog.dbc_edit->text().trimmed();
  if (!dbc.isEmpty())
  {
    out.blf_config.dbc_files = { dbc.toStdString() };
    for (uint8_t ch : out.channels)
    {
      out.blf_config.channel_to_dbc[ch] = dbc.toStdString();
    }
  }
  out.ok = true;
  return out;
}

bool DataStreamVectorJcan::start(QStringList*)
{
  if (_running)
  {
    return true;
  }

  const auto cfg = runConnectDialog();
  if (!cfg.ok)
  {
    return false;
  }

  if (!cfg.demo_mode && cfg.port.isEmpty())
  {
    QMessageBox::warning(nullptr, tr("Vector VN1640A"),
                         tr("No device selected. Enable Demo mode or attach VN1640A."));
    return false;
  }

  if (cfg.channels.empty())
  {
    QMessageBox::warning(nullptr, tr("Vector VN1640A"), tr("Select at least one channel."));
    return false;
  }

  if (cfg.blf_config.emit_decoded && cfg.blf_config.dbc_files.empty())
  {
    QMessageBox::warning(nullptr, tr("Vector VN1640A"),
                         tr("Decoded output is enabled but no DBC was selected."));
    return false;
  }

  _demo_mode = cfg.demo_mode;
  _auto_reconnect = cfg.auto_reconnect;
  _last_port = cfg.port;
  _bitrate_arb = cfg.bitrate_arb;
  _blf_config = cfg.blf_config;
  _last_blf_path = cfg.blf_path;
  _record_blf_pref = cfg.record_blf;
  _channels = cfg.channels;
  _channel_mask = std::unordered_set<uint8_t>(_channels.begin(), _channels.end());
  _primary_channel = _channels.front();
  _t0 = std::chrono::steady_clock::now();

  _notifications_count = 0;
  _notification_messages.clear();
  emit notificationsChanged(0);

  _series_writer = std::make_unique<PlotMapSeriesWriter>(this);
  _dbc_manager = std::make_unique<BLF::DbcManager>([](const std::string& path) {
    std::string err;
    auto dec = std::make_unique<BLF::DbcpppDecoder>(path, &err);
    if (!dec->IsValid())
    {
      return std::unique_ptr<BLF::IDbcDecoder>{};
    }
    return std::unique_ptr<BLF::IDbcDecoder>(std::move(dec));
  });

  if (!_blf_config.channel_to_dbc.empty())
  {
    std::vector<BLF::ChannelDbcBinding> bindings;
    for (const auto& kv : _blf_config.channel_to_dbc)
    {
      bindings.push_back(BLF::ChannelDbcBinding{ kv.first, kv.second });
    }
    std::string load_error;
    if (!_dbc_manager->LoadBindings(bindings, &load_error))
    {
      QMessageBox::warning(nullptr, tr("Vector VN1640A"),
                           tr("Failed to load DBC:\n%1").arg(QString::fromStdString(load_error)));
      return false;
    }
  }

  _pipeline = std::make_unique<BLF::BlfDecoderPipeline>(_blf_config, _dbc_manager.get(),
                                                        _series_writer.get());

  if (cfg.record_blf)
  {
    if (cfg.blf_path.isEmpty())
    {
      QMessageBox::warning(nullptr, tr("Vector VN1640A"), tr("BLF recording enabled but path empty."));
      return false;
    }
    _recorder = std::make_unique<BLF::BlfRecorder>();
    std::string err;
    if (!_recorder->open(cfg.blf_path.toStdString(), &err))
    {
      QMessageBox::warning(nullptr, tr("Vector VN1640A"),
                           tr("Failed to open BLF for write:\n%1").arg(QString::fromStdString(err)));
      return false;
    }
  }

  if (!_demo_mode)
  {
    _open_cfg = {};
    _open_cfg.port = cfg.port.toStdString();
    _open_cfg.bitrate_arb = static_cast<uint32_t>(cfg.bitrate_arb);
    _open_cfg.channels = cfg.channels;

    _device = std::make_unique<jcan_vector::VectorDevice>();
    const auto err = _device->open(_open_cfg);
    if (err != jcan_vector::Error::Ok)
    {
      QMessageBox::warning(nullptr, tr("Vector VN1640A"),
                           tr("Failed to open %1\n%2 (%3)")
                               .arg(cfg.port)
                               .arg(QString::fromUtf8(jcan_vector::to_string(err)))
                               .arg(QString::fromStdString(_device->last_error())));
      _device.reset();
      _recorder.reset();
      return false;
    }
  }

  {
    std::lock_guard<std::mutex> lock(_queue_mutex);
    _queue.clear();
  }

  _running = true;
  if (_demo_mode)
  {
    _rx_thread = std::thread([this]() { rxLoopDemo(); });
  }
  else
  {
    _rx_thread = std::thread([this]() { rxLoopHardware(); });
  }
  _poll_timer->start();
  return true;
}

void DataStreamVectorJcan::shutdown()
{
  const bool was_running = _running.exchange(false);
  if (_poll_timer)
  {
    _poll_timer->stop();
  }
  if (_rx_thread.joinable())
  {
    _rx_thread.join();
  }
  if (_device)
  {
    if (_device->is_open())
    {
      _device->close();
    }
    _device.reset();
  }
  if (_recorder)
  {
    _recorder->close();
    _recorder.reset();
  }
  _pipeline.reset();
  _dbc_manager.reset();
  _series_writer.reset();
  (void)was_running;
}

bool DataStreamVectorJcan::isRunning() const
{
  return _running;
}

void DataStreamVectorJcan::rxLoopHardware()
{
  int backoff_ms = 1000;
  while (_running)
  {
    if (!_device)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }

    if (!_device->is_open())
    {
      if (!_auto_reconnect)
      {
        pushNotification(tr("Device not open; auto-reconnect disabled."));
        // Keep looping until shutdown rather than crashing; user can stop stream.
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        continue;
      }

      const auto err = _device->open(_open_cfg);
      if (err == jcan_vector::Error::Ok)
      {
        pushNotification(tr("Reconnected to %1").arg(QString::fromStdString(_open_cfg.port)));
        backoff_ms = 1000;
        continue;
      }
      pushNotification(tr("Reconnect failed (%1): %2")
                           .arg(QString::fromUtf8(jcan_vector::to_string(err)))
                           .arg(QString::fromStdString(_device->last_error())));
      std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
      backoff_ms = std::min(backoff_ms + 1000, 5000);
      continue;
    }

    std::vector<jcan_vector::CanFrame> frames;
    const auto err = _device->recv_many(frames, 20);
    if (err == jcan_vector::Error::IoError || err == jcan_vector::Error::NotOpen ||
        err == jcan_vector::Error::Unavailable || !_device->is_open())
    {
      pushNotification(tr("Device disconnected (%1)")
                           .arg(QString::fromUtf8(jcan_vector::to_string(err))));
      if (_device->is_open())
      {
        _device->close();
      }
      backoff_ms = 1000;
      if (!_auto_reconnect)
      {
        continue;
      }
      // Fall through to reconnect path on next iteration.
      continue;
    }

    if (!frames.empty())
    {
      std::lock_guard<std::mutex> lock(_queue_mutex);
      _queue.insert(_queue.end(), frames.begin(), frames.end());
    }
  }
}

void DataStreamVectorJcan::rxLoopDemo()
{
  uint32_t counter = 0;
  size_t ch_index = 0;
  while (_running)
  {
    jcan_vector::CanFrame f;
    f.id = 0x100;
    f.extended = false;
    f.fd = true;
    f.brs = true;
    f.dlc = 8;
    f.data[0] = static_cast<uint8_t>((counter >> 8) & 0xFF);
    f.data[1] = static_cast<uint8_t>(counter & 0xFF);
    f.data[2] = static_cast<uint8_t>(counter % 100);
    if (!_channels.empty())
    {
      f.source = _channels[ch_index % _channels.size()];
      ch_index++;
    }
    else
    {
      f.source = 0;
    }
    f.timestamp_sec =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - _t0).count();
    {
      std::lock_guard<std::mutex> lock(_queue_mutex);
      _queue.push_back(f);
    }
    ++counter;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

void DataStreamVectorJcan::onPoll()
{
  std::vector<jcan_vector::CanFrame> local;
  {
    std::lock_guard<std::mutex> lock(_queue_mutex);
    local.swap(_queue);
  }
  if (local.empty())
  {
    return;
  }
  processFrames(local);
}

void DataStreamVectorJcan::processFrames(const std::vector<jcan_vector::CanFrame>& frames)
{
  std::lock_guard<std::mutex> lock(mutex());
  for (const auto& f : frames)
  {
    const uint8_t ch = (f.source != 0xff) ? f.source : _primary_channel;
    if (!channelSelected(ch))
    {
      continue;
    }

    BLF::NormalizedCanFrame nf;
    nf.timestamp = f.timestamp_sec;
    if (nf.timestamp <= 0.0)
    {
      nf.timestamp =
          std::chrono::duration<double>(std::chrono::steady_clock::now() - _t0).count();
    }
    nf.channel = ch;
    nf.id = f.id;
    nf.is_fd = f.fd;
    nf.is_brs = f.brs;
    nf.is_esi = false;
    nf.extended = f.extended;
    nf.dlc = f.dlc;
    nf.size = PayloadLen(f);
    std::memcpy(nf.data.data(), f.data.data(), nf.size);

    if (_pipeline)
    {
      _pipeline->ProcessFrame(nf);
    }
    if (_recorder && _recorder->is_open())
    {
      _recorder->write(nf);
    }
  }
  emit dataReceived();
}

bool DataStreamVectorJcan::xmlSaveState(QDomDocument& doc, QDomElement& parent_element) const
{
  auto el = doc.createElement("vector_jcan");
  el.setAttribute("port", _last_port);
  el.setAttribute("bitrate_arb", _bitrate_arb);
  el.setAttribute("blf_path", _last_blf_path);
  el.setAttribute("demo_mode", _demo_mode ? 1 : 0);
  el.setAttribute("auto_reconnect", _auto_reconnect ? 1 : 0);
  el.setAttribute("channels", ChannelsToCsv(_channels));
  el.setAttribute("record_blf", _record_blf_pref ? 1 : 0);
  parent_element.appendChild(el);
  BLF::SaveConfigToXml(_blf_config, doc, parent_element);
  return true;
}

bool DataStreamVectorJcan::xmlLoadState(const QDomElement& parent_element)
{
  const auto el = parent_element.firstChildElement("vector_jcan");
  if (!el.isNull())
  {
    _last_port = el.attribute("port");
    _bitrate_arb = el.attribute("bitrate_arb", "500000").toInt();
    _last_blf_path = el.attribute("blf_path");
    _demo_mode = el.attribute("demo_mode", "0").toInt() != 0;
    _auto_reconnect = el.attribute("auto_reconnect", "1").toInt() != 0;
    _record_blf_pref = el.attribute("record_blf", "0").toInt() != 0;
    const auto loaded = ChannelsFromCsv(el.attribute("channels", "0"));
    if (!loaded.empty())
    {
      _channels = loaded;
      _channel_mask = std::unordered_set<uint8_t>(_channels.begin(), _channels.end());
      _primary_channel = _channels.front();
    }
  }
  BLF::LoadConfigFromXml(parent_element, _blf_config);
  return true;
}
