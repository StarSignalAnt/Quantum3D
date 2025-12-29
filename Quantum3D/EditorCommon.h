#pragma once

// Coordinate space for gizmo transformations
enum class CoordinateSpace { Local, Global };

// Interaction mode for gizmos
enum class GizmoType { None, Translate, Rotate, Scale };

namespace Quantum {

// Editor Mode
enum class EditorMode { Scene, Terrain };

} // namespace Quantum
