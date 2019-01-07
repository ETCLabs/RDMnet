#pragma once

#include "ControllerUtils.h"

BEGIN_INCLUDE_QT_HEADERS()
#include <QDialog>
#include "ui_AboutGUI.h"
END_INCLUDE_QT_HEADERS()

class AboutGUI : public QDialog
{
  Q_OBJECT

public:
  AboutGUI(QWidget *parent, QString qtVersion, QString rdmNetVersion);
  ~AboutGUI();

protected slots:
  void okButtonClicked();

private:
  Ui::AboutGUI ui;
};
