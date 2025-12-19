#pragma once

#include <QMouseEvent>
#include <QWheelEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QWidget>
#include <memory>
#include <unordered_set>
#include <vector>

// Forward declarations
namespace Quantum {
class SceneGraph;
class GraphNode;
} // namespace Quantum

// Represents a flattened node for display
struct TreeViewItem {
  Quantum::GraphNode *Node = nullptr;
  int Depth = 0; // Indentation level
  bool IsExpanded = true;
  bool IsVisible = true; // False if parent is collapsed
};

class SceneGraphWidget : public QWidget {
  Q_OBJECT

public:
  SceneGraphWidget(QWidget *parent = nullptr);
  ~SceneGraphWidget();

  QSize sizeHint() const override { return QSize(280, 400); }

  // Set the scene graph to display
  void SetGraph(std::shared_ptr<Quantum::SceneGraph> graph);

  // Refresh the tree view from the scene graph
  void RefreshTree();

  // Called when selection changes externally (e.g., from viewport click)
  // This will highlight the node and expand parents as needed
  void OnExternalSelectionChanged(Quantum::GraphNode *node);

signals:
  void NodeSelected(Quantum::GraphNode *node);

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

private:
  // Rebuild the flat list from the scene graph
  void RebuildFlatList();
  void RebuildFlatListRecursive(Quantum::GraphNode *node, int depth,
                                bool parentVisible);

  // Calculate total content height
  int GetTotalContentHeight() const;

  // Get item at y position (accounting for scroll)
  int GetItemIndexAtY(int y) const;

  // Update scrollbar range
  void UpdateScrollBar();

  // Check if a node is expanded
  bool IsNodeExpanded(Quantum::GraphNode *node) const;

  // Toggle node expanded state
  void ToggleNodeExpanded(Quantum::GraphNode *node);

  // Expand a node (remove from collapsed set)
  void ExpandNode(Quantum::GraphNode *node);

  // Expand all ancestors of a node to make it visible
  void ExpandParentsOf(Quantum::GraphNode *node);

  // Scroll to make a node visible
  void ScrollToNode(Quantum::GraphNode *node);

  // Scene graph reference
  std::shared_ptr<Quantum::SceneGraph> m_SceneGraph;

  // Flattened list for rendering
  std::vector<TreeViewItem> m_FlatList;

  // Track collapsed nodes (nodes not in this set are expanded by default)
  std::unordered_set<Quantum::GraphNode *> m_CollapsedNodes;

  // Visual settings
  int m_RowHeight = 22;
  int m_IndentWidth = 20;
  int m_ScrollOffset = 0;

  // Colors
  QColor m_DarkRowColor{45, 45, 48};
  QColor m_LightRowColor{37, 37, 38};
  QColor m_SelectedColor{0, 120, 215};
  QColor m_TextColor{220, 220, 220};
  QColor m_ExpandIconColor{180, 180, 180};

  // Selection
  Quantum::GraphNode *m_SelectedNode = nullptr;

  // Flag to prevent recursive selection updates
  bool m_UpdatingSelection = false;

  // Scrollbar
  QScrollBar *m_VerticalScrollBar = nullptr;

private slots:
  void OnScrollBarValueChanged(int value);
};
