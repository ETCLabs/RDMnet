#pragma once

#include "ControllerUtils.h"

BEGIN_INCLUDE_QT_HEADERS()
#include <QPushButton>
#include <QPersistentModelIndex>
END_INCLUDE_QT_HEADERS()

class PropertyPushButton : public QPushButton
{
  Q_OBJECT

public:
  PropertyPushButton(QWidget *parent, const QPersistentModelIndex &propertyIndex);

signals:
  void clicked(const QPersistentModelIndex &propertyIndex);

private slots:
  void forwardClicked();

private:
  QPersistentModelIndex idx;
};
