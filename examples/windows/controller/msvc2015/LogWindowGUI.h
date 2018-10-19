#pragma once

#include <QDialog>
#include "ui_LogWindowGUI.h"

#include "RDMnetNetworkModel.h"

class LogWindowGUI : public QDialog, public LogOutputStream
{
    Q_OBJECT

public:
    LogWindowGUI(QWidget *parent, RDMnetNetworkModel *model);
    ~LogWindowGUI();

    // LogOutputStream implementation
    virtual LogOutputStream &operator<<(const std::string &str) override;
    virtual void clear() override;

signals:
    void appendText(const QString &text);
    void clearText();

private slots:
    void processAppendText(const QString &text);
    void processClearText();

private:
    Ui::LogWindowGUI ui;
    RDMnetNetworkModel *model_;
};
