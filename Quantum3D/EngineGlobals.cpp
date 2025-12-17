// EngineGlobals.cpp
#include "EngineGlobals.h"
#include "stdafx.h"
#include <iostream>

// === Static Member Initialization ===

// Scene State
std::shared_ptr<Quantum::SceneGraph> EngineGlobals::EditorScene = nullptr;
void *EngineGlobals::VulkanDevice = nullptr;

// Selection State
std::weak_ptr<Quantum::GraphNode> EngineGlobals::SelectedNode;

// UI Components
ViewportWidget *EngineGlobals::Viewport = nullptr;
SceneGraphWidget *EngineGlobals::SceneGraphPanel = nullptr;
PropertiesWidget *EngineGlobals::PropertiesPanel = nullptr;
BrowserWidget *EngineGlobals::BrowserPanel = nullptr;

// Interaction Mode
InteractionMode EngineGlobals::CurrentInteractionMode =
    InteractionMode::Translate;

// === Functions ===

void EngineGlobals::SetSelectedNode(std::shared_ptr<Quantum::GraphNode> node) {
  SelectedNode = node;

  if (node) {
    std::cout << "[EngineGlobals] Selected node set" << std::endl;
  } else {
    std::cout << "[EngineGlobals] Selection cleared" << std::endl;
  }

  // TODO: Notify UI components of selection change if needed
  // e.g., PropertiesPanel->OnSelectionChanged(node);
}

std::shared_ptr<Quantum::GraphNode> EngineGlobals::GetSelectedNode() {
  return SelectedNode.lock();
}

void EngineGlobals::ClearSelection() { SetSelectedNode(nullptr); }

void EngineGlobals::SetInteractionMode(InteractionMode mode) {
  CurrentInteractionMode = mode;

  const char *modeName = "Unknown";
  switch (mode) {
  case InteractionMode::Translate:
    modeName = "Translate";
    break;
  case InteractionMode::Rotate:
    modeName = "Rotate";
    break;
  case InteractionMode::Scale:
    modeName = "Scale";
    break;
  }

  std::cout << "[EngineGlobals] Interaction mode: " << modeName << std::endl;

  // TODO: Notify viewport/gizmo system of mode change if needed
}

InteractionMode EngineGlobals::GetInteractionMode() {
  return CurrentInteractionMode;
}
