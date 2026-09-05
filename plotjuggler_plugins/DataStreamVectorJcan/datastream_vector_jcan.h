#pragma once

#include <QDialog>
#include <QtPlugin>
#include <memory>

#include "PlotJuggler/datastreamer_base.h"

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

private:
  bool _running = false;
  QString _last_port;
  int _bitrate_arb = 500000;
};
