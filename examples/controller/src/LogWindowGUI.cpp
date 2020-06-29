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
#include "LogWindowGUI.h"

#include <sstream>

LogWindowGUI::LogWindowGUI(QWidget* parent, QString log_file_name, bool has_error) : QDialog(parent)
{
  ui.setupUi(this);

  if (has_error)
  {
    ui.fileWarningLabel->setStyleSheet("QLabel { color: red; }");
    ui.fileWarningLabel->setText(tr("Warning: Could not open log file %1").arg(log_file_name));
  }

  connect(this, &LogWindowGUI::appendText, this, &LogWindowGUI::processAppendText, Qt::AutoConnection);
  connect(this, &LogWindowGUI::clearText, this, &LogWindowGUI::processClearText, Qt::AutoConnection);

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

LogOutputStream& LogWindowGUI::operator<<(const std::string& str)
{
  QString qstr(str.data());

  emit appendText(qstr);

  return (*this);
}

void LogWindowGUI::clear()
{
  emit clearText();
}

void LogWindowGUI::processAppendText(const QString& text)
{
  ui.outputTextEdit->moveCursor(QTextCursor::End);
  ui.outputTextEdit->insertPlainText(text);
  ui.outputTextEdit->moveCursor(QTextCursor::End);
}

void LogWindowGUI::processClearText()
{
  ui.outputTextEdit->setText("");
}
