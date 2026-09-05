#include "datastream_vector_jcan.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSpinBox>
#include <QVBoxLayout>

#include "jcan_vector/discovery.hpp"
#include "jcan_vector/vector_device.hpp"

using namespace PJ;

namespace {

class ConnectDialog : public QDialog
{
public:
  QComboBox* device_combo = nullptr;
  QSpinBox* bitrate_spin = nullptr;
  std::vector<jcan_vector::DeviceInfo> devices;

  explicit ConnectDialog(QWidget* parent = nullptr) : QDialog(parent)
  {
    setWindowTitle(tr("Vector VN1640A (jcan)"));
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    device_combo = new QComboBox(this);
    bitrate_spin = new QSpinBox(this);
    bitrate_spin->setRange(10000, 1000000);
    bitrate_spin->setValue(500000);
    bitrate_spin->setSuffix(" bit/s");

    form->addRow(tr("Device / channel"), device_combo);
    form->addRow(tr("Arbitration bitrate"), bitrate_spin);
    layout->addLayout(form);
    layout->addWidget(new QLabel(
        tr("P0 scaffold: opens device to verify USB path. Live DBC/BLF arrives in later phases."),
        this));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    refreshDevices();
  }

  void refreshDevices()
  {
    device_combo->clear();
    devices = jcan_vector::list_vector_devices();
    for (const auto& d : devices) {
      device_combo->addItem(QString::fromStdString(d.friendly_name),
                            QString::fromStdString(d.port));
    }
    if (devices.empty()) {
      device_combo->addItem(tr("(no Vector USB device found)"), QString());
    }
  }

  QString selectedPort() const { return device_combo->currentData().toString(); }
};

}  // namespace

DataStreamVectorJcan::DataStreamVectorJcan() = default;

DataStreamVectorJcan::~DataStreamVectorJcan()
{
  shutdown();
}

bool DataStreamVectorJcan::start(QStringList*)
{
  if (_running) {
    return true;
  }

  ConnectDialog dialog;
  if (!_last_port.isEmpty()) {
    const int idx = dialog.device_combo->findData(_last_port);
    if (idx >= 0) dialog.device_combo->setCurrentIndex(idx);
  }
  dialog.bitrate_spin->setValue(_bitrate_arb);

  if (dialog.exec() != QDialog::Accepted) {
    return false;
  }

  const QString port = dialog.selectedPort();
  if (port.isEmpty()) {
    QMessageBox::warning(nullptr, tr("Vector VN1640A"),
                         tr("No Vector device selected. Plug in VN1640A and check udev permissions."));
    return false;
  }

  jcan_vector::VectorDevice device;
  jcan_vector::OpenConfig cfg;
  cfg.port = port.toStdString();
  cfg.bitrate_arb = static_cast<uint32_t>(dialog.bitrate_spin->value());
  const auto err = device.open(cfg);
  if (err != jcan_vector::Error::Ok) {
    QMessageBox::warning(
        nullptr, tr("Vector VN1640A"),
        tr("Failed to open %1\n%2 (%3)")
            .arg(port)
            .arg(QString::fromUtf8(jcan_vector::to_string(err)))
            .arg(QString::fromStdString(device.last_error())));
    return false;
  }

  // P0: verify open then close. Streaming loop arrives in P1.
  device.close();

  _last_port = port;
  _bitrate_arb = dialog.bitrate_spin->value();
  _running = true;

  {
    std::lock_guard<std::mutex> lock(mutex());
    auto& series = dataMap().getOrCreateNumeric("vector_jcan/status/opened");
    series.pushBack({ 0.0, 1.0 });
  }
  emit dataReceived();

  QMessageBox::information(
      nullptr, tr("Vector VN1640A"),
      tr("P0: device opened and closed successfully on %1.\n"
         "Realtime streaming + DBC + BLF recording will land in P1/P2.")
          .arg(port));

  return true;
}

void DataStreamVectorJcan::shutdown()
{
  _running = false;
}

bool DataStreamVectorJcan::isRunning() const
{
  return _running;
}

bool DataStreamVectorJcan::xmlSaveState(QDomDocument& doc, QDomElement& parent_element) const
{
  auto el = doc.createElement("vector_jcan");
  el.setAttribute("port", _last_port);
  el.setAttribute("bitrate_arb", _bitrate_arb);
  parent_element.appendChild(el);
  return true;
}

bool DataStreamVectorJcan::xmlLoadState(const QDomElement& parent_element)
{
  const auto el = parent_element.firstChildElement("vector_jcan");
  if (!el.isNull()) {
    _last_port = el.attribute("port");
    _bitrate_arb = el.attribute("bitrate_arb", "500000").toInt();
  }
  return true;
}
