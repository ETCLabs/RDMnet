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

#include "BrokerStaticAddGUI.h"
#include "ControllerUtils.h"

BEGIN_INCLUDE_QT_HEADERS()
#include <QMessageBox>
#include <QIntValidator>
END_INCLUDE_QT_HEADERS()

BrokerStaticAddGUI::BrokerStaticAddGUI(QWidget* parent, IHandlesBrokerStaticAdd* handler) : QDialog(parent)
{
  ui.setupUi(this);

  m_Handler = handler;

  ui.portEdit->setValidator(new QIntValidator(1, 65535, this));

  connect(ui.addBrokerButton, &QPushButton::clicked, this, &BrokerStaticAddGUI::addBrokerTriggered);
  connect(ui.cancelButton, &QPushButton::clicked, this, &BrokerStaticAddGUI::cancelTriggered);

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

BrokerStaticAddGUI::~BrokerStaticAddGUI()
{
}

void BrokerStaticAddGUI::addBrokerTriggered()
{
  QString scopeString = ui.scopeEdit->text();

  QMessageBox errorMessageBox;
  errorMessageBox.setIcon(QMessageBox::Icon::Critical);

  etcpal::SockAddr brokerAddr(etcpal::IpAddr::FromString(ui.ipEdit->text().toStdString()),
                              static_cast<uint16_t>(ui.portEdit->text().toInt()));

  if (!brokerAddr.IsValid())
  {
    errorMessageBox.setText(tr("Invalid address format. Please use a correct input format."));
    errorMessageBox.exec();
  }
  //  else if (ui.portEdit->validator()->validate(ui.portEdit->text(), tmp) !=
  //           QValidator::State::Acceptable)
  //  {
  //    errorMessageBox.setText(tr("Invalid port number. Please use a correct input format."));
  //    errorMessageBox.exec();
  //  }
  else if (scopeString.isEmpty())
  {
    errorMessageBox.setText(tr("Invalid scope. Please use a correct input format."));
    errorMessageBox.exec();
  }
  else if (m_Handler)
  {
    close();
    m_Handler->HandleAddBrokerByIP(scopeString, brokerAddr);
  }
}

void BrokerStaticAddGUI::cancelTriggered()
{
  close();
}
