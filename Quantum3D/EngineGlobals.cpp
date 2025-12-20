#define NOMINMAX
#include "EngineGlobals.h"
#include "../QuantumEngine/GraphNode.h"
#include "../QuantumEngine/QLangDomain.h"
#include "../QuantumEngine/SceneGraph.h"
#include "PropertiesWidget.h"
#include "SceneGraphWidget.h"
#include "ViewportWidget.h"
#include <iostream>

// === Static Member Initialization ===

// Scene State
std::shared_ptr<Quantum::SceneGraph> EngineGlobals::EditorScene = nullptr;
void *EngineGlobals::VulkanDevice = nullptr;

// Selection State
std::weak_ptr<Quantum::GraphNode> EngineGlobals::SelectedNode;

// QLang Domain
std::shared_ptr<QLangDomain> EngineGlobals::m_QDomain = nullptr;

// UI Components
ViewportWidget *EngineGlobals::Viewport = nullptr;
SceneGraphWidget *EngineGlobals::SceneGraphPanel = nullptr;
PropertiesWidget *EngineGlobals::PropertiesPanel = nullptr;
BrowserWidget *EngineGlobals::BrowserPanel = nullptr;
Quantum::ScriptEditorWindow *EngineGlobals::ScriptEditor = nullptr;

bool EngineGlobals::m_Playing = false;

// Gizmo State
CoordinateSpace EngineGlobals::CurrentSpace = CoordinateSpace::Local;
GizmoType EngineGlobals::CurrentGizmoType = GizmoType::Translate;

// === Selection Functions ===

void EngineGlobals::SetSelectedNode(std::shared_ptr<Quantum::GraphNode> node) {
  SelectedNode = node;

  if (node) {
    std::cout << "[EngineGlobals] Selected node set: " << node->GetName()
              << std::endl;
  } else {
    std::cout << "[EngineGlobals] Selection cleared" << std::endl;
  }

  // Update the viewport's gizmo for the selection
  // We call UpdateGizmoForSelection (not SetSelectedNode) to avoid infinite
  // loop
  if (Viewport) {
    Viewport->UpdateGizmoForSelection(node);
  }

  // Notify the scene graph panel to update its selection display
  if (SceneGraphPanel) {
    SceneGraphPanel->OnExternalSelectionChanged(node.get());
  }

  // Notify the properties panel
  if (PropertiesPanel) {
    PropertiesPanel->SetNode(node.get());
  }
}

std::shared_ptr<Quantum::GraphNode> EngineGlobals::GetSelectedNode() {
  return SelectedNode.lock();
}

void EngineGlobals::ClearSelection() { SetSelectedNode(nullptr); }

// === Gizmo Functions ===

void EngineGlobals::SetSpace(CoordinateSpace space) {
  CurrentSpace = space;

  const char *spaceName =
      (space == CoordinateSpace::Local) ? "Local" : "Global";
  std::cout << "[EngineGlobals] Coordinate space: " << spaceName << std::endl;

  // Update the viewport's gizmo
  if (Viewport) {
    Viewport->UpdateGizmoSpace();
  }
}

CoordinateSpace EngineGlobals::GetSpace() { return CurrentSpace; }

void EngineGlobals::SetGizmoMode(GizmoType type) {
  CurrentGizmoType = type;

  const char *typeName = "Unknown";
  switch (type) {
  case GizmoType::Translate:
    typeName = "Translate";
    break;
  case GizmoType::Rotate:
    typeName = "Rotate";
    break;
  case GizmoType::Scale:
    typeName = "Scale";
    break;
  }

  std::cout << "[EngineGlobals] Gizmo mode: " << typeName << std::endl;

  // Update the viewport's gizmo type
  if (Viewport) {
    Viewport->UpdateGizmoType();
  }
}

GizmoType EngineGlobals::GetGizmoMode() { return CurrentGizmoType; }

void EngineGlobals::OnPlay() {

  if (m_Playing)
    return;

  m_Playing = true;

  EditorScene->OnPlay();
}

void EngineGlobals::OnStop() {

  if (!m_Playing)
    return;

  m_Playing = false;

  EditorScene->OnStop();
}

void EngineGlobals::OnUpdate(float dt) {

  if (!m_Playing)
    return;
  EditorScene->OnUpdate(dt);
}