#include "datastream_vector_jcan.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <chrono>
#include <cstring>

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
  QCheckBox* raw_check = nullptr;
  QCheckBox* decoded_check = nullptr;
  QCheckBox* record_check = nullptr;
  QLineEdit* dbc_edit = nullptr;
  QLineEdit* blf_edit = nullptr;
  std::vector<jcan_vector::DeviceInfo> devices;

  explicit ConnectDialog(QWidget* parent = nullptr) : QDialog(parent)
  {
    setWindowTitle(tr("Vector VN1640A (jcan)"));
    resize(520, 320);
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    device_combo = new QComboBox(this);
    bitrate_spin = new QSpinBox(this);
    bitrate_spin->setRange(10000, 1000000);
    bitrate_spin->setValue(500000);
    bitrate_spin->setSuffix(" bit/s");

    demo_check = new QCheckBox(tr("Demo mode (synthetic frames, no hardware)"), this);
    raw_check = new QCheckBox(tr("Emit raw byte series"), this);
    decoded_check = new QCheckBox(tr("Emit DBC-decoded signals"), this);
    decoded_check->setChecked(true);
    record_check = new QCheckBox(tr("Record BLF"), this);

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

    form->addRow(tr("Device / channel"), device_combo);
    form->addRow(tr("Arbitration bitrate"), bitrate_spin);
    form->addRow(QString(), demo_check);
    form->addRow(tr("DBC (channel 0)"), dbc_row);
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
    connect(demo_check, &QCheckBox::toggled, this, [this](bool on) {
      device_combo->setEnabled(!on);
      bitrate_spin->setEnabled(!on);
    });
  }

  void refreshDevices()
  {
    device_combo->clear();
    devices = jcan_vector::list_vector_devices();
    for (const auto& d : devices)
    {
      device_combo->addItem(QString::fromStdString(d.friendly_name),
                            QString::fromStdString(d.port));
    }
    if (devices.empty())
    {
      device_combo->addItem(tr("(no Vector USB device found)"), QString());
    }
  }

  QString selectedPort() const { return device_combo->currentData().toString(); }
};

}  // namespace

DataStreamVectorJcan::DataStreamVectorJcan()
{
  _poll_timer = new QTimer(this);
  _poll_timer->setInterval(20);
  connect(_poll_timer, &QTimer::timeout, this, &DataStreamVectorJcan::onPoll);
}

DataStreamVectorJcan::~DataStreamVectorJcan()
{
  shutdown();
}

DataStreamVectorJcan::ConnectResult DataStreamVectorJcan::runConnectDialog()
{
  ConnectResult out;
  ConnectDialog dialog;
  if (!_last_port.isEmpty())
  {
    const int idx = dialog.device_combo->findData(_last_port);
    if (idx >= 0) dialog.device_combo->setCurrentIndex(idx);
  }
  dialog.bitrate_spin->setValue(_bitrate_arb);
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

  if (dialog.exec() != QDialog::Accepted)
  {
    return out;
  }

  out.demo_mode = dialog.demo_check->isChecked();
  out.port = dialog.selectedPort();
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
    out.blf_config.channel_to_dbc[0] = dbc.toStdString();
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

  if (cfg.blf_config.emit_decoded && cfg.blf_config.dbc_files.empty())
  {
    QMessageBox::warning(nullptr, tr("Vector VN1640A"),
                         tr("Decoded output is enabled but no DBC was selected."));
    return false;
  }

  _demo_mode = cfg.demo_mode;
  _last_port = cfg.port;
  _bitrate_arb = cfg.bitrate_arb;
  _blf_config = cfg.blf_config;
  _last_blf_path = cfg.blf_path;
  _channel = 0;
  _t0 = std::chrono::steady_clock::now();

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
    _device = std::make_unique<jcan_vector::VectorDevice>();
    jcan_vector::OpenConfig open_cfg;
    open_cfg.port = cfg.port.toStdString();
    open_cfg.bitrate_arb = static_cast<uint32_t>(cfg.bitrate_arb);
    const auto err = _device->open(open_cfg);
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
  if (was_running)
  {
    // leave series in place
  }
}

bool DataStreamVectorJcan::isRunning() const
{
  return _running;
}

void DataStreamVectorJcan::rxLoopHardware()
{
  while (_running)
  {
    std::vector<jcan_vector::CanFrame> frames;
    if (_device)
    {
      _device->recv_many(frames, 20);
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
    BLF::NormalizedCanFrame nf;
    nf.timestamp = f.timestamp_sec;
    if (nf.timestamp <= 0.0)
    {
      nf.timestamp =
          std::chrono::duration<double>(std::chrono::steady_clock::now() - _t0).count();
    }
    nf.channel = _channel;
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
  }
  BLF::LoadConfigFromXml(parent_element, _blf_config);
  return true;
}
