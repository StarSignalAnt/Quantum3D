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

// Coordinate space for gizmo transformations
enum class CoordinateSpace { Local, Global };

// Interaction mode for gizmos
enum class GizmoType { Translate, Rotate, Scale };

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

  // === Gizmo State ===
  static CoordinateSpace CurrentSpace;
  static GizmoType CurrentGizmoType;

  // === Selection Functions ===
  static void SetSelectedNode(std::shared_ptr<Quantum::GraphNode> node);
  static std::shared_ptr<Quantum::GraphNode> GetSelectedNode();
  static void ClearSelection();

  // === Gizmo Functions ===
  static void SetSpace(CoordinateSpace space);
  static CoordinateSpace GetSpace();

  static void SetGizmoMode(GizmoType type);
  static GizmoType GetGizmoMode();
};
