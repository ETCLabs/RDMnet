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

#include "PropertyEditorsDelegate.h"
#include "RDMnetNetworkItem.h"
#include "PropertyPushButton.h"

#include <qstandarditemmodel.h>
#include <qcombobox.h>
#include <qapplication.h>

PropertyEditorsDelegate::PropertyEditorsDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
}

PropertyEditorsDelegate::~PropertyEditorsDelegate()
{
}

QWidget *PropertyEditorsDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                               const QModelIndex &index) const
{
  QWidget *result = NULL;
  EditorWidgetType editorType =
      static_cast<EditorWidgetType>(index.data(RDMnetNetworkItem::EditorWidgetTypeRole).toInt());

  if (editorType == EditorWidgetType::kComboBox)
  {
    QComboBox *editor = new QComboBox(parent);

    editor->setFrame(false);
    editor->setEditable(false);
    result = editor;
  }
  else if (editorType == EditorWidgetType::kButton)
  {
    PropertyPushButton *button = new PropertyPushButton(parent, QPersistentModelIndex(index));
    QObject *callbackObject = index.data(RDMnetNetworkItem::CallbackObjectRole).value<QObject *>();
    QString callbackSlotQString = index.data(RDMnetNetworkItem::CallbackSlotRole).toString();
    QByteArray local8Bit = callbackSlotQString.toLocal8Bit();
    const char *callbackSlot = local8Bit.constData();

    connect(button, SIGNAL(clicked(const QPersistentModelIndex &)), callbackObject, callbackSlot, Qt::AutoConnection);

    button->setEnabled(true);
    result = button;
  }
  else
  {
    result = QStyledItemDelegate::createEditor(parent, option, index);
  }

  return result;
}

void PropertyEditorsDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
  QComboBox *comboBox = dynamic_cast<QComboBox *>(editor);
  QPushButton *pushButton = dynamic_cast<QPushButton *>(editor);
  EditorWidgetType editorType =
      static_cast<EditorWidgetType>(index.data(RDMnetNetworkItem::EditorWidgetTypeRole).toInt());

  if ((comboBox != NULL) && (editorType == EditorWidgetType::kComboBox))
  {
    uint8_t personality = index.data(RDMnetNetworkItem::PersonalityNumberRole).toInt();
    QStringList descriptions = index.data(RDMnetNetworkItem::PersonalityDescriptionListRole).toStringList();

    if (personality > descriptions.length())
    {
      personality = descriptions.length();
    }

    if (personality == 0)
    {
      personality = 1;
    }

    comboBox->insertItems(0, descriptions);
    comboBox->setCurrentIndex(personality - 1);
  }
  else if ((pushButton != NULL) && (editorType == EditorWidgetType::kButton))
  {
    pushButton->setText(index.data().toString());
  }
  else
  {
    QStyledItemDelegate::setEditorData(editor, index);
  }
}

void PropertyEditorsDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
  QComboBox *comboBox = dynamic_cast<QComboBox *>(editor);
  EditorWidgetType editorType =
      static_cast<EditorWidgetType>(index.data(RDMnetNetworkItem::EditorWidgetTypeRole).toInt());

  if ((comboBox != NULL) && (editorType == EditorWidgetType::kComboBox))
  {
    QStringList descriptions = index.data(RDMnetNetworkItem::PersonalityDescriptionListRole).toStringList();
    uint16_t personality = comboBox->currentIndex() + 1;
    QString currentDescription;
    int currentIndex = comboBox->currentIndex();

    if ((currentIndex < descriptions.size()) && (currentIndex >= 0))
    {
      currentDescription = descriptions.at(comboBox->currentIndex());
    }
    else
    {
      currentDescription = tr("");
    }

    model->setData(index, currentDescription, Qt::EditRole);
    model->setData(index, personality, RDMnetNetworkItem::PersonalityNumberRole);
  }
  else
  {
    QStyledItemDelegate::setModelData(editor, model, index);
  }
}

void PropertyEditorsDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                                                   const QModelIndex & /*index*/) const
{
  editor->setGeometry(option.rect);
}

void PropertyEditorsDelegate::paint(QPainter * painter, const QStyleOptionViewItem & option, 
                                    const QModelIndex & index) const
{
  if (static_cast<EditorWidgetType>(index.data(RDMnetNetworkItem::EditorWidgetTypeRole).toInt()) == kButton)
  {
    QStyleOptionButton button;

    button.rect = option.rect;
    button.text = index.data().toString();
    button.state = QStyle::State_ReadOnly; // A slightly different look to indicate that it should
                                           // be double-clicked to actually open the editor.

    QApplication::style()->drawControl(QStyle::CE_PushButton, &button, painter);
  }
  else
  {
    QStyledItemDelegate::paint(painter, option, index);
  }
}

bool PropertyEditorsDelegate::editorEvent(QEvent * event, QAbstractItemModel * model, const QStyleOptionViewItem & option, const QModelIndex & index)
{
  if (static_cast<EditorWidgetType>(index.data(RDMnetNetworkItem::EditorWidgetTypeRole).toInt()) == kButton)
  {

  }
  else
  {
    return QStyledItemDelegate::editorEvent(event, model, option, index);
  }

  return false;
}
