#pragma once

#include <QDialog>
#include "ui_AboutGUI.h"

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
