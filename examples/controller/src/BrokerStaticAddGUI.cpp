/******************************************************************************
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
 ******************************************************************************
 * This file is a part of RDMnet. For more information, go to:
 * https://github.com/ETCLabs/RDMnet
 *****************************************************************************/

#include "BrokerStaticAddGUI.h"
#include "ControllerUtils.h"

BEGIN_INCLUDE_QT_HEADERS()
#include <QMessageBox>
END_INCLUDE_QT_HEADERS()

#include "etcpal/socket.h"

BrokerStaticAddGUI::BrokerStaticAddGUI(QWidget* parent, IHandlesBrokerStaticAdd* handler) : QDialog(parent)
{
  ui.setupUi(this);

  m_Handler = handler;

  ui.portEdit->setValidator(new QIntValidator(1, 65535, this));

  connect(ui.addBrokerButton, SIGNAL(clicked()), this, SLOT(addBrokerTriggered()));
  connect(ui.cancelButton, SIGNAL(clicked()), this, SLOT(cancelTriggered()));

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

BrokerStaticAddGUI::~BrokerStaticAddGUI()
{
}

void BrokerStaticAddGUI::addBrokerTriggered()
{
  // QString addrQString = QString( "%1:%2" ).arg( ui.ipEdit->text(), ui.portEdit->text());
  // std::string addrStdString = addrQString.toStdString();
  // const char *addrString = addrStdString.data();

  QString scopeString = ui.scopeEdit->text();

  // CIPAddr addr = CIPAddr::StringToAddr( addrString );

  // QStringList ipv6Split = ui.ipEdit->text().split( "]" );
  // QString ipEndString = ipv6Split.value( ipv6Split.length() - 1 );

  QMessageBox errorMessageBox;
  errorMessageBox.setIcon(QMessageBox::Icon::Critical);

  EtcPalSockaddr brokerAddr;
  QByteArray ipBuf = ui.ipEdit->text().toUtf8();
  const char* ipStr = ipBuf.constData();

  if ((kEtcPalErrOk != etcpal_inet_pton(kEtcPalIpTypeV4, ipStr, &brokerAddr.ip) &&
       kEtcPalErrOk != etcpal_inet_pton(kEtcPalIpTypeV6, ipStr, &brokerAddr.ip))
      // || ipEndString.contains( ":" ) || ipEndString.contains( "," )
  )
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
    brokerAddr.port = static_cast<uint16_t>(ui.portEdit->text().toInt());
    m_Handler->handleAddBrokerByIP(scopeString, brokerAddr);
  }
}

void BrokerStaticAddGUI::cancelTriggered()
{
  close();
}
