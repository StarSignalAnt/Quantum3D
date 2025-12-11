#pragma once

#include <QtWidgets/QToolBar>

class QuantumToolBar : public QToolBar {
  Q_OBJECT

public:
  QuantumToolBar(QWidget *parent = nullptr);
  ~QuantumToolBar();

private:
  void setupToolBar();
};
