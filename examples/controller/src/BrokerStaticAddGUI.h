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

#pragma once

#include "ControllerUtils.h"

BEGIN_INCLUDE_QT_HEADERS()
#include <QDialog>
#include "ui_BrokerStaticAddGUI.h"
END_INCLUDE_QT_HEADERS()

#include "etcpal/cpp/inet.h"

class IHandlesBrokerStaticAdd
{
public:
  virtual void handleAddBrokerByIP(QString scope, const etcpal::SockAddr& addr) = 0;
};

class BrokerStaticAddGUI : public QDialog
{
  Q_OBJECT

public slots:

  void addBrokerTriggered();
  void cancelTriggered();

signals:

  void addBrokerByIP(etcpal::SockAddr addr);

public:
  BrokerStaticAddGUI(QWidget* parent = nullptr, IHandlesBrokerStaticAdd* handler = nullptr);
  ~BrokerStaticAddGUI();

private:
  Ui::BrokerStaticAddGUI ui;

  IHandlesBrokerStaticAdd* m_Handler;
};
