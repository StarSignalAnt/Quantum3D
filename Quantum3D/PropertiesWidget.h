#pragma once

#include <QtWidgets/QWidget>

class PropertiesWidget : public QWidget {
  Q_OBJECT

public:
  PropertiesWidget(QWidget *parent = nullptr);
  ~PropertiesWidget();

  QSize sizeHint() const override { return QSize(280, 400); }
};
