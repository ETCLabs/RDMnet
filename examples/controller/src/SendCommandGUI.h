#ifndef SENDCOMMANDGUI_H
#define SENDCOMMANDGUI_H

#include "ControllerUtils.h"
#include "rdm/message.h"

BEGIN_INCLUDE_QT_HEADERS()
#include <QDialog>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include "ui_SendCommandGUI.h"
END_INCLUDE_QT_HEADERS()

class RDMnetNetworkItem;
class RDMnetNetworkModel;

class SendCommandGUI : public QDialog
{
  Q_OBJECT
public:
  SendCommandGUI(QWidget* parent, RDMnetNetworkItem* item, RDMnetNetworkModel* networkModel);
private slots:
  void addDataRow();
  void removeDataRow();
  void sendCommand();
  void rawDataTypeComboChanged(int index);
  void commandComplete(uint8_t response, QByteArray data);

protected:
  virtual void resizeEvent(QResizeEvent* event) override;
  virtual void showEvent(QShowEvent* event) override;

private:
  Ui::SendCommandGUI  ui;
  RDMnetNetworkModel* model_;
  RDMnetNetworkItem*  item_;

  QSpinBox*  m_subDevice;
  QComboBox* m_commandType;
  QComboBox* m_parameterId;

  QHash<QObject*, int> m_comboToRow;
  QList<QComboBox*>    m_customPropCombo;
  QHash<int, QWidget*> m_customPropEdits;

  enum ROWS
  {
    ROW_COMMAND,
    ROW_PARAMETER,
    ROW_SUBDEVICE
  };

  void       addRdmCommands(QComboBox* combo);
  void       setupRawDataEditor(int datatype, int row);
  QByteArray composeCommand();
};

#endif  // SENDCOMMANDGUI_H
