#include "Craig_ResourceManager.hpp"
#include "../External/tiny_obj_loader.h"
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
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;
    std::string warn;

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, m_testModel.modelPath.c_str());

    if (!warn.empty()) {
        std::cout << warn << std::endl;
    }

    if (!err.empty()) {
        std::cerr << err << std::endl;
    }

    if (!ret) {
        exit(CRAIG_FAIL);
    }

    int i = 0;
    for (const auto& shape : shapes) {
        i++;
        SubMesh* tempMesh = new SubMesh();

        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};



            vertex.m_pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            if (index.texcoord_index >= 0) {
                vertex.m_texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }



            vertex.m_color = { 1.0f, 1.0f, 1.0f };

            //TODO: unique vertex loading

            tempMesh->m_vertices.push_back(vertex);
            tempMesh->m_indices.push_back(tempMesh->m_indices.size());
        }

        m_testModel.subMeshes.push_back(tempMesh);
    }

    m_testModel.subMeshesCount = i;

}
