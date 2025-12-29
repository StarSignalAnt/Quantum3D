void Quantum3D::updateApp() {
  static float lastTime = 0.0f;
  // Simple dt calculation (placeholder)
  float dt = 0.016f;

  if (EngineGlobals::EditorScene) {
    EngineGlobals::EditorScene->Update(dt);
  }

  // Also request update of viewport if we are animating
  if (m_viewportWidget) {
    m_viewportWidget->update();
  }
}
