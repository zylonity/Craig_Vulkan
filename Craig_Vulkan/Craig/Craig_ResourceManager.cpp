#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "Craig_ResourceManager.hpp"
#include "Craig_Renderer.hpp"
#include "Craig_Logger.hpp"
#include "../External/tiny_gltf.h"
#include <glm/gtc/type_ptr.hpp>
#include <limits>

vk::VertexInputBindingDescription Craig::Vertex::getBindingDescription() {
    vk::VertexInputBindingDescription bindingDescription;

    bindingDescription
        .setBinding(0)
        .setStride(sizeof(Craig::Vertex))
        .setInputRate(vk::VertexInputRate::eVertex);


    return bindingDescription;
}

std::array<vk::VertexInputAttributeDescription, kVertexAttributeDescriptors> Craig::Vertex::getAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, kVertexAttributeDescriptors> attributeDescriptions;

    attributeDescriptions[0]
        .setBinding(0)
        .setLocation(0)//Location0 = POSITION0
        .setFormat(vk::Format::eR32G32B32Sfloat) //Not a colour, just uses the same format. Float2 = RG_float (Only 2 channels) 
        .setOffset(offsetof(Craig::Vertex, m_pos));

    attributeDescriptions[1]
        .setBinding(0)
        .setLocation(1)//Location1 = COLOR1 <- ps fuck american spelling.
        .setFormat(vk::Format::eR32G32B32Sfloat) //This time it IS a colour, so float3 = RGB_float
        .setOffset(offsetof(Craig::Vertex, m_color));

    attributeDescriptions[2]
        .setBinding(0)
        .setLocation(2)
        .setFormat(vk::Format::eR32G32B32Sfloat)
        .setOffset(offsetof(Craig::Vertex, m_normals));

    attributeDescriptions[3]
        .setBinding(0)
        .setLocation(3)
        .setFormat(vk::Format::eR32G32Sfloat)
        .setOffset(offsetof(Craig::Vertex, m_texCoord));




    return attributeDescriptions;
}

CraigError Craig::ResourceManager::init(Craig::Renderer* rendererToSet) {

    CraigError ret = CRAIG_SUCCESS;

    m_renderer = rendererToSet;

    return ret;
}



CraigError Craig::ResourceManager::terminate() {

    CraigError ret = CRAIG_SUCCESS;

    return ret;
}

// The glTF loading below is adapted from Sascha Willems' gltfloading example (MIT)
// https://github.com/SaschaWillems/Vulkan/blob/master/examples/gltfloading/gltfloading.cpp
// Copyright (C) 2020-2026 by Sascha Willems - www.saschawillems.de, license in External/SaschaWillems_LICENSE.md

glm::mat4 Craig::Node::getWorldMatrix() const {
    // Walk up to the top-most parent to get the final matrix
    glm::mat4 worldMatrix = matrix;
    const Node* currentParent = parent;
    while (currentParent) {
        worldMatrix = currentParent->matrix * worldMatrix;
        currentParent = currentParent->parent;
    }
    return worldMatrix;
}

const Craig::Material& Craig::Model::getMaterial(int32_t materialIndex) const {
    static const Craig::Material kDefaultMaterial{}; // White, no texture
    if (materialIndex < 0 || materialIndex >= (int32_t)materials.size()) {
        return kDefaultMaterial;
    }
    return materials[materialIndex];
}

Craig::Texture& Craig::Model::getMaterialImage(const Craig::Material& material) {
    // The fallback is always last, so the real images are 0 -> size - 2
    const int32_t textureIndex = material.baseColorTextureIndex;
    if (textureIndex >= 0 && textureIndex < (int32_t)textures.size()) {
        const int32_t imageIndex = textures[textureIndex].imageIndex;
        if (imageIndex >= 0 && imageIndex < (int32_t)images.size() - 1) {
            return images[imageIndex];
        }
    }
    return images.back();
}

// Grows min/max by every vertex under this node, then does the same for its children
static void expandBoundsByNode(const Craig::Node* node, glm::vec3& min, glm::vec3& max) {
    const glm::mat4 nodeMatrix = node->getWorldMatrix();
    for (const Craig::SubMesh* subMesh : node->subMeshes) {
        for (const Craig::Vertex& vertex : subMesh->m_vertices) {
            const glm::vec3 pos = glm::vec3(nodeMatrix * glm::vec4(vertex.m_pos, 1.0f));
            min = glm::min(min, pos);
            max = glm::max(max, pos);
        }
    }
    for (const Craig::Node* child : node->children) {
        expandBoundsByNode(child, min, max);
    }
}

bool Craig::Model::calculateBounds(glm::vec3& min, glm::vec3& max) const {
    // start inside out so the first vertex sets both
    min = glm::vec3(std::numeric_limits<float>::max());
    max = glm::vec3(std::numeric_limits<float>::lowest());
    for (const Craig::Node* node : nodes) {
        expandBoundsByNode(node, min, max);
    }
    return min.x <= max.x;
}

// Adds every vertex under this node to the list, then does the same for its children
static void collectPointsByNode(const Craig::Node* node, std::vector<glm::vec3>& outPoints) {
    const glm::mat4 nodeMatrix = node->getWorldMatrix();
    for (const Craig::SubMesh* subMesh : node->subMeshes) {
        for (const Craig::Vertex& vertex : subMesh->m_vertices) {
            outPoints.push_back(glm::vec3(nodeMatrix * glm::vec4(vertex.m_pos, 1.0f)));
        }
    }
    for (const Craig::Node* child : node->children) {
        collectPointsByNode(child, outPoints);
    }
}

void Craig::Model::collectPoints(std::vector<glm::vec3>& outPoints) const {
    for (const Craig::Node* node : nodes) {
        collectPointsByNode(node, outPoints);
    }
}

// images can be stored inside the glTF, so we grab them from tinygltf and upload them
static void loadImages(const tinygltf::Model& input, Craig::Model& outModel, Craig::Renderer* renderer) {
    static const uint8_t kWhitePixel[4] = { 255, 255, 255, 255 };

    outModel.images.resize(input.images.size() + 1);
    for (size_t i = 0; i < input.images.size(); i++) {
        const tinygltf::Image& glTFImage = input.images[i];

        // tinygltf gives us 8 bit RGBA by default, anything else gets the white pixel instead
        if (glTFImage.image.empty() || glTFImage.bits != 8 || glTFImage.component != 4) {
            Craig::Logger::resources().warn("Unsupported image {} in {}, using white instead", i, outModel.modelPath);
            renderer->createTextureImage2(kWhitePixel, 1, 1, 4, &outModel.images[i]);
            continue;
        }
        renderer->createTextureImage2(glTFImage.image.data(), glTFImage.width, glTFImage.height, glTFImage.component, &outModel.images[i]);
    }

    // fallback for primitives with no texture, so they don't bind garbage
    renderer->createTextureImage2(kWhitePixel, 1, 1, 4, &outModel.images.back());
}

static void loadTextures(const tinygltf::Model& input, Craig::Model& outModel) {
    outModel.textures.resize(input.textures.size());
    for (size_t i = 0; i < input.textures.size(); i++) {
        outModel.textures[i].imageIndex = input.textures[i].source;
    }
}

static void loadMaterials(const tinygltf::Model& input, Craig::Model& outModel) {
    outModel.materials.resize(input.materials.size());
    for (size_t i = 0; i < input.materials.size(); i++) {
        // Sascha reads these through material.values, newer tinygltf has them on pbrMetallicRoughness
        const tinygltf::PbrMetallicRoughness& pbr = input.materials[i].pbrMetallicRoughness;
        if (pbr.baseColorFactor.size() == 4) {
            outModel.materials[i].baseColorFactor = glm::vec4(glm::make_vec4(pbr.baseColorFactor.data()));
        }
        outModel.materials[i].baseColorTextureIndex = pbr.baseColorTexture.index;
    }
}

// pointer to where an accessor's data starts + how far to step per element
static const uint8_t* getAccessorData(const tinygltf::Model& input, const tinygltf::Accessor& accessor, size_t& outStride) {
    const tinygltf::BufferView& bufferView = input.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = input.buffers[bufferView.buffer];

    // stride - From what I understand, it's how much forward in memory (how many bits) we need to move before finding the next vertex
    // oxford dictionary: Stride - walk with long, decisive steps in a specified direction.
    outStride = tinygltf::GetNumComponentsInType(accessor.type) * tinygltf::GetComponentSizeInBytes(accessor.componentType);
    if (accessor.ByteStride(bufferView) != 0) {
        outStride = accessor.ByteStride(bufferView);
    }
    return buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
}

// one primitive = one draw call, returns nullptr if we can't draw it
static Craig::SubMesh* loadPrimitive(const tinygltf::Model& input, const tinygltf::Primitive& prim) {

    //INDICES STUFF
    if (prim.indices < 0) {
        // you *can* support non-indexed later, skip for now
        Craig::Logger::resources().warn("Skipping a primitive with no indices, non-indexed meshes aren't supported yet");
        return nullptr;
    }

    //POSITION STUFF
    auto itPos = prim.attributes.find("POSITION");
    if (itPos == prim.attributes.end()) {
        Craig::Logger::resources().warn("Skipping a primitive with no positions");
        return nullptr; // no positions mean we can skip the primitive
    }

    const tinygltf::Accessor& posAccessor = input.accessors[itPos->second];
    size_t posStride = 0;
    const uint8_t* posData = getAccessorData(input, posAccessor, posStride);

    //TEXCOORD STUFF
    const uint8_t* texData = nullptr;
    size_t texStride = 0;
    auto itUv = prim.attributes.find("TEXCOORD_0");
    if (itUv != prim.attributes.end()) {
        texData = getAccessorData(input, input.accessors[itUv->second], texStride);
    }

    //NORMALS STUFF
    const uint8_t* normData = nullptr;
    size_t normStride = 0;
    auto itNorm = prim.attributes.find("NORMAL");
    if (itNorm != prim.attributes.end()) {
        normData = getAccessorData(input, input.accessors[itNorm->second], normStride);
    }

    Craig::SubMesh* subMesh = new Craig::SubMesh();

    //GET VERTICES
    for (size_t i = 0; i < posAccessor.count; ++i) {
        Craig::Vertex v{};

        const float* p = reinterpret_cast<const float*>(posData + i * posStride);
        v.m_pos = glm::vec3(p[0], p[1], p[2]);

        if (texData) {
            const float* t = reinterpret_cast<const float*>(texData + i * texStride);
            v.m_texCoord = glm::vec2(t[0], t[1]);
        }

        if (normData) {
            const float* n = reinterpret_cast<const float*>(normData + i * normStride);
            v.m_normals = glm::vec3(n[0], n[1], n[2]);
        }

        v.m_color = glm::vec3(1.0f);

        subMesh->m_vertices.push_back(v);
    }

    //GET INDICES
    const tinygltf::Accessor& indexAccessor = input.accessors[prim.indices];
    size_t indexStride = 0;
    const uint8_t* indexData = getAccessorData(input, indexAccessor, indexStride);

    for (size_t i = 0; i < indexAccessor.count; ++i) {
        uint32_t index = 0;

        switch (indexAccessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            index = reinterpret_cast<const uint16_t*>(indexData)[i];
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            index = reinterpret_cast<const uint32_t*>(indexData)[i];
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            index = reinterpret_cast<const uint8_t*>(indexData)[i];
            break;
        default:
            // unsupported index type for now
            break;
        }

        subMesh->m_indices.push_back(index);
    }

    subMesh->indexCount = (uint32_t)subMesh->m_indices.size();
    subMesh->materialIndex = prim.material;

    return subMesh;
}

static void loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, Craig::Node* parent, Craig::Model& outModel) {
    Craig::Node* node = new Craig::Node{};
    node->parent = parent;

    // Get the local node matrix
    // It's either made up from translation, rotation, scale or a 4x4 matrix
    if (inputNode.translation.size() == 3) {
        node->matrix = glm::translate(node->matrix, glm::vec3(glm::make_vec3(inputNode.translation.data())));
    }
    if (inputNode.rotation.size() == 4) {
        glm::quat q = glm::quat(glm::make_quat(inputNode.rotation.data()));
        node->matrix *= glm::mat4_cast(q);
    }
    if (inputNode.scale.size() == 3) {
        node->matrix = glm::scale(node->matrix, glm::vec3(glm::make_vec3(inputNode.scale.data())));
    }
    if (inputNode.matrix.size() == 16) {
        node->matrix = glm::mat4(glm::make_mat4x4(inputNode.matrix.data()));
    }

    // Load node's children
    for (int childIndex : inputNode.children) {
        loadNode(input.nodes[childIndex], input, node, outModel);
    }

    // If the node has a mesh, load each of its primitives
    if (inputNode.mesh > -1) {
        for (const tinygltf::Primitive& prim : input.meshes[inputNode.mesh].primitives) {
            Craig::SubMesh* subMesh = loadPrimitive(input, prim);
            if (subMesh) {
                outModel.subMeshes.push_back(subMesh);
                node->subMeshes.push_back(subMesh);
            }
        }
    }

    if (parent) {
        parent->children.push_back(node);
    }
    else {
        outModel.nodes.push_back(node);
    }
}

bool Craig::ResourceManager::loadModel(std::string modelPath) {
    // If this model has already been loaded (e.g. a second GameObject using the
    // same glb), don't re-upload it. Doing so leaks the GPU texture and SubMesh
    // pointers because unordered_map::insert silently drops the duplicate key.
    if (m_loadedModels.find(modelPath) != m_loadedModels.end()) {
        Craig::Logger::resources().debug("{} is already loaded, reusing it", modelPath);
        return false;
    }

    const spdlog::stopwatch loadTimer;

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, modelPath.c_str());
    // use LoadBinaryFromFile for .glb

    if (!warn.empty()) {
        Craig::Logger::resources().warn("{}", warn);
    }
    if (!err.empty()) {
        Craig::Logger::resources().error("{}", err);
    }
    if (!ret) {
        Craig::Logger::resources().critical("Couldn't load {}, bailing out", modelPath);
        exit(CRAIG_FAIL);
    }
    Craig::Logger::resources().debug("Parsed {} in {:.1f} ms, uploading it", modelPath, loadTimer.elapsed().count() * 1000.0);

    // Insert first and fill it in place, saves copying all the vectors after
    Craig::Model& newModel = m_loadedModels[modelPath];
    newModel.modelPath = modelPath;

    loadImages(model, newModel, m_renderer);
    loadTextures(model, newModel);
    loadMaterials(model, newModel);

    // Load the default scene's top level nodes, their children get loaded recursively
    if (!model.scenes.empty()) {
        const tinygltf::Scene& scene = model.scenes[model.defaultScene > -1 ? model.defaultScene : 0];
        for (int nodeIndex : scene.nodes) {
            loadNode(model.nodes[nodeIndex], model, nullptr, newModel);
        }
    }
    else {
        Craig::Logger::resources().warn("{} has no scenes, nothing to draw", modelPath);
    }

    newModel.subMeshesCount = (uint32_t)newModel.subMeshes.size();

    size_t vertexCount = 0;
    size_t indexCount = 0;
    for (const Craig::SubMesh* subMesh : newModel.subMeshes) {
        vertexCount += subMesh->m_vertices.size();
        indexCount += subMesh->m_indices.size();
    }

    // images has the white fallback on the end, so one less
    Craig::Logger::resources().info("Loaded {}: {} submeshes, {} vertices, {} triangles, {} textures, {} materials in {:.1f} ms",
        modelPath, newModel.subMeshesCount, vertexCount, indexCount / 3, newModel.images.size() - 1, newModel.materials.size(), loadTimer.elapsed().count() * 1000.0);

    return true;
}

void Craig::ResourceManager::terminateModels(const vk::Device& device, const VmaAllocator& memoryAllocator) {

    Craig::Logger::resources().debug("Freeing {} model(s)", m_loadedModels.size());

    for (auto& modelPair : m_loadedModels)
    {
        Craig::Model& model = modelPair.second;
        for (size_t i = 0; i < model.subMeshes.size(); i++)
        {
            delete model.subMeshes[i];
            model.subMeshes[i] = nullptr;
        }
        model.subMeshes.clear();

        // nodes delete their own children
        for (Craig::Node* node : model.nodes)
        {
            delete node;
        }
        model.nodes.clear();

        // descriptor sets get freed with the pool, just the images here
        for (Craig::Texture& image : model.images)
        {
            device.destroyImageView(image.m_VK_textureImageView);
            vmaDestroyImage(memoryAllocator, image.m_VK_textureImage, image.m_VMA_textureImageAllocation);
        }
        model.images.clear();
    }

}
