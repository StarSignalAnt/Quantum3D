// EngineGlobals.cpp
#include "EngineGlobals.h"
#include "stdafx.h"

std::shared_ptr<Quantum::SceneGraph> EngineGlobals::EditorScene = nullptr;
void *EngineGlobals::VulkanDevice = nullptr;
