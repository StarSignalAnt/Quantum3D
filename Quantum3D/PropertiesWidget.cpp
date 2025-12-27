#define NOMINMAX
#include "PropertiesWidget.h"
// JIT types (QJClassInstance, QJitRunner, etc.) included via QLangDomain.h
#include "../QuantumEngine/GraphNode.h"
#include "../QuantumEngine/QLangDomain.h"
#include "../QuantumEngine/SceneGraph.h"
#include "EngineGlobals.h"
#include "SceneGraphWidget.h"
#include "ViewportWidget.h"
#include <QClipboard>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <variant>

// Undefine Windows GetClassName macro to avoid conflict with
// QJClassInstance::GetClassName
#ifdef GetClassName
#undef GetClassName
#endif

using namespace Quantum;

const int RowHeight = 26;
const int Padding = 10;
const int NameWidth = 120;

PropertiesWidget::PropertiesWidget(QWidget *parent) : QWidget(parent) {
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setAcceptDrops(true);
  m_CursorTimer = new QTimer(this);
  connect(m_CursorTimer, &QTimer::timeout, this,
          &PropertiesWidget::OnCursorTimer);
  m_CursorTimer->start(500);
}

PropertiesWidget::~PropertiesWidget() {}

void PropertiesWidget::SetNode(GraphNode *node) {
  if (m_CurrentNode == node)
    return;
  m_CurrentNode = node;
  RefreshProperties();
}

void PropertiesWidget::RefreshProperties() {
  m_Fields.clear();
  if (!m_CurrentNode) {
    update();
    return;
  }

  AddHeader("Node");

  // Name
  PropertyField nameField;
  nameField.Name = "Name";
  nameField.Type = PropertyType::String;
  nameField.GetString = [this]() { return m_CurrentNode->GetName(); };
  nameField.SetString = [this](const std::string &val) {
    m_CurrentNode->SetName(val);
    if (EngineGlobals::SceneGraphPanel) {
      EngineGlobals::SceneGraphPanel->RefreshTree();
    }
  };
  m_Fields.push_back(nameField);

  AddHeader("Transform");

  // Position
  PropertyField posField;
  posField.Name = "Position";
  posField.Type = PropertyType::Vec3;
  posField.GetVec3 = [this]() { return m_CurrentNode->GetLocalPosition(); };
  posField.SetVec3 = [this](glm::vec3 val) {
    m_CurrentNode->SetLocalPosition(val);
  };
  m_Fields.push_back(posField);

  // Rotation
  PropertyField rotField;
  rotField.Name = "Rotation";
  rotField.Type = PropertyType::Vec3;
  rotField.GetVec3 = [this]() { return m_CurrentNode->GetRotationEuler(); };
  rotField.SetVec3 = [this](glm::vec3 val) {
    m_CurrentNode->SetRotationEuler(val);
  };
  m_Fields.push_back(rotField);

  // Scale
  PropertyField scaleField;
  scaleField.Name = "Scale";
  scaleField.Type = PropertyType::Vec3;
  scaleField.GetVec3 = [this]() { return m_CurrentNode->GetLocalScale(); };
  scaleField.SetVec3 = [this](glm::vec3 val) {
    m_CurrentNode->SetLocalScale(val);
  };
  m_Fields.push_back(scaleField);

  // Scripts (QLang JIT class member properties via ScriptPair)
  for (auto *scriptPair : m_CurrentNode->GetScripts()) {
    if (!scriptPair || !scriptPair->ClsInstance)
      continue;

    auto clsInstance = scriptPair->ClsInstance;
    std::string className = clsInstance->GetClassName();
    AddHeader(className);

    // DEBUG: Log member count
    std::cout << "[DEBUG] PropertiesWidget: Class '" << className << "' has "
              << clsInstance->GetMembers().size() << " registered members"
              << std::endl;
    for (const auto &m : clsInstance->GetMembers()) {
      std::cout << "  - " << m.first << " (typeToken=" << m.second.typeToken
                << ", typeName=" << m.second.typeName << ")" << std::endl;
    }

    // Helper to check for GameNode inheritance
    auto isGameNodeClass = [](const std::string &checkClassName) {
      if (checkClassName == "GameNode")
        return true;
      auto runner = QLangDomain::m_QLang->GetRunner();
      if (!runner)
        return false;
      // Check inheritance chain via compiled classes
      // For now simple check - consider any non-primitive type as potential
      // GameNode
      return true; // TODO: implement proper inheritance check via QJitRunner
    };

    // Iterate over registered members
    for (const auto &memberPair : clsInstance->GetMembers()) {
      const std::string &fieldName = memberPair.first;
      const MemberInfo &memberInfo = memberPair.second;

      PropertyField field;
      field.Name = fieldName;

      // Skip internal members like NodePtr
      if (fieldName == "NodePtr")
        continue;

      // Determine type from typeName (more reliable than typeToken)
      std::string typeName = memberInfo.typeName;

      if (typeName == "float32" || typeName == "float") {
        field.Type = PropertyType::Float;
        field.GetFloat = [clsInstance, fieldName]() {
          return clsInstance->GetMember<float>(fieldName);
        };
        field.SetFloat = [clsInstance, fieldName](float val) {
          clsInstance->SetMember<float>(fieldName, val);
        };
      } else if (typeName == "int32" || typeName == "int") {
        field.Type = PropertyType::Int;
        field.GetInt = [clsInstance, fieldName]() {
          return clsInstance->GetMember<int32_t>(fieldName);
        };
        field.SetInt = [clsInstance, fieldName](int val) {
          clsInstance->SetMember<int32_t>(fieldName, static_cast<int32_t>(val));
        };
      } else if (typeName == "bool") {
        field.Type = PropertyType::Bool;
        field.GetBool = [clsInstance, fieldName]() {
          return clsInstance->GetMember<bool>(fieldName);
        };
        field.SetBool = [clsInstance, fieldName](bool val) {
          clsInstance->SetMember<bool>(fieldName, val);
        };
      } else if (typeName == "cptr" || typeName == "iptr" ||
                 typeName == "fptr" || typeName == "bptr") {
        // Pointer types - skip for now unless it's a class reference
        continue;
      } else if (!typeName.empty()) {
        // T_IDENTIFIER - class reference (stored as pointer)
        if (typeName == "Vec3") {
          // Vec3 is embedded, not a pointer - access via struct offset
          field.Type = PropertyType::Vec3;
          field.GetVec3 = [clsInstance, fieldName]() {
            // Get pointer to the Vec3 struct within the class instance
            void *instancePtr = clsInstance->GetInstancePtr();
            const auto &members = clsInstance->GetMembers();
            auto it = members.find(fieldName);
            if (it == members.end())
              return glm::vec3(0.0f);

            // The member is at offset within the struct
            char *memberPtr =
                static_cast<char *>(instancePtr) + it->second.offset;
            // Vec3 struct has X, Y, Z as consecutive floats
            float *floatPtr = reinterpret_cast<float *>(memberPtr);
            return glm::vec3(floatPtr[0], floatPtr[1], floatPtr[2]);
          };
          field.SetVec3 = [clsInstance, fieldName](glm::vec3 val) {
            void *instancePtr = clsInstance->GetInstancePtr();
            const auto &members = clsInstance->GetMembers();
            auto it = members.find(fieldName);
            if (it == members.end())
              return;

            char *memberPtr =
                static_cast<char *>(instancePtr) + it->second.offset;
            float *floatPtr = reinterpret_cast<float *>(memberPtr);
            floatPtr[0] = val.x;
            floatPtr[1] = val.y;
            floatPtr[2] = val.z;
          };
        } else if (isGameNodeClass(typeName)) {
          // Class reference (stored as pointer in struct)
          field.Type = PropertyType::Node;
          field.TargetClass = typeName;

          field.GetNodeName = [clsInstance, fieldName]() {
            void *ptr = clsInstance->GetPtrMember(fieldName);
            if (ptr) {
              // ptr is the class instance's raw memory
              // If the target class inherits from GameNode, NodePtr is the
              // first member
              void **nodePtrLocation = static_cast<void **>(ptr);
              void *nodePtr = *nodePtrLocation;
              if (nodePtr) {
                auto *node = static_cast<GraphNode *>(nodePtr);
                return node->GetFullName();
              }
              std::stringstream ss;
              ss << "instance@" << ptr;
              return ss.str();
            }
            return std::string("null");
          };
          field.ClearNode = [clsInstance, fieldName]() {
            clsInstance->SetPtrMember(fieldName, nullptr);
          };
          field.SetNode = [clsInstance, fieldName,
                           typeName](GraphNode *newNode) {
            if (!newNode) {
              clsInstance->SetPtrMember(fieldName, nullptr);
              return;
            }
            // Scan the dropped node's scripts for matching class type
            for (auto *sp : newNode->GetScripts()) {
              if (!sp || !sp->ClsInstance)
                continue;
              if (sp->ClsInstance->GetClassName() == typeName) {
                void *instancePtr = sp->ClsInstance->GetInstancePtr();
                clsInstance->SetPtrMember(fieldName, instancePtr);
                std::cout << "[INFO] PropertiesWidget: Set '" << fieldName
                          << "' to instance of '" << typeName << "' from node '"
                          << newNode->GetName() << "'" << std::endl;
                return;
              }
            }
            std::cerr << "[WARNING] PropertiesWidget: Node '"
                      << newNode->GetName() << "' has no script of type '"
                      << typeName << "'" << std::endl;
          };
        } else {
          continue; // Unknown class type
        }
      } else {
        continue; // Unsupported type
      }
      m_Fields.push_back(field);
    }
  }
  resizeEvent(nullptr);
  update();
}

void PropertiesWidget::AddHeader(const std::string &name) {
  PropertyField header;
  header.Name = name;
  header.Type = PropertyType::Header;
  m_Fields.push_back(header);
}

bool PropertiesWidget::event(QEvent *event) {
  if (event->type() == QEvent::KeyPress) {
    QKeyEvent *ke = static_cast<QKeyEvent *>(event);
    if (ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Backtab) {
      keyPressEvent(ke);
      return true;
    }
  }
  return QWidget::event(event);
}

void PropertiesWidget::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  // Main background
  painter.fillRect(rect(), QColor(30, 30, 30)); // #1E1E1E

  QFont normalFont = painter.font();
  normalFont.setPointSize(9);
  normalFont.setFamily("Segoe UI");

  QFont headerFont = normalFont;
  headerFont.setBold(true);

  for (int i = 0; i < (int)m_Fields.size(); ++i) {
    auto &field = m_Fields[i];

    if (field.Type == PropertyType::Header) {
      // Header background
      painter.fillRect(field.NameRect.adjusted(0, 0, width(), 0),
                       QColor(45, 45, 45)); // #2D2D2D

      // Top/bottom borders for headers
      painter.setPen(QColor(64, 64, 64)); // #404040
      painter.drawLine(field.NameRect.left(), field.NameRect.top(), width(),
                       field.NameRect.top());
      painter.drawLine(field.NameRect.left(), field.NameRect.bottom(), width(),
                       field.NameRect.bottom());

      painter.setPen(Qt::white);
      painter.setFont(headerFont);
      painter.drawText(field.NameRect.adjusted(Padding, 0, 0, 0),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       QString::fromStdString(field.Name));
      continue;
    }

    // Row hover highlight
    if (m_HoverIdx == i) {
      painter.fillRect(field.NameRect.adjusted(0, 0, width(), 0),
                       QColor(42, 42, 42)); // #2A2A2A
    }

    painter.setFont(normalFont);
    painter.setPen(QColor(180, 180, 180));
    painter.drawText(field.NameRect.adjusted(Padding, 0, 0, 0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QString::fromStdString(field.Name));

    // Field background (inset look)
    auto drawFieldBG = [&](const QRect &r, bool isEditing, bool isHovered) {
      painter.fillRect(r, QColor(18, 18, 18)); // #121212
      if (isEditing) {
        // Focus Indicator: Blue border with subtle inner glow
        painter.setPen(QPen(QColor(0, 120, 215), 2));
        painter.drawRect(r.adjusted(1, 1, -1, -1));
      } else if (isHovered) {
        painter.setPen(QColor(60, 60, 60));
        painter.drawRect(r.adjusted(0, 0, -1, -1));
      } else {
        painter.setPen(QColor(45, 45, 45));
        painter.drawRect(r.adjusted(0, 0, -1, -1));
      }
    };

    if (field.Type != PropertyType::Vec3) {
      drawFieldBG(field.ValueRect, field.IsEditing, m_HoverIdx == i);

      std::string displayVal;
      if (field.IsEditing)
        displayVal = field.EditBuffer;
      else {
        if (field.Type == PropertyType::String)
          displayVal = field.GetString();
        else if (field.Type == PropertyType::Float) {
          std::stringstream ss;
          ss << std::fixed << std::setprecision(2) << field.GetFloat();
          displayVal = ss.str();
        } else if (field.Type == PropertyType::Int)
          displayVal = std::to_string(field.GetInt());
        else if (field.Type == PropertyType::Bool)
          displayVal = field.GetBool() ? "true" : "false";
        else if (field.Type == PropertyType::Node)
          displayVal = field.GetNodeName();
      }

      painter.save();
      painter.setClipRect(field.ValueRect.adjusted(2, 2, -2, -2));

      // Selection
      if (field.IsEditing && field.SelectionStart != field.SelectionEnd) {
        int start = (std::min)(field.SelectionStart, field.SelectionEnd);
        int end = (std::max)(field.SelectionStart, field.SelectionEnd);
        QFontMetrics fm(painter.font());
        int startX = fm.horizontalAdvance(
            QString::fromStdString(displayVal.substr(0, start)));
        int endX = fm.horizontalAdvance(
            QString::fromStdString(displayVal.substr(0, end)));
        QRect selRect(field.ValueRect.left() + Padding + startX - field.ScrollX,
                      field.ValueRect.top() + 4, endX - startX,
                      field.ValueRect.height() - 8);
        painter.fillRect(selRect, QColor(0, 120, 215, 150));
      }

      if (field.Type == PropertyType::Node) {
        painter.setPen(QColor(100, 180, 255)); // Light blue for nodes
      } else {
        painter.setPen(Qt::white);
      }
      painter.drawText(
          field.ValueRect.adjusted(Padding - field.ScrollX, 0, -Padding, 0),
          Qt::AlignLeft | Qt::AlignVCenter, QString::fromStdString(displayVal));

      if (field.IsEditing && m_CursorVisible) {
        QFontMetrics fm(painter.font());
        int cursorX = fm.horizontalAdvance(
            QString::fromStdString(displayVal.substr(0, field.CursorPos)));
        painter.setPen(Qt::white);
        painter.drawLine(
            field.ValueRect.left() + Padding + cursorX - field.ScrollX,
            field.ValueRect.top() + 6,
            field.ValueRect.left() + Padding + cursorX - field.ScrollX,
            field.ValueRect.bottom() - 6);
      }
      painter.restore();
    } else {
      glm::vec3 v = field.GetVec3();
      const char *comps[] = {"X", "Y", "Z"};
      QColor axisColors[] = {QColor(255, 75, 75), QColor(75, 255, 75),
                             QColor(75, 75, 255)};
      float vals[] = {v.x, v.y, v.z};

      for (int s = 0; s < 3; ++s) {
        QRect sub = field.SubRects[s];
        bool isSubEditing = (field.IsEditing && field.EditingSubIndex == s);
        bool isSubHovered = (m_HoverIdx == i && m_HoverSubIdx == s);

        drawFieldBG(sub, isSubEditing, isSubHovered);

        // Axis Indicator Strip
        painter.fillRect(sub.left() + 2, sub.top() + 4, 3, sub.height() - 8,
                         axisColors[s]);

        std::string sText;
        if (isSubEditing)
          sText = field.EditBuffer;
        else {
          std::stringstream ss;
          ss << std::fixed << std::setprecision(2) << vals[s];
          sText = ss.str();
        }

        painter.save();
        painter.setClipRect(sub.adjusted(12, 2, -2, -2));
        int scroll = isSubEditing ? field.SubScrollX[s] : 0;

        if (isSubEditing && field.SelectionStart != field.SelectionEnd) {
          int start = (std::min)(field.SelectionStart, field.SelectionEnd);
          int end = (std::max)(field.SelectionStart, field.SelectionEnd);
          QFontMetrics fm(painter.font());
          int startX = fm.horizontalAdvance(
              QString::fromStdString(sText.substr(0, start)));
          int endX = fm.horizontalAdvance(
              QString::fromStdString(sText.substr(0, end)));
          QRect selRect(sub.left() + 12 + startX - scroll, sub.top() + 4,
                        endX - startX, sub.height() - 8);
          painter.fillRect(selRect, QColor(0, 120, 215, 150));
        }

        painter.setPen(Qt::white);
        painter.drawText(sub.adjusted(12 - scroll, 0, -2, 0),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QString::fromStdString(sText));

        if (isSubEditing && m_CursorVisible) {
          QFontMetrics fm(painter.font());
          int cursorX = fm.horizontalAdvance(
              QString::fromStdString(sText.substr(0, field.CursorPos)));
          painter.setPen(Qt::white);
          painter.drawLine(sub.left() + 12 + cursorX - scroll, sub.top() + 6,
                           sub.left() + 12 + cursorX - scroll,
                           sub.bottom() - 6);
        }
        painter.restore();
      }
    }
  }
}

void PropertiesWidget::mousePressEvent(QMouseEvent *event) {
  bool found = false;
  for (auto &field : m_Fields) {
    if (field.Type == PropertyType::Header)
      continue;

    bool hitValue = field.ValueRect.contains(event->pos());
    int subIndex = -1;
    if (field.Type == PropertyType::Vec3) {
      for (int i = 0; i < 3; ++i) {
        if (field.SubRects[i].contains(event->pos())) {
          hitValue = true;
          subIndex = i;
          break;
        }
      }
    }

    if (hitValue) {
      if (field.IsEditing && field.Type == PropertyType::Vec3 &&
          field.EditingSubIndex != subIndex) {
        ApplyChanges(field);
      }
      field.IsEditing = true;
      field.EditingSubIndex = subIndex;
      // ... (rest of the hitValue block remains same, just ensuring we use
      // correct scroll below)

      if (field.Type == PropertyType::String)
        field.EditBuffer = field.GetString();
      else if (field.Type == PropertyType::Float) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << field.GetFloat();
        field.EditBuffer = ss.str();
      } else if (field.Type == PropertyType::Int)
        field.EditBuffer = std::to_string(field.GetInt());
      else if (field.Type == PropertyType::Bool) {
        field.SetBool(!field.GetBool());
        field.IsEditing = false;
      } else if (field.Type == PropertyType::Vec3 && subIndex != -1) {
        glm::vec3 v = field.GetVec3();
        float val = (subIndex == 0) ? v.x : (subIndex == 1) ? v.y : v.z;
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << val;
        field.EditBuffer = ss.str();
      } else if (field.Type == PropertyType::Node) {
        // Don't clear on click, only on drag-drop or explicit action
        // field.ClearNode();
        // RefreshProperties();
        // return;
      }

      if (field.IsEditing && event->button() != Qt::RightButton) {
        field.CursorPos = posToIndex(field, event->pos().x());
        field.SelectionStart = field.CursorPos;
        field.SelectionEnd = field.CursorPos;
        ensureCursorVisible(field);
        setFocus();
      } else if (field.IsEditing && event->button() == Qt::RightButton) {
        setFocus();
      }
      found = true;
    } else {
      if (field.IsEditing) {
        ApplyChanges(field);
        field.IsEditing = false;
        field.EditingSubIndex = -1;
      }
    }
  }
  if (!found) {
    setFocus();
  }
  update();
}

void PropertiesWidget::mouseMoveEvent(QMouseEvent *event) {
  int oldHover = m_HoverIdx;
  int oldSub = m_HoverSubIdx;
  m_HoverIdx = -1;
  m_HoverSubIdx = -1;

  for (int i = 0; i < (int)m_Fields.size(); ++i) {
    auto &field = m_Fields[i];
    if (field.Type == PropertyType::Header)
      continue;

    if (field.NameRect.contains(event->pos()) ||
        field.ValueRect.contains(event->pos())) {
      m_HoverIdx = i;
      if (field.Type == PropertyType::Vec3) {
        for (int s = 0; s < 3; ++s) {
          if (field.SubRects[s].contains(event->pos())) {
            m_HoverSubIdx = s;
            break;
          }
        }
      }
      break;
    }
  }

  if (oldHover != m_HoverIdx || oldSub != m_HoverSubIdx) {
    update();
  }

  // Handle drag selection
  for (auto &field : m_Fields) {
    if (field.IsEditing && (event->buttons() & Qt::LeftButton)) {
      field.CursorPos = posToIndex(field, event->pos().x());
      field.SelectionEnd = field.CursorPos;
      ensureCursorVisible(field);
      update();
      break;
    }
  }
}

void PropertiesWidget::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasFormat("application/x-quantum-node-ptr")) {
    event->acceptProposedAction();
  } else {
    event->ignore();
  }
}

void PropertiesWidget::dragMoveEvent(QDragMoveEvent *event) {
  int hoveredIdx = -1;
  for (int i = 0; i < (int)m_Fields.size(); ++i) {
    if (m_Fields[i].NameRect.contains(event->position().toPoint()) ||
        m_Fields[i].ValueRect.contains(event->position().toPoint())) {
      hoveredIdx = i;
      break;
    }
  }

  if (hoveredIdx != -1) {
    PropertyField &field = m_Fields[hoveredIdx];
    if (field.Type == PropertyType::Node) {
      // Validate node type
      QByteArray data =
          event->mimeData()->data("application/x-quantum-node-ptr");
      if (data.size() == sizeof(GraphNode *)) {
        GraphNode *draggedNode = nullptr;
        memcpy(&draggedNode, data.constData(), sizeof(GraphNode *));

        if (draggedNode) {
          bool valid = false;
          if (field.TargetClass == "GameNode") {
            valid = true; // Any node for base GameNode
          } else {
            // Check if node has a script with matching class name
            for (auto *scriptPair : draggedNode->GetScripts()) {
              if (scriptPair && scriptPair->ClsInstance &&
                  scriptPair->ClsInstance->GetClassName() ==
                      field.TargetClass) {
                valid = true;
                break;
              }
            }
          }

          if (valid) {
            event->acceptProposedAction();
            return;
          }
        }
      }
    }
  }
  event->ignore();
}

void PropertiesWidget::dropEvent(QDropEvent *event) {
  int hoveredIdx = -1;
  for (int i = 0; i < (int)m_Fields.size(); ++i) {
    if (m_Fields[i].NameRect.contains(event->position().toPoint()) ||
        m_Fields[i].ValueRect.contains(event->position().toPoint())) {
      hoveredIdx = i;
      break;
    }
  }

  if (hoveredIdx != -1) {
    PropertyField &field = m_Fields[hoveredIdx];
    if (field.Type == PropertyType::Node) {
      QByteArray data =
          event->mimeData()->data("application/x-quantum-node-ptr");
      if (data.size() == sizeof(GraphNode *)) {
        GraphNode *draggedNode = nullptr;
        memcpy(&draggedNode, data.constData(), sizeof(GraphNode *));

        if (draggedNode && field.SetNode) {
          field.SetNode(draggedNode);
          RefreshProperties();
          event->acceptProposedAction();
          return;
        }
      }
    }
  }
  event->ignore();
}

void PropertiesWidget::leaveEvent(QEvent *event) {
  m_HoverIdx = -1;
  m_HoverSubIdx = -1;
  update();
}

void PropertiesWidget::mouseReleaseEvent(QMouseEvent *event) {}

void PropertiesWidget::keyPressEvent(QKeyEvent *event) {
  int activeIdx = -1;
  for (int i = 0; i < (int)m_Fields.size(); ++i) {
    if (m_Fields[i].IsEditing) {
      activeIdx = i;
      break;
    }
  }

  if (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab) {
    bool forward = (event->key() == Qt::Key_Tab);
    int nextIdx = (activeIdx == -1) ? (forward ? 0 : (int)m_Fields.size() - 1)
                                    : activeIdx;
    int nextSub = (activeIdx == -1) ? 0 : m_Fields[activeIdx].EditingSubIndex;

    if (activeIdx != -1) {
      ApplyChanges(m_Fields[activeIdx]);
      m_Fields[activeIdx].IsEditing = false;
    }

    auto advance = [&]() {
      if (forward) {
        if (m_Fields[nextIdx].Type == PropertyType::Vec3 && nextSub < 2) {
          nextSub++;
        } else {
          nextIdx = (nextIdx + 1) % m_Fields.size();
          nextSub = 0;
        }
      } else {
        if (m_Fields[nextIdx].Type == PropertyType::Vec3 && nextSub > 0) {
          nextSub--;
        } else {
          nextIdx = (nextIdx - 1 + (int)m_Fields.size()) % m_Fields.size();
          nextSub = (m_Fields[nextIdx].Type == PropertyType::Vec3) ? 2 : -1;
        }
      }
    };

    if (activeIdx != -1)
      advance();

    // Find next editable/activatable field
    int startIdx = nextIdx;
    int startSub = nextSub;
    while (m_Fields[nextIdx].Type == PropertyType::Header ||
           m_Fields[nextIdx].Type == PropertyType::Bool) {
      advance();
      if (nextIdx == startIdx && nextSub == startSub)
        break;
    }

    auto &nextField = m_Fields[nextIdx];
    nextField.IsEditing = true;
    nextField.EditingSubIndex =
        (nextField.Type == PropertyType::Vec3) ? nextSub : -1;

    // Initialize edit buffer
    if (nextField.Type == PropertyType::String)
      nextField.EditBuffer = nextField.GetString();
    else if (nextField.Type == PropertyType::Float) {
      std::stringstream ss;
      ss << std::fixed << std::setprecision(2) << nextField.GetFloat();
      nextField.EditBuffer = ss.str();
    } else if (nextField.Type == PropertyType::Int)
      nextField.EditBuffer = std::to_string(nextField.GetInt());
    else if (nextField.Type == PropertyType::Vec3) {
      glm::vec3 v = nextField.GetVec3();
      float val = (nextSub == 0) ? v.x : (nextSub == 1) ? v.y : v.z;
      std::stringstream ss;
      ss << std::fixed << std::setprecision(2) << val;
      nextField.EditBuffer = ss.str();
    }

    nextField.SelectionStart = 0;
    nextField.SelectionEnd = (int)nextField.EditBuffer.length();
    nextField.CursorPos = nextField.SelectionEnd;
    ensureCursorVisible(nextField);
    update();
    return;
  }

  if (activeIdx == -1)
    return;
  PropertyField &field = m_Fields[activeIdx];
  bool changed = false;

  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter ||
      event->key() == Qt::Key_Escape) {
    ApplyChanges(field);
    field.IsEditing = false;
    field.EditingSubIndex = -1;
    update();
    return;
  } else if (event->key() == Qt::Key_Backspace) {
    if (field.SelectionStart != field.SelectionEnd) {
      int start = std::min(field.SelectionStart, field.SelectionEnd);
      int end = std::max(field.SelectionStart, field.SelectionEnd);
      field.EditBuffer.erase(start, end - start);
      field.CursorPos = start;
      field.SelectionStart = field.SelectionEnd = field.CursorPos;
    } else if (field.CursorPos > 0) {
      field.EditBuffer.erase(field.CursorPos - 1, 1);
      field.CursorPos--;
      field.SelectionStart = field.SelectionEnd = field.CursorPos;
    }
    changed = true;
  } else if (event->key() == Qt::Key_Delete) {
    if (field.SelectionStart != field.SelectionEnd) {
      int start = std::min(field.SelectionStart, field.SelectionEnd);
      int end = std::max(field.SelectionStart, field.SelectionEnd);
      field.EditBuffer.erase(start, end - start);
      field.CursorPos = start;
      field.SelectionStart = field.SelectionEnd = field.CursorPos;
    } else if (field.CursorPos < (int)field.EditBuffer.length()) {
      field.EditBuffer.erase(field.CursorPos, 1);
    }
    changed = true;
  } else if (event->key() == Qt::Key_Left) {
    if (field.CursorPos > 0)
      field.CursorPos--;
    if (!(event->modifiers() & Qt::ShiftModifier))
      field.SelectionStart = field.CursorPos;
    field.SelectionEnd = field.CursorPos;
    ensureCursorVisible(field);
  } else if (event->key() == Qt::Key_Right) {
    if (field.CursorPos < (int)field.EditBuffer.length())
      field.CursorPos++;
    if (!(event->modifiers() & Qt::ShiftModifier))
      field.SelectionStart = field.CursorPos;
    field.SelectionEnd = field.CursorPos;
    ensureCursorVisible(field);
  } else if (event->matches(QKeySequence::SelectAll)) {
    field.SelectionStart = 0;
    field.SelectionEnd = (int)field.EditBuffer.length();
    field.CursorPos = field.SelectionEnd;
  } else if (event->matches(QKeySequence::Copy)) {
    int start = std::min(field.SelectionStart, field.SelectionEnd);
    int end = std::max(field.SelectionStart, field.SelectionEnd);
    std::string text = (start == end)
                           ? field.EditBuffer
                           : field.EditBuffer.substr(start, end - start);
    QGuiApplication::clipboard()->setText(QString::fromStdString(text));
  } else if (event->matches(QKeySequence::Cut)) {
    int start = std::min(field.SelectionStart, field.SelectionEnd);
    int end = std::max(field.SelectionStart, field.SelectionEnd);
    std::string text = (start == end)
                           ? field.EditBuffer
                           : field.EditBuffer.substr(start, end - start);
    QGuiApplication::clipboard()->setText(QString::fromStdString(text));
    if (start != end) {
      field.EditBuffer.erase(start, end - start);
      field.CursorPos = start;
      field.SelectionStart = field.SelectionEnd = field.CursorPos;
      changed = true;
    }
  } else if (event->matches(QKeySequence::Paste)) {
    std::string text = QGuiApplication::clipboard()->text().toStdString();
    int start = std::min(field.SelectionStart, field.SelectionEnd);
    int end = std::max(field.SelectionStart, field.SelectionEnd);
    if (start == end) {
      field.EditBuffer = text;
      field.CursorPos = (int)text.length();
    } else {
      field.EditBuffer.replace(start, end - start, text);
      field.CursorPos = start + (int)text.length();
    }
    field.SelectionStart = field.SelectionEnd = field.CursorPos;
    changed = true;
  } else {
    QString txt = event->text();
    if (!txt.isEmpty() && txt[0].isPrint()) {
      int start = std::min(field.SelectionStart, field.SelectionEnd);
      int end = std::max(field.SelectionStart, field.SelectionEnd);
      field.EditBuffer.replace(start, end - start, txt.toStdString());
      field.CursorPos = start + 1;
      field.SelectionStart = field.SelectionEnd = field.CursorPos;
      changed = true;
    }
  }

  if (changed) {
    ensureCursorVisible(field);
    ApplyChanges(field);
    update();
  }
  return;
}

void PropertiesWidget::resizeEvent(QResizeEvent *event) {
  int y = 0;
  for (auto &field : m_Fields) {
    field.NameRect = QRect(0, y, NameWidth, RowHeight);
    field.ValueRect = QRect(NameWidth, y, width() - NameWidth, RowHeight);

    if (field.Type == PropertyType::Vec3) {
      int subWidth = (width() - NameWidth) / 3;
      for (int i = 0; i < 3; ++i) {
        field.SubRects[i] =
            QRect(NameWidth + i * subWidth, y, subWidth, RowHeight);
      }
    }
    y += RowHeight;
  }
}

void PropertiesWidget::wheelEvent(QWheelEvent *event) {}

void PropertiesWidget::contextMenuEvent(QContextMenuEvent *event) {
  for (auto &field : m_Fields) {
    QRect targetRect = field.ValueRect;
    int subIndex = -1;
    if (field.Type == PropertyType::Vec3) {
      for (int i = 0; i < 3; ++i) {
        if (field.SubRects[i].contains(event->pos())) {
          targetRect = field.SubRects[i];
          subIndex = i;
          break;
        }
      }
    }

    if (targetRect.contains(event->pos())) {
      if (field.Type == PropertyType::Vec3 &&
          field.EditingSubIndex != subIndex) {
        // Switch to the component under the mouse if it's not the current one
        field.EditingSubIndex = subIndex;
        glm::vec3 v = field.GetVec3();
        float val = (subIndex == 0) ? v.x : (subIndex == 1) ? v.y : v.z;
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << val;
        field.EditBuffer = ss.str();
        field.SelectionStart = 0;
        field.SelectionEnd = (int)field.EditBuffer.length();
        field.CursorPos = field.SelectionEnd;
      }

      QMenu menu(this);
      QAction *cutAct = menu.addAction("Cut");
      QAction *copyAct = menu.addAction("Copy");
      QAction *pasteAct = menu.addAction("Paste");
      menu.addSeparator();
      QAction *selAllAct = menu.addAction("Select All");

      QAction *selected = menu.exec(event->globalPos());
      if (selected == cutAct) {
        QKeyEvent keyEvent(QEvent::KeyPress, Qt::Key_X, Qt::ControlModifier);
        keyPressEvent(&keyEvent);
      } else if (selected == copyAct) {
        QKeyEvent keyEvent(QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier);
        keyPressEvent(&keyEvent);
      } else if (selected == pasteAct) {
        QKeyEvent keyEvent(QEvent::KeyPress, Qt::Key_V, Qt::ControlModifier);
        keyPressEvent(&keyEvent);
      } else if (selected == selAllAct) {
        QKeyEvent keyEvent(QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier);
        keyPressEvent(&keyEvent);
      }
      return;
    }
  }
}

void PropertiesWidget::OnCursorTimer() {
  m_CursorVisible = !m_CursorVisible;
  update();
}

void PropertiesWidget::ApplyChanges(PropertyField &field) {
  try {
    std::string buffer = field.EditBuffer;
    bool isNumeric =
        (field.Type == PropertyType::Float || field.Type == PropertyType::Int ||
         field.Type == PropertyType::Vec3);

    if (isNumeric && buffer.empty()) {
      buffer = "0";
    }

    if (field.Type == PropertyType::String)
      field.SetString(field.EditBuffer);
    else if (field.Type == PropertyType::Float)
      field.SetFloat(std::stof(buffer));
    else if (field.Type == PropertyType::Int)
      field.SetInt(std::stoi(buffer));
    else if (field.Type == PropertyType::Vec3 && field.EditingSubIndex != -1) {
      glm::vec3 v = field.GetVec3();
      float val = std::stof(buffer);
      if (field.EditingSubIndex == 0)
        v.x = val;
      else if (field.EditingSubIndex == 1)
        v.y = val;
      else
        v.z = val;
      field.SetVec3(v);
    }
  } catch (...) {
  }
}

int PropertiesWidget::posToIndex(PropertyField &field, int x) {
  QFontMetrics fm(font());
  int relX = 0;
  if (field.Type == PropertyType::Vec3 && field.EditingSubIndex != -1) {
    relX = x - field.SubRects[field.EditingSubIndex].left() - 12 +
           field.SubScrollX[field.EditingSubIndex];
  } else {
    relX = x - field.ValueRect.left() - Padding + field.ScrollX;
  }

  std::string s;
  if (field.IsEditing)
    s = field.EditBuffer;
  else {
    if (field.Type == PropertyType::String)
      s = field.GetString();
    else if (field.Type == PropertyType::Float) {
      std::stringstream ss;
      ss << std::fixed << std::setprecision(2) << field.GetFloat();
      s = ss.str();
    } else if (field.Type == PropertyType::Int)
      s = std::to_string(field.GetInt());
    else if (field.Type == PropertyType::Vec3 && field.EditingSubIndex != -1) {
      glm::vec3 v = field.GetVec3();
      float val = (field.EditingSubIndex == 0)   ? v.x
                  : (field.EditingSubIndex == 1) ? v.y
                                                 : v.z;
      std::stringstream ss;
      ss << std::fixed << std::setprecision(2) << val;
      s = ss.str();
    }
  }

  for (int i = 0; i < (int)s.length(); ++i) {
    int w = fm.horizontalAdvance(QString::fromStdString(s.substr(0, i + 1)));
    if (relX <
        w - fm.horizontalAdvance(QString::fromStdString(s.substr(i, 1))) / 2)
      return i;
  }
  return (int)s.length();
}

void PropertiesWidget::ensureCursorVisible(PropertyField &field) {
  QFontMetrics fm(font());
  int cursorX = fm.horizontalAdvance(
      QString::fromStdString(field.EditBuffer.substr(0, field.CursorPos)));

  if (field.Type == PropertyType::Vec3 && field.EditingSubIndex != -1) {
    int viewWidth = field.SubRects[field.EditingSubIndex].width() - 14;
    int &scrollX = field.SubScrollX[field.EditingSubIndex];
    if (cursorX - scrollX < 0)
      scrollX = cursorX;
    else if (cursorX - scrollX > viewWidth)
      scrollX = cursorX - viewWidth;
    if (scrollX < 0)
      scrollX = 0;
  } else {
    int viewWidth = field.ValueRect.width() - Padding * 2;
    if (cursorX - field.ScrollX < 0)
      field.ScrollX = cursorX;
    else if (cursorX - field.ScrollX > viewWidth)
      field.ScrollX = cursorX - viewWidth;
    if (field.ScrollX < 0)
      field.ScrollX = 0;
  }
}
