#include "SendCommandGUI.h"
#include "RDMnetNetworkItem.h"
#include "RDMnetNetworkModel.h"
#include "HexLineEdit.h"

#include "rdm/defs.h"
#include "etcpal/pack.h"

// RDM Data Types
enum RDM_DATATYPES
{
  RDMDATATYPE_UINT8,
  RDMDATATYPE_UINT16,
  RDMDATATYPE_STRING,
  RDMDATATYPE_HEX
};
const QStringList RDM_DATATYPE_DESCS = {"Unsigned Int 8bit", "Unsigned Int 16bit", "String", "Hex Bytes"};

QString nakReasonToString(const int reason)
{
  switch (reason)
  {
    case E120_NR_UNKNOWN_PID:
      return QString("E120_NR_UNKNOWN_PID");
    case E120_NR_FORMAT_ERROR:
      return QString("E120_NR_FORMAT_ERROR");
    case E120_NR_HARDWARE_FAULT:
      return QString("E120_NR_HARDWARE_FAULT");
    case E120_NR_PROXY_REJECT:
      return QString("E120_NR_PROXY_REJECT");
    case E120_NR_WRITE_PROTECT:
      return QString("E120_NR_WRITE_PROTECT");
    case E120_NR_UNSUPPORTED_COMMAND_CLASS:
      return QString("E120_NR_UNSUPPORTED_COMMAND_CLASS");
    case E120_NR_DATA_OUT_OF_RANGE:
      return QString("E120_NR_DATA_OUT_OF_RANGE");
    case E120_NR_BUFFER_FULL:
      return QString("E120_NR_BUFFER_FULL");
    case E120_NR_PACKET_SIZE_UNSUPPORTED:
      return QString("E120_NR_PACKET_SIZE_UNSUPPORTED");
    case E120_NR_SUB_DEVICE_OUT_OF_RANGE:
      return QString("E120_NR_SUB_DEVICE_OUT_OF_RANGE");
    case E120_NR_PROXY_BUFFER_FULL:
      return QString("E120_NR_PROXY_BUFFER_FULL");
    case E137_2_NR_ACTION_NOT_SUPPORTED:
      return QString("E137_2_NR_ACTION_NOT_SUPPORTED");
  }
  return QString("Unknown");
}

// Create a Wireshark-style prettified dump of a byte array
QString prettifyHex(const QByteArray& data)
{
  QString result;
  for (int line = 0; line < data.length(); line += 16)
  {
    QString text = QString("%1 ").arg(line, 4, 16, QChar('0')).toUpper();
    for (int i = 0; i < 16; i++)
      if (line + i < data.length())
        text.append(QString(" %1").arg(static_cast<unsigned char>(data[line + i]), 2, 16, QChar('0')).toUpper());

    // Fill out the last line
    if (text.length() < 52)
      text += QString(53 - text.length(), ' ');

    text.append("  ");

    for (int i = 0; i < 16; i++)
      if (line + i < data.length())
      {
        QChar c(data[line + i]);
        if (c.isPrint())
          text.append(QString("%1").arg(c));
        else
          text.append(QString("."));
      }

    text += "\r\n";
    result += text;
  }
  return result;
}

SendCommandGUI::SendCommandGUI(QWidget* parent, RDMnetNetworkItem* item, RDMnetNetworkModel* model) : QDialog{parent}
{
  ui.setupUi(this);
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  model_ = model;
  connect(model, &RDMnetNetworkModel::arbitraryCommandComplete, this, &SendCommandGUI::commandComplete,
          Qt::QueuedConnection);
  item_ = item;
  QString uid = QString::fromStdString(item->uid().ToString()).toUpper();
  setWindowTitle(tr("Send RDM Commands to %1").arg(uid));

  ui.sendCommandButton->setDescription(tr("Send the command described above to RDMnet device %1").arg(uid));

  connect(ui.addDataButton, &QPushButton::pressed, this, &SendCommandGUI::addDataRow);
  connect(ui.removeDataButton, &QPushButton::pressed, this, &SendCommandGUI::removeDataRow);
  connect(ui.sendCommandButton, &QCommandLinkButton::pressed, this, &SendCommandGUI::sendCommand);

  // Command Type
  m_commandType = new QComboBox(this);
  m_commandType->addItem(tr("GET"), QVariant(E120_GET_COMMAND));
  m_commandType->addItem(tr("SET"), QVariant(E120_SET_COMMAND));
  ui.sendCommandTable->setCellWidget(ROW_COMMAND, 1, m_commandType);

  // Parameter
  m_parameterId = new QComboBox(this);
  addRdmCommands(m_parameterId);
  ui.sendCommandTable->setCellWidget(ROW_PARAMETER, 1, m_parameterId);

  // Subdevice
  m_subDevice = new QSpinBox(this);
  m_subDevice->setMinimum(0);
  m_subDevice->setMaximum(512);
  m_subDevice->setValue(0);
  ui.sendCommandTable->setCellWidget(ROW_SUBDEVICE, 1, m_subDevice);
}

void SendCommandGUI::resizeEvent(QResizeEvent* event)
{
  QDialog::resizeEvent(event);
  ui.sendCommandTable->setColumnWidth(0, ui.sendCommandTable->width() / 2);
}

void SendCommandGUI::showEvent(QShowEvent* event)
{
  QDialog::showEvent(event);
  ui.sendCommandTable->setColumnWidth(0, ui.sendCommandTable->width() / 2);
}

void SendCommandGUI::addRdmCommands(QComboBox* combo)
{
  combo->addItem("E120_DISC_UNIQUE_BRANCH", QVariant(E120_DISC_UNIQUE_BRANCH));
  combo->addItem("E120_DISC_MUTE", QVariant(E120_DISC_MUTE));
  combo->addItem("E120_DISC_UN_MUTE", QVariant(E120_DISC_UN_MUTE));
  combo->addItem("E120_PROXIED_DEVICES", QVariant(E120_PROXIED_DEVICES));
  combo->addItem("E120_PROXIED_DEVICE_COUNT", QVariant(E120_PROXIED_DEVICE_COUNT));
  combo->addItem("E120_COMMS_STATUS", QVariant(E120_COMMS_STATUS));
  combo->addItem("E120_QUEUED_MESSAGE", QVariant(E120_QUEUED_MESSAGE));
  combo->addItem("E120_STATUS_MESSAGES", QVariant(E120_STATUS_MESSAGES));
  combo->addItem("E120_STATUS_ID_DESCRIPTION", QVariant(E120_STATUS_ID_DESCRIPTION));
  combo->addItem("E120_CLEAR_STATUS_ID", QVariant(E120_CLEAR_STATUS_ID));
  combo->addItem("E120_SUB_DEVICE_STATUS_REPORT_THRESHOLD", QVariant(E120_SUB_DEVICE_STATUS_REPORT_THRESHOLD));
  combo->addItem("E120_SUPPORTED_PARAMETERS", QVariant(E120_SUPPORTED_PARAMETERS));
  combo->addItem("E120_PARAMETER_DESCRIPTION", QVariant(E120_PARAMETER_DESCRIPTION));
  combo->addItem("E120_DEVICE_INFO", QVariant(E120_DEVICE_INFO));
  combo->addItem("E120_PRODUCT_DETAIL_ID_LIST", QVariant(E120_PRODUCT_DETAIL_ID_LIST));
  combo->addItem("E120_DEVICE_MODEL_DESCRIPTION", QVariant(E120_DEVICE_MODEL_DESCRIPTION));
  combo->addItem("E120_MANUFACTURER_LABEL", QVariant(E120_MANUFACTURER_LABEL));
  combo->addItem("E120_DEVICE_LABEL", QVariant(E120_DEVICE_LABEL));
  combo->addItem("E120_FACTORY_DEFAULTS", QVariant(E120_FACTORY_DEFAULTS));
  combo->addItem("E120_LANGUAGE_CAPABILITIES", QVariant(E120_LANGUAGE_CAPABILITIES));
  combo->addItem("E120_LANGUAGE", QVariant(E120_LANGUAGE));
  combo->addItem("E120_SOFTWARE_VERSION_LABEL", QVariant(E120_SOFTWARE_VERSION_LABEL));
  combo->addItem("E120_BOOT_SOFTWARE_VERSION_ID", QVariant(E120_BOOT_SOFTWARE_VERSION_ID));
  combo->addItem("E120_BOOT_SOFTWARE_VERSION_LABEL", QVariant(E120_BOOT_SOFTWARE_VERSION_LABEL));
  combo->addItem("E120_DMX_PERSONALITY", QVariant(E120_DMX_PERSONALITY));
  combo->addItem("E120_DMX_PERSONALITY_DESCRIPTION", QVariant(E120_DMX_PERSONALITY_DESCRIPTION));
  combo->addItem("E120_DMX_START_ADDRESS", QVariant(E120_DMX_START_ADDRESS));
  combo->addItem("E120_SLOT_INFO", QVariant(E120_SLOT_INFO));
  combo->addItem("E120_SLOT_DESCRIPTION", QVariant(E120_SLOT_DESCRIPTION));
  combo->addItem("E120_DEFAULT_SLOT_VALUE", QVariant(E120_DEFAULT_SLOT_VALUE));
  combo->addItem("E137_1_DMX_BLOCK_ADDRESS", QVariant(E137_1_DMX_BLOCK_ADDRESS));
  combo->addItem("E137_1_DMX_FAIL_MODE", QVariant(E137_1_DMX_FAIL_MODE));
  combo->addItem("E137_1_DMX_STARTUP_MODE", QVariant(E137_1_DMX_STARTUP_MODE));
  combo->addItem("E120_SENSOR_DEFINITION", QVariant(E120_SENSOR_DEFINITION));
  combo->addItem("E120_SENSOR_VALUE", QVariant(E120_SENSOR_VALUE));
  combo->addItem("E120_RECORD_SENSORS", QVariant(E120_RECORD_SENSORS));
  combo->addItem("E137_1_DIMMER_INFO", QVariant(E137_1_DIMMER_INFO));
  combo->addItem("E137_1_MINIMUM_LEVEL", QVariant(E137_1_MINIMUM_LEVEL));
  combo->addItem("E137_1_MAXIMUM_LEVEL", QVariant(E137_1_MAXIMUM_LEVEL));
  combo->addItem("E137_1_CURVE", QVariant(E137_1_CURVE));
  combo->addItem("E137_1_CURVE_DESCRIPTION", QVariant(E137_1_CURVE_DESCRIPTION));
  combo->addItem("E137_1_OUTPUT_RESPONSE_TIME", QVariant(E137_1_OUTPUT_RESPONSE_TIME));
  combo->addItem("E137_1_OUTPUT_RESPONSE_TIME_DESCRIPTION", QVariant(E137_1_OUTPUT_RESPONSE_TIME_DESCRIPTION));
  combo->addItem("E137_1_MODULATION_FREQUENCY", QVariant(E137_1_MODULATION_FREQUENCY));
  combo->addItem("E137_1_MODULATION_FREQUENCY_DESCRIPTION", QVariant(E137_1_MODULATION_FREQUENCY_DESCRIPTION));
  combo->addItem("E120_DEVICE_HOURS", QVariant(E120_DEVICE_HOURS));
  combo->addItem("E120_LAMP_HOURS", QVariant(E120_LAMP_HOURS));
  combo->addItem("E120_LAMP_STRIKES", QVariant(E120_LAMP_STRIKES));
  combo->addItem("E120_LAMP_STATE", QVariant(E120_LAMP_STATE));
  combo->addItem("E120_LAMP_ON_MODE", QVariant(E120_LAMP_ON_MODE));
  combo->addItem("E120_DEVICE_POWER_CYCLES", QVariant(E120_DEVICE_POWER_CYCLES));
  combo->addItem("E137_1_BURN_IN", QVariant(E137_1_BURN_IN));
  combo->addItem("E120_DISPLAY_INVERT", QVariant(E120_DISPLAY_INVERT));
  combo->addItem("E120_DISPLAY_LEVEL", QVariant(E120_DISPLAY_LEVEL));
  combo->addItem("E120_PAN_INVERT", QVariant(E120_PAN_INVERT));
  combo->addItem("E120_TILT_INVERT", QVariant(E120_TILT_INVERT));
  combo->addItem("E120_PAN_TILT_SWAP", QVariant(E120_PAN_TILT_SWAP));
  combo->addItem("E120_REAL_TIME_CLOCK", QVariant(E120_REAL_TIME_CLOCK));
  combo->addItem("E137_1_LOCK_PIN", QVariant(E137_1_LOCK_PIN));
  combo->addItem("E137_1_LOCK_STATE", QVariant(E137_1_LOCK_STATE));
  combo->addItem("E137_1_LOCK_STATE_DESCRIPTION", QVariant(E137_1_LOCK_STATE_DESCRIPTION));
  combo->addItem("E137_2_LIST_INTERFACES", QVariant(E137_2_LIST_INTERFACES));
  combo->addItem("E137_2_INTERFACE_LABEL", QVariant(E137_2_INTERFACE_LABEL));
  combo->addItem("E137_2_INTERFACE_HARDWARE_ADDRESS_TYPE1", QVariant(E137_2_INTERFACE_HARDWARE_ADDRESS_TYPE1));
  combo->addItem("E137_2_IPV4_DHCP_MODE", QVariant(E137_2_IPV4_DHCP_MODE));
  combo->addItem("E137_2_IPV4_ZEROCONF_MODE", QVariant(E137_2_IPV4_ZEROCONF_MODE));
  combo->addItem("E137_2_IPV4_CURRENT_ADDRESS", QVariant(E137_2_IPV4_CURRENT_ADDRESS));
  combo->addItem("E137_2_IPV4_STATIC_ADDRESS", QVariant(E137_2_IPV4_STATIC_ADDRESS));
  combo->addItem("E137_2_INTERFACE_RENEW_DHCP", QVariant(E137_2_INTERFACE_RENEW_DHCP));
  combo->addItem("E137_2_INTERFACE_RELEASE_DHCP", QVariant(E137_2_INTERFACE_RELEASE_DHCP));
  combo->addItem("E137_2_INTERFACE_APPLY_CONFIGURATION", QVariant(E137_2_INTERFACE_APPLY_CONFIGURATION));
  combo->addItem("E137_2_IPV4_DEFAULT_ROUTE", QVariant(E137_2_IPV4_DEFAULT_ROUTE));
  combo->addItem("E137_2_DNS_IPV4_NAME_SERVER", QVariant(E137_2_DNS_IPV4_NAME_SERVER));
  combo->addItem("E137_2_DNS_HOSTNAME", QVariant(E137_2_DNS_HOSTNAME));
  combo->addItem("E137_2_DNS_DOMAIN_NAME", QVariant(E137_2_DNS_DOMAIN_NAME));
  combo->addItem("E133_COMPONENT_SCOPE", QVariant(E133_COMPONENT_SCOPE));
  combo->addItem("E133_SEARCH_DOMAIN", QVariant(E133_SEARCH_DOMAIN));
  combo->addItem("E133_TCP_COMMS_STATUS", QVariant(E133_TCP_COMMS_STATUS));
  combo->addItem("E133_BROKER_STATUS", QVariant(E133_BROKER_STATUS));
  combo->addItem("E120_IDENTIFY_DEVICE", QVariant(E120_IDENTIFY_DEVICE));
  combo->addItem("E120_RESET_DEVICE", QVariant(E120_RESET_DEVICE));
  combo->addItem("E120_POWER_STATE", QVariant(E120_POWER_STATE));
  combo->addItem("E120_PERFORM_SELFTEST", QVariant(E120_PERFORM_SELFTEST));
  combo->addItem("E120_SELF_TEST_DESCRIPTION", QVariant(E120_SELF_TEST_DESCRIPTION));
  combo->addItem("E120_CAPTURE_PRESET", QVariant(E120_CAPTURE_PRESET));
  combo->addItem("E120_PRESET_PLAYBACK", QVariant(E120_PRESET_PLAYBACK));
  combo->addItem("E137_1_IDENTIFY_MODE", QVariant(E137_1_IDENTIFY_MODE));
  combo->addItem("E137_1_PRESET_INFO", QVariant(E137_1_PRESET_INFO));
  combo->addItem("E137_1_PRESET_STATUS", QVariant(E137_1_PRESET_STATUS));
  combo->addItem("E137_1_PRESET_MERGEMODE", QVariant(E137_1_PRESET_MERGEMODE));
  combo->addItem("E137_1_POWER_ON_SELF_TEST", QVariant(E137_1_POWER_ON_SELF_TEST));
}

void SendCommandGUI::addDataRow()
{
  ui.sendCommandTable->setRowCount(ui.sendCommandTable->rowCount() + 1);

  QWidget*     cellWidget = new QWidget(this);
  QHBoxLayout* layout = new QHBoxLayout();
  QLabel*      label = new QLabel(tr("Data, Type : "), cellWidget);
  QComboBox*   combo = new QComboBox(cellWidget);
  m_comboToRow[combo] = ui.sendCommandTable->rowCount() - 1;
  m_customPropCombo << combo;
  connect(combo, SIGNAL(currentIndexChanged(int)), this, SLOT(rawDataTypeComboChanged(int)));
  combo->addItems(RDM_DATATYPE_DESCS);
  layout->setContentsMargins(10, 0, 10, 0);

  layout->addWidget(label);
  layout->addWidget(combo);
  cellWidget->setLayout(layout);

  ui.sendCommandTable->setCellWidget(ui.sendCommandTable->rowCount() - 1, 0, cellWidget);
}

void SendCommandGUI::rawDataTypeComboChanged(int index)
{
  if (!m_comboToRow.contains(sender()))
    return;

  int row = m_comboToRow[sender()];

  setupRawDataEditor(index, row);
}

void SendCommandGUI::setupRawDataEditor(int datatype, int row)
{
  QWidget* editor = Q_NULLPTR;
  switch (datatype)
  {
    case RDMDATATYPE_UINT8: {
      QSpinBox* sb = new QSpinBox(this);
      sb->setMinimum(0);
      sb->setMaximum(0xFF);
      editor = sb;
    }
    break;
    case RDMDATATYPE_UINT16: {
      QSpinBox* sb = new QSpinBox(this);
      sb->setMinimum(0);
      sb->setMaximum(0xFFFF);
      editor = sb;
    }
    break;
    case RDMDATATYPE_STRING: {
      QLineEdit* le = new QLineEdit(this);
      le->setMaxLength(32);
      editor = le;
    }
    break;
    case RDMDATATYPE_HEX: {
      QLineEdit* le = new HexLineEdit(this);
      editor = le;
    }
  }

  if (editor)
  {
    ui.sendCommandTable->setCellWidget(row, 1, editor);

    m_customPropEdits[row] = editor;
  }
}

void SendCommandGUI::removeDataRow()
{
}

void SendCommandGUI::sendCommand()
{
  model_->sendArbitraryCommmand(item_, static_cast<uint8_t>(m_commandType->currentData().toInt()),
                                static_cast<uint16_t>(m_parameterId->currentData().toInt()), composeCommand());
  ui.rxTextEdit->clear();
}

void SendCommandGUI::commandComplete(uint8_t response, QByteArray rdmData)
{
  switch (response)
  {
    case kRdmResponseTypeAck:
      ui.rxTextEdit->appendPlainText(tr("Response ACK"));
      break;
    case kRdmResponseTypeAckOverflow:
      ui.rxTextEdit->appendPlainText(tr("Response ACK_OVERFLOW"));
      break;
    case kRdmResponseTypeNackReason: {
      int reason = etcpal_unpack_u16b(reinterpret_cast<uint8_t*>(rdmData.data()));
      ui.rxTextEdit->appendPlainText(tr("NACK with reason %1 (%2)").arg(nakReasonToString(reason)).arg(reason));
    }
    break;
    case kRdmResponseTypeAckTimer:
      ui.rxTextEdit->appendPlainText(tr("Response ACK_TIMER"));
      break;
  }

  if (rdmData.length() > 0)
  {
    ui.rxTextEdit->appendPlainText(tr("%1 bytes of data:\n").arg(rdmData.length()));
    ui.rxTextEdit->appendPlainText(prettifyHex(rdmData));
  }
}

QByteArray SendCommandGUI::composeCommand()
{
  QByteArray result;

  for (int i = 0; i < m_customPropCombo.count(); i++)
  {
    int row = i + 3;
    int type = m_customPropCombo[i]->currentIndex();
    switch (type)
    {
      case RDMDATATYPE_UINT8: {
        QSpinBox* sb = dynamic_cast<QSpinBox*>(m_customPropEdits[row]);
        quint8    value = 0xFF & sb->value();
        result.append(static_cast<char>(value));
      }
      break;
      case RDMDATATYPE_UINT16: {
        QSpinBox* sb = dynamic_cast<QSpinBox*>(m_customPropEdits[row]);
        quint8    value_high = 0xFF & (sb->value() >> 8);
        quint8    value_lo = 0xFF & sb->value();

        result.append(static_cast<char>(value_high));
        result.append(static_cast<char>(value_lo));
      }
      break;
      case RDMDATATYPE_STRING: {
        QLineEdit* le = dynamic_cast<QLineEdit*>(m_customPropEdits[row]);
        result.append(le->text().toLatin1());
      }
      break;
      case RDMDATATYPE_HEX: {
        HexLineEdit* hle = dynamic_cast<HexLineEdit*>(m_customPropEdits[row]);
        result.append(hle->currentValue());
      }
      break;
    }
  }

  return result;
}
