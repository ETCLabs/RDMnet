#include "AboutGUI.h"

AboutGUI::AboutGUI(QWidget *parent, QString qtVersion, QString rdmNetVersion)
    : QDialog(parent)
{
  QFont titleFont("Arial", 18, QFont::Bold);
  QFont versionFont("Arial", 14, QFont::Bold);

  ui.setupUi(this);

  ui.titleLabel->setFont(titleFont);
  ui.versionLabel->setText(ui.versionLabel->text() + rdmNetVersion);
  ui.versionLabel->setFont(versionFont);
  ui.qtVersionLabel->setText(ui.qtVersionLabel->text() + qtVersion);
  ui.lwpaVersionLabel->setText(ui.lwpaVersionLabel->text() + rdmNetVersion);

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
