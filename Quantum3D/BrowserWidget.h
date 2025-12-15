#pragma once

#include <QtWidgets/QWidget>
#include <stack>
#include <string>

class QListWidget;
class QListWidgetItem;
class QMouseEvent;

class BrowserWidget : public QWidget {
  Q_OBJECT

public:
  BrowserWidget(QWidget *parent = nullptr);
  ~BrowserWidget();

  void BrowsePath(const std::string &path);

  QSize sizeHint() const override { return QSize(800, 280); }

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

signals:
  void ModelImported(); // Emitted when a model is successfully imported

private:
  void OnItemDoubleClicked(QListWidgetItem *item);

  QListWidget *m_ListWidget;
  std::string m_CurrentPath;
  std::stack<std::string> m_History;
};
