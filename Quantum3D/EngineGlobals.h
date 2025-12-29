// EngineGlobals.h
#pragma once
#include "QLangDomain.h"
#include <memory>

// Forward declarations
namespace Quantum {
class SceneGraph;
class GraphNode;
class SceneRenderer;
} // namespace Quantum

class ViewportWidget;
class SceneGraphWidget;
class PropertiesWidget;
class BrowserWidget;
class ConsoleWidget;
class TerrainEditorWidget;

namespace Quantum {
class ScriptEditorWindow;
}

// Editor Mode
#include "EditorCommon.h"

// Editor Mode
#include "EditorCommon.h"

class EngineGlobals {
public:
  // === Scene State ===
  static std::shared_ptr<Quantum::SceneGraph> EditorScene;
  static void *VulkanDevice;

  // === Selection State ===
  static std::weak_ptr<Quantum::GraphNode> SelectedNode;

  static std::shared_ptr<QLangDomain> m_QDomain;

  // === UI Components ===
  static ViewportWidget *Viewport;
  static SceneGraphWidget *SceneGraphPanel;
  static PropertiesWidget *PropertiesPanel;
  static BrowserWidget *BrowserPanel;
  static ConsoleWidget *Console;
  static Quantum::ScriptEditorWindow *ScriptEditor;
  static TerrainEditorWidget *TerrainEditor;
  static Quantum::SceneRenderer *Renderer;

  // === Gizmo State ===
  static CoordinateSpace CurrentSpace;
  static GizmoType CurrentGizmoType;
  static Quantum::EditorMode CurrentEditorMode;

  // === Selection Functions ===
  static void SetSelectedNode(std::shared_ptr<Quantum::GraphNode> node);
  static std::shared_ptr<Quantum::GraphNode> GetSelectedNode();
  static void ClearSelection();

  // === Gizmo Functions ===
  static void SetSpace(CoordinateSpace space);
  static CoordinateSpace GetSpace();

  static void SetGizmoMode(GizmoType type);
  static GizmoType GetGizmoMode();

  static void SetEditorMode(Quantum::EditorMode mode);
  static Quantum::EditorMode GetEditorMode();

  static void OnPlay();
  static void OnStop();
  static void OnUpdate(float dt);

  static bool m_Playing;
};
