/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 77. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
* Copyright 2019 ETC Inc.
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/
#include "LogWindowGUI.h"

#include <sstream>

LogWindowGUI::LogWindowGUI(QWidget *parent, RDMnetNetworkModel *model)
    : QDialog(parent)
    , model_(model)
{
    ui.setupUi(this);

    connect(this, SIGNAL(appendText(const QString &)), this, SLOT(processAppendText(const QString &)), Qt::AutoConnection);
    connect(this, SIGNAL(clearText()), this, SLOT(processClearText()), Qt::AutoConnection);

    if (model != NULL)
    {
      model->addCustomLogOutputStream(this);
    }

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

LogWindowGUI::~LogWindowGUI()
{
  if (model_ != NULL)
  {
    model_->removeCustomLogOutputStream(this);
  }
}

LogOutputStream & LogWindowGUI::operator<<(const std::string & str)
{
  QString qstr(str.data());

  emit appendText(qstr);

  return (*this);
}

void LogWindowGUI::clear()
{
  emit clearText();
}

void LogWindowGUI::processAppendText(const QString &text)
{
  ui.outputTextEdit->moveCursor(QTextCursor::End);
  ui.outputTextEdit->insertPlainText(text);
  ui.outputTextEdit->moveCursor(QTextCursor::End);
}

void LogWindowGUI::processClearText()
{
  ui.outputTextEdit->setText("");
}
