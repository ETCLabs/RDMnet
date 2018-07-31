#pragma once

#include <QPushButton>
#include <QPersistentModelIndex>

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
