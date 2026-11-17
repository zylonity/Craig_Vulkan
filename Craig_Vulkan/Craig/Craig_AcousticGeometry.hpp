#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

namespace Craig {

	struct Model;
	class GameObject;

	//One model's triangles with its glTF node transforms baked in, but not the object's own transform
	//Both the BLASes and the Steam Audio sub-scenes get built from this, so the GPU and CPU trace the exact same triangles
	struct AcousticMesh {
		std::vector<std::array<float, 3>> positions;
		std::vector<uint32_t> indices;
		uint32_t triangleCount() const { return static_cast<uint32_t>(indices.size() / 3); }
	};

	//Walks the model's nodes the same way drawNode does
	AcousticMesh collectAcousticMesh(const Model& model);

	//A scale of 0 (or NaNs) would squash the instance flat, so those get skipped
	bool validAcousticTransform(const glm::mat4& transform);

	//Has a loaded model and is marked as something sound can hit
	bool isAcousticObject(GameObject& object);

}
