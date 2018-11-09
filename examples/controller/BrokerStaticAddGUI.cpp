/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 63. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
* Copyright 2018 ETC Inc.
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

#include "BrokerStaticAddGUI.h"

#include <QMessageBox>
#include "lwpa/socket.h"

BrokerStaticAddGUI::BrokerStaticAddGUI(QWidget *parent, IHandlesBrokerStaticAdd *handler) : QDialog(parent)
{
  ui.setupUi(this);

  m_Handler = handler;

  ui.portEdit->setValidator(new QIntValidator(1, 65535, this));

  connect(ui.addBrokerButton, SIGNAL(clicked()), this, SLOT(addBrokerTriggered()));

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

  QString scopeQString = ui.scopeEdit->text();
  std::string scopeStdString = scopeQString.toStdString();

  // CIPAddr addr = CIPAddr::StringToAddr( addrString );

  // QStringList ipv6Split = ui.ipEdit->text().split( "]" );
  // QString ipEndString = ipv6Split.value( ipv6Split.length() - 1 );

  QMessageBox errorMessageBox;
  errorMessageBox.setIcon(QMessageBox::Icon::Critical);

  LwpaSockaddr brokerAddr;
  QByteArray ipBuf = ui.ipEdit->text().toUtf8();
  const char *ipStr = ipBuf.constData();

  if ((LWPA_OK != lwpa_inet_pton(LWPA_IPV4, ipStr, &brokerAddr.ip) &&
       LWPA_OK != lwpa_inet_pton(LWPA_IPV6, ipStr, &brokerAddr.ip))
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
  else if (scopeStdString.empty())
  {
    errorMessageBox.setText(tr("Invalid scope. Please use a correct input format."));
    errorMessageBox.exec();
  }
  else if (m_Handler != Q_NULLPTR)
  {
    close();
    brokerAddr.port = static_cast<uint16_t>(ui.portEdit->text().toInt());
    m_Handler->handleAddBrokerByIP(scopeStdString, brokerAddr);
  }
}
