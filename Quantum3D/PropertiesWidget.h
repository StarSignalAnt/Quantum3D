#pragma once

#include <QRect>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <functional>
#include <glm/vec3.hpp>
#include <string>
#include <vector>

namespace Quantum {
class GraphNode;
}
using GraphNode = Quantum::GraphNode;

enum class PropertyType { String, Float, Int, Vec3, Bool, Header };

struct PropertyField {
  std::string Name;
  PropertyType Type;
  QRect NameRect;
  QRect ValueRect;

  // Callbacks for data binding
  std::function<std::string()> GetString;
  std::function<void(const std::string &)> SetString;
  std::function<float()> GetFloat;
  std::function<void(float)> SetFloat;
  std::function<int()> GetInt;
  std::function<void(int)> SetInt;
  std::function<glm::vec3()> GetVec3;
  std::function<void(glm::vec3)> SetVec3;
  std::function<bool()> GetBool;
  std::function<void(bool)> SetBool;

  // UI State for editing
  bool IsEditing = false;
  std::string EditBuffer;
  int SelectionStart = 0;
  int SelectionEnd = 0;
  int CursorPos = 0;
  int ScrollX = 0;
  int EditingSubIndex = -1; // -1 for scalar, 0-2 for Vec3 components
  int SubScrollX[3] = {0, 0, 0};

  // Sub-rects for Vec3 components
  QRect SubRects[3];
};

class PropertiesWidget : public QWidget {
  Q_OBJECT

public:
  PropertiesWidget(QWidget *parent = nullptr);
  virtual ~PropertiesWidget();

public:
  void SetNode(GraphNode *node);
  void RefreshProperties();

protected:
  bool event(QEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void leaveEvent(QEvent *event) override;

private slots:
  void OnCursorTimer();

private:
  void AddHeader(const std::string &name);
  void ApplyChanges(PropertyField &field);
  int posToIndex(PropertyField &field, int x);
  void ensureCursorVisible(PropertyField &field);

  GraphNode *m_CurrentNode = nullptr;
  std::vector<PropertyField> m_Fields;

  QTimer *m_CursorTimer;
  bool m_CursorVisible = true;
  int m_HoverIdx = -1;
  int m_HoverSubIdx = -1;
};
