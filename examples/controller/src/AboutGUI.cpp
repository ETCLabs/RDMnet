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
#include "AboutGUI.h"

AboutGUI::AboutGUI(QWidget* parent, QString qt_version, QString rdmnet_version, QString etcpal_version) : QDialog(parent)
{
  QFont titleFont("Arial", 18, QFont::Bold);
  QFont versionFont("Arial", 14, QFont::Bold);

  ui.setupUi(this);

  ui.titleLabel->setFont(titleFont);
  ui.versionLabel->setText(ui.versionLabel->text() + rdmnet_version);
  ui.versionLabel->setFont(versionFont);
  ui.qtVersionLabel->setText(ui.qtVersionLabel->text() + qt_version);
  ui.etcpalVersionLabel->setText(ui.etcpalVersionLabel->text() + etcpal_version);

  ui.repoLinkLabel->setText("<a href=\"https://github.com/ETCLabs/RDMnet/\">https://github.com/ETCLabs/RDMnet</a>");
  ui.repoLinkLabel->setTextFormat(Qt::RichText);
  ui.repoLinkLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
  ui.repoLinkLabel->setOpenExternalLinks(true);

  ui.etcLinkLabel->setText("<a href=\"http://www.etcconnect.com/\">http://www.etcconnect.com</a>");
  ui.etcLinkLabel->setTextFormat(Qt::RichText);
  ui.etcLinkLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
  ui.etcLinkLabel->setOpenExternalLinks(true);

  connect(ui.okButton, SIGNAL(clicked()), this, SLOT(okButtonClicked()));

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

void AboutGUI::okButtonClicked()
{
  done(0);
}

AboutGUI::~AboutGUI()
{
}
