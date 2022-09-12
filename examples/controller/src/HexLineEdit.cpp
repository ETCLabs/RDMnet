// Copyright (c) 2017 Electronic Theatre Controls, Inc., http://www.etcconnect.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "hexlineedit.h"
#include <QKeyEvent>

static QList<int> ACCEPTED_KEYS = {Qt::Key_0, Qt::Key_1,      Qt::Key_2,         Qt::Key_3,    Qt::Key_4,
                                   Qt::Key_5, Qt::Key_6,      Qt::Key_7,         Qt::Key_8,    Qt::Key_9,
                                   Qt::Key_A, Qt::Key_B,      Qt::Key_C,         Qt::Key_D,    Qt::Key_E,
                                   Qt::Key_F, Qt::Key_Delete, Qt::Key_Backspace, Qt::Key_Space};

HexLineEdit::HexLineEdit(QWidget* parent) : QLineEdit(parent)
{
  m_valid = true;
}

void HexLineEdit::keyPressEvent(QKeyEvent* event)
{
  if (!ACCEPTED_KEYS.contains(event->key()))
  {
    event->setAccepted(false);
    return;
  }
  QLineEdit::keyPressEvent(event);
  if (event->key() != Qt::Key_Backspace && event->key() != Qt::Key_Delete)
    setText(fixupHex(text()));
}

QString HexLineEdit::fixupHex(const QString& input)
{
  QString hex = input.toUpper();
  QString result;
  hex = hex.replace(QString(" "), QString());
  for (int i = 0; i < hex.length(); i++)
  {
    result += hex.at(i);
    if (i % 2 && i < hex.length() - 1)
      result += QString(" ");
  }
  return result;
}

QByteArray HexLineEdit::currentValue()
{
  QByteArray result;
  QString    hex = text().replace(QString(" "), QString());
  for (int i = 0; i < hex.length(); i += 2)
  {
    QString value = hex.at(i);
    if (i == hex.length() - 1)
      break;
    else
      value.append(hex.at(i + 1));
    result.append(static_cast<char>(value.toInt(Q_NULLPTR, 16)));
  }
  return result;
}
