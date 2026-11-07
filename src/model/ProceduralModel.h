#pragma once
#include "../assets/SceneAsset.h"

namespace claw {

// Builds the complete CLAW LAB scene from C++ primitive geometry.
// No Blender/glTF model is loaded at runtime.
SceneAsset buildProceduralScene();

} // namespace claw
