// EngineGlobals.h
#pragma once
#include <memory>

// Forward declarations
namespace Quantum {
class SceneGraph;
class GraphNode;
} // namespace Quantum

class ViewportWidget;
class SceneGraphWidget;
class PropertiesWidget;
class BrowserWidget;

// Interaction mode for gizmos
enum class InteractionMode { Translate, Rotate, Scale };

class EngineGlobals {
public:
  // === Scene State ===
  static std::shared_ptr<Quantum::SceneGraph> EditorScene;
  static void *VulkanDevice;

  // === Selection State ===
  static std::weak_ptr<Quantum::GraphNode> SelectedNode;

  // === UI Components ===
  static ViewportWidget *Viewport;
  static SceneGraphWidget *SceneGraphPanel;
  static PropertiesWidget *PropertiesPanel;
  static BrowserWidget *BrowserPanel;

  // === Interaction Mode ===
  static InteractionMode CurrentInteractionMode;

  // === Functions ===

  // Set the selected node (triggers UI updates if needed)
  static void SetSelectedNode(std::shared_ptr<Quantum::GraphNode> node);

  // Get the currently selected node (may be null)
  static std::shared_ptr<Quantum::GraphNode> GetSelectedNode();

  // Clear the current selection
  static void ClearSelection();

  // Set the current interaction mode
  static void SetInteractionMode(InteractionMode mode);

  // Get the current interaction mode
  static InteractionMode GetInteractionMode();
};
