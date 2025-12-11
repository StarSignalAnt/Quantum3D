#pragma once

#include <QtWidgets/QWidget>

class BrowserWidget : public QWidget {
  Q_OBJECT

public:
  BrowserWidget(QWidget *parent = nullptr);
  ~BrowserWidget();

  QSize sizeHint() const override { return QSize(800, 280); }
};
