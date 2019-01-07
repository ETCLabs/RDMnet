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

#pragma once

#include "ControllerUtils.h"

BEGIN_INCLUDE_QT_HEADERS()
#include <QDialog>
#include "ui_BrokerStaticAddGUI.h"
END_INCLUDE_QT_HEADERS()

#include "lwpa/inet.h"

class IHandlesBrokerStaticAdd
{
public:
  virtual void handleAddBrokerByIP(std::string scope, const LwpaSockaddr &addr) = 0;
};

class BrokerStaticAddGUI : public QDialog
{
  Q_OBJECT

public slots:

  void addBrokerTriggered();

signals:

  void addBrokerByIP(LwpaSockaddr addr);

public:
  BrokerStaticAddGUI(QWidget *parent = Q_NULLPTR, IHandlesBrokerStaticAdd *handler = Q_NULLPTR);
  ~BrokerStaticAddGUI();

private:
  Ui::BrokerStaticAddGUI ui;

  IHandlesBrokerStaticAdd *m_Handler;
};
