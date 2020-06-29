/******************************************************************************
 * Copyright 2020 ETC Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************
 * This file is a part of RDMnet. For more information, go to:
 * https://github.com/ETCLabs/RDMnet
 *****************************************************************************/
#pragma once

#include "ControllerUtils.h"
#include "ControllerLog.h"

BEGIN_INCLUDE_QT_HEADERS()
#include <QDialog>
#include "ui_LogWindowGUI.h"
END_INCLUDE_QT_HEADERS()

#include "RDMnetNetworkModel.h"

class LogWindowGUI : public QDialog, public LogOutputStream
{
  Q_OBJECT

public:
  LogWindowGUI(QWidget* parent, QString log_file_name, bool has_error = false);

  // LogOutputStream implementation
  virtual LogOutputStream& operator<<(const std::string& str) override;
  virtual void             clear() override;

signals:
  void appendText(const QString& text);
  void clearText();

private slots:
  void processAppendText(const QString& text);
  void processClearText();

private:
  Ui::LogWindowGUI ui;
};
