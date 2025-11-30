#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "Craig_ResourceManager.hpp"
#include "Craig_Renderer.hpp"
#include "../External/tiny_gltf.h"
#include <iostream>

vk::VertexInputBindingDescription Craig::Vertex::getBindingDescription() {
    vk::VertexInputBindingDescription bindingDescription;

    bindingDescription
        .setBinding(0)
        .setStride(sizeof(Craig::Vertex))
        .setInputRate(vk::VertexInputRate::eVertex);


    return bindingDescription;
}

std::array<vk::VertexInputAttributeDescription, 3> Craig::Vertex::getAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions;

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
        .setFormat(vk::Format::eR32G32Sfloat)
        .setOffset(offsetof(Craig::Vertex, m_texCoord));


    return attributeDescriptions;
}

CraigError Craig::ResourceManager::init() {

    CraigError ret = CRAIG_SUCCESS;

    return ret;
}



CraigError Craig::ResourceManager::terminate() {

    CraigError ret = CRAIG_SUCCESS;

    return ret;
}

void Craig::ResourceManager::loadModel() {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, m_testModel.modelPath.c_str());
    // use LoadBinaryFromFile for .glb

    if (!warn.empty()) {
        std::cout << warn << std::endl;
    }
    if (!err.empty()) {
        std::cerr << err << std::endl;
    }
    if (!ret) {
        exit(CRAIG_FAIL);
    }
    else {
        printf("model found");
    }

    int i = 0;
    // iterate all meshes / primitives, no scene graph yet
    for (const auto& mesh : model.meshes) {
        SubMesh* tempMesh = new SubMesh();
        i++;

        for (const auto& prim : mesh.primitives) { //in gltf a primitive is a draw call, we can have multiple draw calls for like different layer textures

            //INDICES STUFF
            if (prim.indices < 0) {
                // you *can* support non-indexed later, skip for now
                continue;
            }

            const tinygltf::Accessor& indexAccessor = model.accessors[prim.indices];
            const tinygltf::BufferView& indexBV = model.bufferViews[indexAccessor.bufferView];
            const tinygltf::Buffer& indexBuf = model.buffers[indexBV.buffer];

            const uint8_t* indexData = indexBuf.data.data() + indexBV.byteOffset + indexAccessor.byteOffset; //pointer to where the index data starts, offset by the other things


            //POSITION STUFF
            auto itPos = prim.attributes.find("POSITION");
            if (itPos == prim.attributes.end()) {
                continue; // no positions means we can skip the primitive
            }

            const tinygltf::Accessor& posAccessor = model.accessors[itPos->second];
            const tinygltf::BufferView& posBV = model.bufferViews[posAccessor.bufferView];
            const tinygltf::Buffer& posBuf = model.buffers[posBV.buffer];

            const uint8_t* posData = posBuf.data.data() + posBV.byteOffset + posAccessor.byteOffset; //pointer to where the position data starts, offset by the other things

            // position stride - From what I understand, it's how much forward in memory (how many bits) we need to move before finding the next vertex
            // oxford dictionary: Stride - walk with long, decisive steps in a specified direction.
            size_t posStride = tinygltf::GetNumComponentsInType(posAccessor.type) * tinygltf::GetComponentSizeInBytes(posAccessor.componentType);
            if (posAccessor.ByteStride(posBV) != 0) {
                posStride = posAccessor.ByteStride(posBV);
            }

            //TEXCOORD STUFF
            const tinygltf::Accessor* texAccessor = nullptr;
            const tinygltf::BufferView* texBV = nullptr;
            const tinygltf::Buffer* texBuf = nullptr;
            const uint8_t* texData = nullptr;
            size_t texStride = 0;

            auto itUv = prim.attributes.find("TEXCOORD_0");
            if (itUv != prim.attributes.end()) {
                texAccessor = &model.accessors[itUv->second];
                texBV = &model.bufferViews[texAccessor->bufferView];
                texBuf = &model.buffers[texBV->buffer];

                texData = texBuf->data.data() + texBV->byteOffset + texAccessor->byteOffset;

                texStride = tinygltf::GetNumComponentsInType(texAccessor->type) * tinygltf::GetComponentSizeInBytes(texAccessor->componentType);
                if (texAccessor->ByteStride(*texBV) != 0) {
                    texStride = texAccessor->ByteStride(*texBV);
                }
            }

            uint32_t firstVertex = (uint32_t)tempMesh->m_vertices.size();
            uint32_t firstIndex = (uint32_t)tempMesh->m_indices.size();

            //GET VERTICES
            for (size_t i = 0; i < posAccessor.count; ++i) {
                Vertex v{};

                const float* p = reinterpret_cast<const float*>(posData + i * posStride);
                v.m_pos = glm::vec3(p[0], p[1], p[2]);

                if (texAccessor) {
                    const float* t = reinterpret_cast<const float*>(texData + i * texStride);
                    v.m_texCoord = glm::vec2(t[0], t[1]);
                }
                else {
                    v.m_texCoord = glm::vec2(0.0f);
                }

                v.m_color = glm::vec3(1.0f);

                tempMesh->m_vertices.push_back(v);
            }

            //GET INDICES
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

                tempMesh->m_indices.push_back(firstVertex + index);
            }

            uint32_t indexCount = (uint32_t)tempMesh->m_indices.size() - firstIndex;

            tempMesh->firstVertex = firstVertex;
            tempMesh->firstIndex = firstIndex;
            tempMesh->indexCount = indexCount;
            tempMesh->materialIndex = prim.material;


            //GET TEXTURE
            int materialIndex = prim.material; //find the material
            if (materialIndex >= 0 && materialIndex < model.materials.size()) { //if we actually have a texutre
                const tinygltf::Material& mat = model.materials[materialIndex];

                // Base color texture (what you usually think of as albedo/diffuse)
                int baseColorTexIndex = mat.pbrMetallicRoughness.baseColorTexture.index;
                if (baseColorTexIndex >= 0 && baseColorTexIndex < model.textures.size()) {
                    const tinygltf::Texture& tex = model.textures[baseColorTexIndex];

                    int imageIndex = tex.source;
                    if (imageIndex >= 0 && imageIndex < model.images.size()) {
                        const tinygltf::Image& img = model.images[imageIndex];

                        const uint8_t* pixels = img.image.data();
                        int width = img.width;
                        int height = img.height;
                        int comp = img.component; // usually 4 (RGBA)

                        // Here you replace your old stb_image path:
                        // createVulkanTextureFromPixels(pixels, width, height, comp);
                        m_renderer->createTextureImage2(pixels, width, height, comp);
                    }
                }
            }

        }

        m_testModel.subMeshes.push_back(tempMesh);
    }

    m_testModel.subMeshesCount = i;
}

void Craig::ResourceManager::terminateModel() {

    for (size_t i = 0; i < m_testModel.subMeshes.size(); i++)
    {
        delete m_testModel.subMeshes[i];
        m_testModel.subMeshes[i] = nullptr;
    }
    m_testModel.subMeshes.clear();

}