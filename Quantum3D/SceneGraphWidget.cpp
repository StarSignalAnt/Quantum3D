#include "SceneGraphWidget.h"
#include "EngineGlobals.h"
#include "stdafx.h"

#include <QApplication>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

// Include Quantum headers
#include "../QuantumEngine/GraphNode.h"
#include "../QuantumEngine/QLangDomain.h"
#include "../QuantumEngine/SceneGraph.h"

SceneGraphWidget::SceneGraphWidget(QWidget *parent) : QWidget(parent) {
  // Create vertical scrollbar
  m_VerticalScrollBar = new QScrollBar(Qt::Vertical, this);
  m_VerticalScrollBar->setVisible(false);
  connect(m_VerticalScrollBar, &QScrollBar::valueChanged, this,
          &SceneGraphWidget::OnScrollBarValueChanged);

  // Set background
  setAutoFillBackground(true);
  QPalette pal = palette();
  pal.setColor(QPalette::Window, m_DarkRowColor);
  setPalette(pal);

  // Enable mouse tracking for hover effects (future)
  setMouseTracking(true);

  // Enable drop support for scripts
  setAcceptDrops(true);
}

SceneGraphWidget::~SceneGraphWidget() {}

void SceneGraphWidget::SetGraph(std::shared_ptr<Quantum::SceneGraph> graph) {
  m_SceneGraph = graph;
  m_CollapsedNodes.clear(); // Reset collapsed state for new graph
  RefreshTree();
}

void SceneGraphWidget::RefreshTree() {
  RebuildFlatList();
  UpdateScrollBar();
  update(); // Trigger repaint
}

void SceneGraphWidget::OnExternalSelectionChanged(Quantum::GraphNode *node) {
  // Prevent recursive updates
  if (m_UpdatingSelection) {
    return;
  }

  m_SelectedNode = node;

  if (node) {
    // Expand all parent nodes to make this node visible
    ExpandParentsOf(node);

    // Rebuild to reflect expanded state
    RebuildFlatList();
    UpdateScrollBar();

    // Scroll to make the node visible
    ScrollToNode(node);
  }

  update();
}

void SceneGraphWidget::ExpandParentsOf(Quantum::GraphNode *node) {
  if (!node) {
    return;
  }

  // Walk up the parent chain and expand each parent
  Quantum::GraphNode *parent = node->GetParent();
  while (parent) {
    ExpandNode(parent);
    parent = parent->GetParent();
  }
}

void SceneGraphWidget::ExpandNode(Quantum::GraphNode *node) {
  if (node) {
    m_CollapsedNodes.erase(node);
  }
}

void SceneGraphWidget::ScrollToNode(Quantum::GraphNode *node) {
  if (!node) {
    return;
  }

  // Find the node in the flat list and calculate its Y position
  int visibleIndex = 0;
  for (const auto &item : m_FlatList) {
    if (!item.IsVisible) {
      continue;
    }

    if (item.Node == node) {
      int nodeY = visibleIndex * m_RowHeight;
      int viewportHeight = height();

      // Check if node is above visible area
      if (nodeY < m_ScrollOffset) {
        m_ScrollOffset = nodeY;
        if (m_VerticalScrollBar->isVisible()) {
          m_VerticalScrollBar->setValue(m_ScrollOffset);
        }
      }
      // Check if node is below visible area
      else if (nodeY + m_RowHeight > m_ScrollOffset + viewportHeight) {
        m_ScrollOffset = nodeY + m_RowHeight - viewportHeight;
        if (m_VerticalScrollBar->isVisible()) {
          m_VerticalScrollBar->setValue(m_ScrollOffset);
        }
      }
      break;
    }
    visibleIndex++;
  }
}

void SceneGraphWidget::RebuildFlatList() {
  m_FlatList.clear();

  if (!m_SceneGraph) {
    return;
  }

  Quantum::GraphNode *root = m_SceneGraph->GetRoot();
  if (root) {
    // Add root and its children
    RebuildFlatListRecursive(root, 0, true);
  }
}

void SceneGraphWidget::RebuildFlatListRecursive(Quantum::GraphNode *node,
                                                int depth, bool parentVisible) {
  if (!node) {
    return;
  }

  TreeViewItem item;
  item.Node = node;
  item.Depth = depth;
  item.IsExpanded = IsNodeExpanded(node); // Check persisted state
  item.IsVisible = parentVisible;

  m_FlatList.push_back(item);

  // Process children
  const auto &children = node->GetChildren();
  for (const auto &child : children) {
    // Child visibility depends on this node's expanded state and visibility
    bool childVisible = parentVisible && item.IsExpanded;
    RebuildFlatListRecursive(child.get(), depth + 1, childVisible);
  }
}

bool SceneGraphWidget::IsNodeExpanded(Quantum::GraphNode *node) const {
  // Nodes are expanded by default; only collapsed if in the set
  return m_CollapsedNodes.find(node) == m_CollapsedNodes.end();
}

void SceneGraphWidget::ToggleNodeExpanded(Quantum::GraphNode *node) {
  if (!node) {
    return;
  }

  if (IsNodeExpanded(node)) {
    // Currently expanded, collapse it
    m_CollapsedNodes.insert(node);
  } else {
    // Currently collapsed, expand it
    m_CollapsedNodes.erase(node);
  }
}

int SceneGraphWidget::GetTotalContentHeight() const {
  int visibleCount = 0;
  for (const auto &item : m_FlatList) {
    if (item.IsVisible) {
      visibleCount++;
    }
  }
  return visibleCount * m_RowHeight;
}

int SceneGraphWidget::GetItemIndexAtY(int y) const {
  int adjustedY = y + m_ScrollOffset;
  int visibleIndex = 0;

  for (size_t i = 0; i < m_FlatList.size(); i++) {
    if (m_FlatList[i].IsVisible) {
      int itemTop = visibleIndex * m_RowHeight;
      int itemBottom = itemTop + m_RowHeight;

      if (adjustedY >= itemTop && adjustedY < itemBottom) {
        return static_cast<int>(i);
      }
      visibleIndex++;
    }
  }

  return -1;
}

void SceneGraphWidget::UpdateScrollBar() {
  int contentHeight = GetTotalContentHeight();
  int viewportHeight = height();

  if (contentHeight > viewportHeight) {
    m_VerticalScrollBar->setVisible(true);
    m_VerticalScrollBar->setRange(0, contentHeight - viewportHeight);
    m_VerticalScrollBar->setPageStep(viewportHeight);
    m_VerticalScrollBar->setSingleStep(m_RowHeight);

    // Position scrollbar on the right
    int scrollBarWidth = m_VerticalScrollBar->sizeHint().width();
    m_VerticalScrollBar->setGeometry(width() - scrollBarWidth, 0,
                                     scrollBarWidth, viewportHeight);
  } else {
    m_VerticalScrollBar->setVisible(false);
    m_ScrollOffset = 0;
  }
}

void SceneGraphWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);

  // Calculate visible area
  int scrollBarWidth =
      m_VerticalScrollBar->isVisible() ? m_VerticalScrollBar->width() : 0;
  int drawWidth = width() - scrollBarWidth;

  int rowIndex = 0; // For alternating colors
  int drawY = 0;

  for (size_t i = 0; i < m_FlatList.size(); i++) {
    const TreeViewItem &item = m_FlatList[i];

    if (!item.IsVisible) {
      continue;
    }

    int itemY = drawY - m_ScrollOffset;

    // Skip items above viewport
    if (itemY + m_RowHeight < 0) {
      rowIndex++;
      drawY += m_RowHeight;
      continue;
    }

    // Stop if below viewport
    if (itemY >= height()) {
      break;
    }

    // Draw row background (alternating colors)
    QRect rowRect(0, itemY, drawWidth, m_RowHeight);

    if (item.Node == m_SelectedNode) {
      painter.fillRect(rowRect, m_SelectedColor);
    } else {
      // Alternating dark/light
      QColor bgColor = (rowIndex % 2 == 0) ? m_DarkRowColor : m_LightRowColor;
      painter.fillRect(rowRect, bgColor);
    }

    // Calculate indent
    int indent = item.Depth * m_IndentWidth + 5;

    // Draw expand/collapse icon if node has children
    bool hasChildren = item.Node && !item.Node->GetChildren().empty();
    if (hasChildren) {
      int iconX = indent;
      int iconY = itemY + (m_RowHeight / 2);
      int iconSize = 8;

      painter.setPen(m_ExpandIconColor);

      if (item.IsExpanded) {
        // Draw down arrow (V shape) - expanded
        QPolygon arrow;
        arrow << QPoint(iconX, iconY - 3) << QPoint(iconX + iconSize, iconY - 3)
              << QPoint(iconX + iconSize / 2, iconY + 3);
        painter.setBrush(m_ExpandIconColor);
        painter.drawPolygon(arrow);
      } else {
        // Draw right arrow (> shape) - collapsed
        QPolygon arrow;
        arrow << QPoint(iconX, iconY - 4) << QPoint(iconX + iconSize, iconY)
              << QPoint(iconX, iconY + 4);
        painter.setBrush(m_ExpandIconColor);
        painter.drawPolygon(arrow);
      }

      indent += iconSize + 8;
    } else {
      indent += 16; // Spacing for items without children
    }

    // Draw node name
    if (item.Node) {
      painter.setPen(m_TextColor);
      QString name = QString::fromStdString(item.Node->GetName());
      QRect textRect(indent, itemY, drawWidth - indent - 5, m_RowHeight);
      painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, name);
    }

    rowIndex++;
    drawY += m_RowHeight;
  }
}

void SceneGraphWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  UpdateScrollBar();
}

void SceneGraphWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    int itemIndex = GetItemIndexAtY(event->pos().y());
    if (itemIndex >= 0 && itemIndex < static_cast<int>(m_FlatList.size())) {
      const TreeViewItem &item = m_FlatList[itemIndex];

      // Check if clicked on expand/collapse icon area
      int indent = item.Depth * m_IndentWidth + 5;
      int iconEnd = indent + 16;
      bool hasChildren = item.Node && !item.Node->GetChildren().empty();

      if (hasChildren && event->pos().x() >= indent &&
          event->pos().x() < iconEnd) {
        // Toggle expand/collapse
        ToggleNodeExpanded(item.Node);

        // Rebuild to recalculate visibility
        RebuildFlatList();
        UpdateScrollBar();
        update();
      } else {
        // Internal selection only (for visual feedback)
        m_SelectedNode = item.Node;
        m_PotentialSelection = item.Node;
        update();
      }
    }
    m_DragStartPosition = event->pos();
    m_Dragging = false;
  }

  QWidget::mousePressEvent(event);
}

void SceneGraphWidget::wheelEvent(QWheelEvent *event) {
  if (m_VerticalScrollBar->isVisible()) {
    int delta = event->angleDelta().y();
    int scrollAmount = -delta / 4; // Adjust sensitivity
    m_VerticalScrollBar->setValue(m_VerticalScrollBar->value() + scrollAmount);
  }

  event->accept();
}

void SceneGraphWidget::OnScrollBarValueChanged(int value) {
  m_ScrollOffset = value;
  update();
}

void SceneGraphWidget::dragEnterEvent(QDragEnterEvent *event) {
  // Accept drag if it contains QLang script data
  if (event->mimeData()->hasFormat("application/x-qlang-script")) {
    event->acceptProposedAction();
  } else {
    event->ignore();
  }
}

void SceneGraphWidget::dragMoveEvent(QDragMoveEvent *event) {
  // Check if we're over a valid node
  int itemIndex = GetItemIndexAtY(event->position().toPoint().y());
  if (itemIndex >= 0 && itemIndex < static_cast<int>(m_FlatList.size())) {
    event->acceptProposedAction();
  } else {
    event->ignore();
  }
}

void SceneGraphWidget::dropEvent(QDropEvent *event) {
  if (!event->mimeData()->hasFormat("application/x-qlang-script")) {
    event->ignore();
    return;
  }

  // Get the target node from drop position
  int itemIndex = GetItemIndexAtY(event->position().toPoint().y());
  if (itemIndex < 0 || itemIndex >= static_cast<int>(m_FlatList.size())) {
    event->ignore();
    return;
  }

  Quantum::GraphNode *targetNode = m_FlatList[itemIndex].Node;
  if (!targetNode) {
    event->ignore();
    return;
  }

  // Get the script path from the dropped data
  QString scriptPath =
      QString::fromUtf8(event->mimeData()->data("application/x-qlang-script"));

  qDebug() << "Dropping script" << scriptPath << "onto node"
           << QString::fromStdString(targetNode->GetName());

  // Load the class instance from QLangDomain
  if (EngineGlobals::m_QDomain) {
    std::shared_ptr<QClassInstance> classInstance =
        EngineGlobals::m_QDomain->LoadClass(scriptPath.toStdString(),
                                            targetNode);

    if (classInstance) {
      targetNode->AddScript(classInstance);
      qDebug() << "Script attached successfully";
    } else {
      qDebug() << "Failed to load script class from:" << scriptPath;
    }
  } else {
    qDebug() << "QLangDomain not initialized";
  }

  event->acceptProposedAction();
  update();
}

void SceneGraphWidget::mouseMoveEvent(QMouseEvent *event) {
  if (!(event->buttons() & Qt::LeftButton))
    return;

  if (!m_Dragging && (event->pos() - m_DragStartPosition).manhattanLength() <
                         QApplication::startDragDistance())
    return;

  if (!m_Dragging) {
    m_Dragging = true;
    m_PotentialSelection = nullptr; // Cancel selection on drag
  }

  int itemIndex = GetItemIndexAtY(m_DragStartPosition.y());
  if (itemIndex < 0 || itemIndex >= static_cast<int>(m_FlatList.size()))
    return;

  Quantum::GraphNode *node = m_FlatList[itemIndex].Node;
  if (!node)
    return;

  QDrag *drag = new QDrag(this);
  QMimeData *mimeData = new QMimeData;

  // Store the node pointer and the full name
  mimeData->setData("application/x-quantum-node-ptr",
                    QByteArray((const char *)&node, sizeof(node)));
  mimeData->setText(QString::fromStdString(node->GetFullName()));

  drag->setMimeData(mimeData);
  drag->exec(Qt::CopyAction | Qt::MoveAction);

  // After drag exec, reset state
  m_Dragging = false;
}

void SceneGraphWidget::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    if (m_PotentialSelection) {
      // Finalize the selection change
      m_SelectedNode = m_PotentialSelection;

      if (m_SelectedNode) {
        emit NodeSelected(m_SelectedNode);

        m_UpdatingSelection = true;

        if (m_SceneGraph) {
          auto sharedNode = m_SceneGraph->FindNode(m_SelectedNode->GetName());
          if (sharedNode) {
            EngineGlobals::SetSelectedNode(sharedNode);
          }
        }

        m_UpdatingSelection = false;
      }
      m_PotentialSelection = nullptr;
      update();
    }
    m_Dragging = false;
  }
  QWidget::mouseReleaseEvent(event);
}
