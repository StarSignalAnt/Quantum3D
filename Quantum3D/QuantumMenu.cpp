#include "QuantumMenu.h"
#include "../QuantumEngine/CameraNode.h"
#include "../QuantumEngine/GraphNode.h"
#include "../QuantumEngine/SceneSerializer.h"
#include "BrowserWidget.h"
#include "EngineGlobals.h"
#include "SceneGraphWidget.h"
#include "ScriptEditorWindow.h"
#include "ViewportWidget.h"
#include "stdafx.h"
#include <QFileDialog>
#include <QMessageBox>

// Clipboard for node copy/paste
static std::shared_ptr<Quantum::GraphNode> s_ClipboardNode = nullptr;

QuantumMenu::QuantumMenu(QWidget *parent) : QMenuBar(parent) { setupMenus(); }

QuantumMenu::~QuantumMenu() {}

void QuantumMenu::setupMenus() {
  // File Menu
  m_fileMenu = addMenu(tr("&File"));

  // Open Scene
  m_openSceneAction = m_fileMenu->addAction(tr("&Open Scene..."));
  m_openSceneAction->setShortcut(QKeySequence::Open);
  connect(m_openSceneAction, &QAction::triggered, []() {
    if (!EngineGlobals::EditorScene)
      return;

    // Get default directory from browser widget
    QString defaultDir = ".";
    if (EngineGlobals::BrowserPanel) {
      defaultDir =
          QString::fromStdString(EngineGlobals::BrowserPanel->GetCurrentPath());
    }

    QString filepath = QFileDialog::getOpenFileName(
        nullptr, tr("Open Scene"), defaultDir, tr("Scene Files (*.graph)"));

    if (filepath.isEmpty())
      return;

    // Get content root
    std::string contentRoot = ".";
    if (EngineGlobals::BrowserPanel) {
      contentRoot = EngineGlobals::BrowserPanel->GetContentRoot();
    }

    // Get Vulkan device and QLang domain
    Vivid::VividDevice *device = nullptr;
    if (EngineGlobals::Viewport) {
      device = EngineGlobals::Viewport->GetDevice();
    }

    QLangDomain *domain = EngineGlobals::m_QDomain.get();

    Quantum::SceneSerializer::LoadedCameraState cameraState;
    if (Quantum::SceneSerializer::Load(*EngineGlobals::EditorScene,
                                       filepath.toStdString(), contentRoot,
                                       device, domain, &cameraState)) {
      // Refresh UI
      if (EngineGlobals::SceneGraphPanel) {
        EngineGlobals::SceneGraphPanel->RefreshTree();
      }
      // Refresh material descriptor sets for newly loaded meshes
      if (EngineGlobals::Viewport) {
        EngineGlobals::Viewport->RefreshMaterials();
      }
      // Sync editor camera rotation with loaded yaw/pitch
      if (EngineGlobals::Viewport && cameraState.hasData) {
        EngineGlobals::Viewport->SetEditorCameraRotation(cameraState.pitch,
                                                         cameraState.yaw);
      }
    } else {
      QMessageBox::warning(nullptr, tr("Open Scene"),
                           tr("Failed to open scene file."));
    }
  });

  // Save Scene
  m_saveSceneAction = m_fileMenu->addAction(tr("&Save Scene..."));
  m_saveSceneAction->setShortcut(QKeySequence::Save);
  connect(m_saveSceneAction, &QAction::triggered, []() {
    if (!EngineGlobals::EditorScene)
      return;

    // Get default directory from browser widget
    QString defaultDir = ".";
    if (EngineGlobals::BrowserPanel) {
      defaultDir =
          QString::fromStdString(EngineGlobals::BrowserPanel->GetCurrentPath());
    }

    QString filepath = QFileDialog::getSaveFileName(
        nullptr, tr("Save Scene"), defaultDir, tr("Scene Files (*.graph)"));

    if (filepath.isEmpty())
      return;

    // Ensure .graph extension
    if (!filepath.endsWith(".graph", Qt::CaseInsensitive)) {
      filepath += ".graph";
    }

    // Get content root
    std::string contentRoot = ".";
    if (EngineGlobals::BrowserPanel) {
      contentRoot = EngineGlobals::BrowserPanel->GetContentRoot();
    }

    // Get editor camera yaw/pitch from viewport
    float editorYaw = 0.0f, editorPitch = 0.0f;
    if (EngineGlobals::Viewport) {
      EngineGlobals::Viewport->GetEditorCameraRotation(editorPitch, editorYaw);
    }

    if (Quantum::SceneSerializer::Save(*EngineGlobals::EditorScene,
                                       filepath.toStdString(), contentRoot,
                                       editorYaw, editorPitch)) {
      // Success
    } else {
      QMessageBox::warning(nullptr, tr("Save Scene"),
                           tr("Failed to save scene file."));
    }
  });

  m_fileMenu->addSeparator();

  // New Scene
  m_newSceneAction = m_fileMenu->addAction(tr("&New Scene"));
  m_newSceneAction->setShortcut(QKeySequence::New);
  connect(m_newSceneAction, &QAction::triggered, []() {
    if (EngineGlobals::EditorScene) {
      EngineGlobals::SetSelectedNode(nullptr);
      EngineGlobals::EditorScene->Clear();
      if (EngineGlobals::SceneGraphPanel)
        EngineGlobals::SceneGraphPanel->RefreshTree();
      if (EngineGlobals::Viewport)
        EngineGlobals::Viewport->RefreshMaterials();
    }
  });

  // Open Scene
  m_openSceneAction = m_fileMenu->addAction(tr("&Open Scene..."));
  m_openSceneAction->setShortcut(QKeySequence::Open);
  // ... [Existing Open Scene Logic is preserved via context match, wait, I need
  // to preserve it] I should only insert New Scene before Open Scene. But I
  // also need Edit Menu actions. I will split this into multiple replacement
  // chunks or do a larger replace. I'll do multiple chunks.

  // Edit Menu
  m_editMenu = addMenu(tr("&Edit"));

  // Edit Menu Actions
  m_copyAction = m_editMenu->addAction(tr("&Copy Node"));
  m_copyAction->setShortcut(QKeySequence::Copy);
  connect(m_copyAction, &QAction::triggered, []() {
    if (EngineGlobals::SceneGraphPanel) {
      auto node = EngineGlobals::SceneGraphPanel->GetSelectedNode();
      if (node) {
        s_ClipboardNode = node->Clone();
      }
    }
  });

  m_pasteAction = m_editMenu->addAction(tr("&Paste Node"));
  m_pasteAction->setShortcut(QKeySequence::Paste);
  connect(m_pasteAction, &QAction::triggered, []() {
    if (s_ClipboardNode && EngineGlobals::EditorScene) {
      auto newNode = s_ClipboardNode->Clone();

      // Offset position slightly
      newNode->SetLocalPosition(newNode->GetLocalPosition() +
                                glm::vec3(1.0f, 0.0f, 1.0f));

      // Determine parent
      Quantum::GraphNode *parent = EngineGlobals::EditorScene->GetRoot();
      if (EngineGlobals::SceneGraphPanel) {
        auto selected = EngineGlobals::SceneGraphPanel->GetSelectedNode();
        if (selected && selected->GetParent()) {
          // Paste as sibling
          parent = selected->GetParent();
        } else if (selected) {
          // Selected is root? Only if user selected root.
          // Or paste as child of selected?
          // User said "paste would be a new graphnode... moved slightly".
          // Sibling is safest. If selected is root, paste as child.
          if (selected == EngineGlobals::EditorScene->GetRoot()) {
            parent = selected;
          } else {
            parent = selected->GetParent();
          }
        }
      }

      if (parent) {
        parent->AddChild(newNode);
        if (EngineGlobals::SceneGraphPanel)
          EngineGlobals::SceneGraphPanel->RefreshTree();
      }
    }
  });

  m_editMenu->addSeparator();

  m_alignNodeToCamAction = m_editMenu->addAction(tr("Align &Node to Camera"));
  connect(m_alignNodeToCamAction, &QAction::triggered, []() {
    if (EngineGlobals::SceneGraphPanel && EngineGlobals::Viewport) {
      auto node = EngineGlobals::SceneGraphPanel->GetSelectedNode();
      if (node) {
        // Get View Matrix (which is Inverse of World Matrix)
        glm::mat4 viewMat =
            EngineGlobals::Viewport->GetEditorCameraViewMatrix();
        // Invert it to get the actual World Transform of the Camera
        glm::mat4 camMat = glm::inverse(viewMat);

        glm::vec3 camPos = glm::vec3(camMat[3]);

        // Extract pure View Rotation (remove any scale if present)
        glm::vec3 right = glm::normalize(glm::vec3(camMat[0]));
        glm::vec3 up = glm::normalize(glm::vec3(camMat[1]));
        glm::vec3 fwd = glm::normalize(glm::vec3(camMat[2]));

        glm::mat4 camRotMat(1.0f);
        camRotMat[0] = glm::vec4(right, 0.0f);
        camRotMat[1] = glm::vec4(up, 0.0f);
        camRotMat[2] = glm::vec4(fwd, 0.0f);

        // Apply 180 degree rotation to align Node's +Z with Camera's -Z (View
        // Dir)
        camRotMat = glm::rotate(camRotMat, glm::radians(180.0f),
                                glm::vec3(0.0f, 1.0f, 0.0f));

        if (node->GetParent()) {
          glm::mat4 parentMat = node->GetParent()->GetWorldMatrix();
          glm::mat4 parentInv = glm::inverse(parentMat);

          // 1. Position: Transform World Pos to Local Pos (handles parent
          // scale/rot/pos)
          glm::vec3 localPos = glm::vec3(parentInv * glm::vec4(camPos, 1.0f));
          node->SetLocalPosition(localPos);

          // 2. Rotation: Remove Scale from Parent Matrix for pure rotation calc
          glm::vec3 pRight = glm::normalize(glm::vec3(parentMat[0]));
          glm::vec3 pUp = glm::normalize(glm::vec3(parentMat[1]));
          glm::vec3 pFwd = glm::normalize(glm::vec3(parentMat[2]));

          glm::mat4 parentRotMat(1.0f);
          parentRotMat[0] = glm::vec4(pRight, 0.0f);
          parentRotMat[1] = glm::vec4(pUp, 0.0f);
          parentRotMat[2] = glm::vec4(pFwd, 0.0f);

          // LocalRot = Inv(ParentRot) * TargetWorldRot
          // Note: Inv(Rot) == Transpose(Rot)
          glm::mat4 localRot = glm::transpose(parentRotMat) * camRotMat;

          node->SetLocalRotation(localRot);
        } else {
          node->SetLocalPosition(camPos);
          node->SetLocalRotation(camRotMat);
        }
      }
    }
  });

  m_alignCamToNodeAction = m_editMenu->addAction(tr("Align &Camera to Node"));
  connect(m_alignCamToNodeAction, &QAction::triggered, []() {
    if (EngineGlobals::SceneGraphPanel && EngineGlobals::Viewport) {
      auto node = EngineGlobals::SceneGraphPanel->GetSelectedNode();
      if (node) {
        glm::mat4 nodeMat = node->GetWorldMatrix();
        glm::vec3 pos = glm::vec3(nodeMat[3]);

        // Extract pure Node Rotation
        glm::vec3 forward = glm::normalize(glm::vec3(nodeMat[2])); // Z axis
        glm::vec3 right = glm::normalize(glm::vec3(nodeMat[0]));
        glm::vec3 up = glm::normalize(glm::vec3(nodeMat[1]));

        glm::mat4 rotMat(1.0f);
        rotMat[0] = glm::vec4(right, 0.0f);
        rotMat[1] = glm::vec4(up, 0.0f);
        rotMat[2] = glm::vec4(forward, 0.0f);

        // Apply 180 flip (Rotate around Y)
        rotMat = glm::rotate(rotMat, glm::radians(180.0f),
                             glm::vec3(0.0f, 1.0f, 0.0f));

        // Decompose to Euler YXZ (Yaw, Pitch) from Forward (Col 2)
        // M[2][1] = -Sin(Pitch)
        float pitch = -std::asin(rotMat[2][1]);
        float yaw = std::atan2(rotMat[2][0], rotMat[2][2]);

        EngineGlobals::Viewport->SetEditorCameraPosition(pos);
        EngineGlobals::Viewport->SetEditorCameraRotation(pitch, yaw);
      }
    }
  });

  // View Menu
  m_viewMenu = addMenu(tr("&View"));

  // Tools Menu
  m_toolsMenu = addMenu(tr("&Tools"));
  m_scriptEditorAction = m_toolsMenu->addAction(tr("&Script Editor"));

  // Connect via EngineGlobals
  connect(m_scriptEditorAction, &QAction::triggered, []() {
    if (EngineGlobals::ScriptEditor) {
      EngineGlobals::ScriptEditor->show();
      EngineGlobals::ScriptEditor->raise();
      EngineGlobals::ScriptEditor->activateWindow();
    }
  });

  // Help Menu
  m_helpMenu = addMenu(tr("&Help"));
}
