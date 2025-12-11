#pragma once

#include <QtWidgets/QWidget>

class SceneGraphWidget : public QWidget {
  Q_OBJECT

public:
  SceneGraphWidget(QWidget *parent = nullptr);
  ~SceneGraphWidget();

  QSize sizeHint() const override { return QSize(280, 400); }
};
