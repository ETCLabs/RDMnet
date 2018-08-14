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
