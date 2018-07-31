#include "PropertyPushButton.h"

PropertyPushButton::PropertyPushButton(QWidget *parent, const QPersistentModelIndex &propertyIndex)
    : QPushButton(parent), idx(propertyIndex)
{
  connect(this, SIGNAL(clicked()), this, SLOT(forwardClicked()));
}

void PropertyPushButton::forwardClicked()
{
  emit clicked(idx);
}
