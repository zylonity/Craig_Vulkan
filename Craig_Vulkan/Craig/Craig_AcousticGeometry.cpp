#include "Craig_AcousticGeometry.hpp"
#include "Craig_ResourceManager.hpp"
#include "Craig_GameObject.hpp"
#include "Components/Craig_Model.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
	void collectNode(const Craig::Node& node, Craig::AcousticMesh& mesh) {
		//Same accumulated matrix the raster shader gets pushed
		const glm::mat4 nodeMatrix = node.getWorldMatrix();

		for (const Craig::SubMesh* pMesh : node.subMeshes) {
			if (pMesh->indexCount == 0) continue;

			if (pMesh->indexCount > pMesh->m_indices.size() || pMesh->indexCount % 3 != 0 ||
				mesh.positions.size() + pMesh->m_vertices.size() > std::numeric_limits<uint32_t>::max()) {
				throw std::runtime_error("invalid acoustic triangle list");
			}

			//Submesh indices start at 0, so they get moved up past everything already in the buffer
			const uint32_t baseVertex = static_cast<uint32_t>(mesh.positions.size());
			for (const Craig::Vertex& vertex : pMesh->m_vertices) {
				const glm::vec3 position(nodeMatrix * glm::vec4(vertex.m_pos, 1.0f));
				if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
					throw std::runtime_error("non-finite acoustic vertex");
				}
				mesh.positions.push_back({position.x, position.y, position.z});
			}
			for (uint32_t i = 0; i < pMesh->indexCount; i++) {
				if (pMesh->m_indices[i] >= pMesh->m_vertices.size()) {
					throw std::runtime_error("acoustic index outside vertex buffer");
				}
				mesh.indices.push_back(baseVertex + pMesh->m_indices[i]);
			}
		}

		for (const Craig::Node* pChild : node.children) {
			collectNode(*pChild, mesh);
		}
	}
}

Craig::AcousticMesh Craig::collectAcousticMesh(const Model& model) {
	AcousticMesh mesh;
	for (const Node* pNode : model.nodes) {
		collectNode(*pNode, mesh);
	}
	if (mesh.indices.size() / 3 > std::numeric_limits<uint32_t>::max()) {
		throw std::runtime_error("too many acoustic triangles");
	}
	return mesh;
}

bool Craig::validAcousticTransform(const glm::mat4& transform) {
	for (int column = 0; column < 4; column++) {
		for (int row = 0; row < 4; row++) {
			if (!std::isfinite(transform[column][row])) return false;
		}
	}
	const float determinant = glm::determinant(glm::mat3(transform));
	return std::isfinite(determinant) && determinant != 0.0f;
}

bool Craig::isAcousticObject(GameObject& object) {
	const auto* pModel = object.getComponent<Components::Model>();
	return pModel && pModel->hasModel() && pModel->isAcousticBlocker();
}
