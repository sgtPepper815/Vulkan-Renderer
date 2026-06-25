#include "vulkanrenderer.h"
#include <QVulkanFunctions>
#include <QFile>
#include <QTextStream>
#include <QMatrix4x4>

VulkanRenderer::VulkanRenderer(QVulkanWindow* window)
    : m_window(window) {}

// ── Helper: Memory Type finden ────────────────────────────
uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    m_window->vulkanInstance()->functions()->vkGetPhysicalDeviceMemoryProperties(
        m_window->physicalDevice(), &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    qFatal("Kein geeigneter Memory Type gefunden");
    return 0;
}

// ── Helper: Buffer erstellen ──────────────────────────────
void VulkanRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                  VkMemoryPropertyFlags properties,
                                  VkBuffer& buffer, VkDeviceMemory& memory) {
    auto* df = m_window->vulkanInstance()->deviceFunctions(m_window->device());

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size        = size;
    bufferInfo.usage       = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    df->vkCreateBuffer(m_window->device(), &bufferInfo, nullptr, &buffer);

    VkMemoryRequirements memReqs;
    df->vkGetBufferMemoryRequirements(m_window->device(), buffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, properties);
    df->vkAllocateMemory(m_window->device(), &allocInfo, nullptr, &memory);
    df->vkBindBufferMemory(m_window->device(), buffer, memory, 0);
}

// ── OBJ Loader ────────────────────────────────────────────
void VulkanRenderer::loadMesh(const QString& path) {
    auto* df = m_window->vulkanInstance()->deviceFunctions(m_window->device());

    // GPU warten bis sie fertig ist
    df->vkDeviceWaitIdle(m_window->device());

    // Alte Buffer aufräumen
    if (m_vertexBuffer)       df->vkDestroyBuffer(m_window->device(), m_vertexBuffer, nullptr);
    if (m_vertexBufferMemory) df->vkFreeMemory(m_window->device(), m_vertexBufferMemory, nullptr);
    if (m_indexBuffer)        df->vkDestroyBuffer(m_window->device(), m_indexBuffer, nullptr);
    if (m_indexBufferMemory)  df->vkFreeMemory(m_window->device(), m_indexBufferMemory, nullptr);

    m_vertexBuffer       = VK_NULL_HANDLE;
    m_vertexBufferMemory = VK_NULL_HANDLE;
    m_indexBuffer        = VK_NULL_HANDLE;
    m_indexBufferMemory  = VK_NULL_HANDLE;

    // Neues Mesh laden
    loadObj(path);
}

void VulkanRenderer::loadObj(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        qFatal("OBJ nicht gefunden: %s", qPrintable(path));

    QVector<std::array<float,3>> positions;
    QVector<std::array<float,3>> normals;
    QVector<Vertex>              vertices;
    QVector<uint32_t>            indices;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.startsWith("v ")) {
            QStringList p = line.split(' ', Qt::SkipEmptyParts);
            positions.append({ p[1].toFloat(), p[2].toFloat(), p[3].toFloat() });
        } else if (line.startsWith("vn ")) {
            QStringList p = line.split(' ', Qt::SkipEmptyParts);
            normals.append({ p[1].toFloat(), p[2].toFloat(), p[3].toFloat() });
        } else if (line.startsWith("f ")) {
            QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            QVector<uint32_t> faceIndices;
            for (int i = 1; i < parts.size(); i++) {
                QStringList indices_str = parts[i].split('/');
                uint32_t posIdx = indices_str[0].toUInt() - 1;
                uint32_t nrmIdx = (indices_str.size() > 2 && !indices_str[2].isEmpty())
                                      ? indices_str[2].toUInt() - 1 : posIdx;

                Vertex v;
                v.pos[0]    = positions[posIdx][0];
                v.pos[1]    = positions[posIdx][1];
                v.pos[2]    = positions[posIdx][2];
                v.normal[0] = normals.isEmpty() ? 0.0f : normals[nrmIdx][0];
                v.normal[1] = normals.isEmpty() ? 0.0f : normals[nrmIdx][1];
                v.normal[2] = normals.isEmpty() ? 1.0f : normals[nrmIdx][2];

                faceIndices.append(vertices.size());
                vertices.append(v);
            }
            for (int i = 1; i + 1 < faceIndices.size(); i++) {
                indices.append(faceIndices[0]);
                indices.append(faceIndices[i]);
                indices.append(faceIndices[i+1]);
            }
        }
    }

    m_vertexCount = vertices.size();
    m_indexCount  = indices.size();

    // Buffers befüllen (gleich wie vorher)
    auto* df = m_window->vulkanInstance()->deviceFunctions(m_window->device());

    createBuffer(sizeof(Vertex) * m_vertexCount,
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 m_vertexBuffer, m_vertexBufferMemory);

    void* data;
    df->vkMapMemory(m_window->device(), m_vertexBufferMemory, 0, VK_WHOLE_SIZE, 0, &data);
    memcpy(data, vertices.constData(), sizeof(Vertex) * m_vertexCount);
    df->vkUnmapMemory(m_window->device(), m_vertexBufferMemory);

    createBuffer(sizeof(uint32_t) * m_indexCount,
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 m_indexBuffer, m_indexBufferMemory);

    df->vkMapMemory(m_window->device(), m_indexBufferMemory, 0, VK_WHOLE_SIZE, 0, &data);
    memcpy(data, indices.constData(), sizeof(uint32_t) * m_indexCount);
    df->vkUnmapMemory(m_window->device(), m_indexBufferMemory);
}

// ── Shader Module ─────────────────────────────────────────
VkShaderModule VulkanRenderer::createShaderModule(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        qFatal("Shader nicht gefunden: %s", qPrintable(path));
    QByteArray code = file.readAll();

    VkShaderModuleCreateInfo info{};
    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();
    info.pCode    = reinterpret_cast<const uint32_t*>(code.constData());

    VkShaderModule mod;
    m_window->vulkanInstance()->deviceFunctions(m_window->device())
        ->vkCreateShaderModule(m_window->device(), &info, nullptr, &mod);
    return mod;
}

// ----------------- INIT -----------------
void VulkanRenderer::initResources() {

    // OBJ laden — lege eine .obj Datei ins Projektverzeichnis
    loadObj(":/models/helmet.obj");

    // Uniform Buffer
    createBuffer(sizeof(float) * 32,
                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 m_uniformBuffer, m_uniformBufferMemory);

    auto* df = m_window->vulkanInstance()->deviceFunctions(m_window->device());
    df->vkMapMemory(m_window->device(), m_uniformBufferMemory, 0, VK_WHOLE_SIZE, 0, &m_uniformBufferMapped);

    // Descriptor Set Layout
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding         = 0;
    uboBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &uboBinding;
    df->vkCreateDescriptorSetLayout(m_window->device(), &layoutInfo, nullptr, &m_descSetLayout);

    // Descriptor Pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets       = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    df->vkCreateDescriptorPool(m_window->device(), &poolInfo, nullptr, &m_descPool);

    // Descriptor Set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = m_descPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &m_descSetLayout;
    df->vkAllocateDescriptorSets(m_window->device(), &allocInfo, &m_descSet);

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = m_uniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range  = sizeof(float) * 32;

    VkWriteDescriptorSet descWrite{};
    descWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrite.dstSet          = m_descSet;
    descWrite.dstBinding      = 0;
    descWrite.descriptorCount = 1;
    descWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descWrite.pBufferInfo     = &bufferInfo;
    df->vkUpdateDescriptorSets(m_window->device(), 1, &descWrite, 0, nullptr);

    // Shader
    VkShaderModule vertShader = createShaderModule(":/shader/triangle.vert.spv");
    VkShaderModule fragShader = createShaderModule(":/shader/triangle.frag.spv");

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertShader;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragShader;
    stages[1].pName  = "main";

    // Vertex Input — pos(0) + color(1)
    VkVertexInputBindingDescription binding{};
    binding.binding   = 0;
    binding.stride    = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[2]{};
    attrs[0].binding  = 0;
    attrs[0].location = 0;
    attrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset   = offsetof(Vertex, pos);
    attrs[1].binding  = 0;
    attrs[1].location = 1;
    attrs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset   = offsetof(Vertex, normal);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount   = 1;
    vertexInput.pVertexBindingDescriptions      = &binding;
    vertexInput.vertexAttributeDescriptionCount = 2;
    vertexInput.pVertexAttributeDescriptions    = attrs;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode    = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = m_window->sampleCountFlagBits();

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable  = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates    = dynamicStates;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts    = &m_descSetLayout;
    df->vkCreatePipelineLayout(m_window->device(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout);

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = stages;
    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &dynamicState;
    pipelineInfo.layout              = m_pipelineLayout;
    pipelineInfo.renderPass          = m_window->defaultRenderPass();
    df->vkCreateGraphicsPipelines(m_window->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline);

    df->vkDestroyShaderModule(m_window->device(), vertShader, nullptr);
    df->vkDestroyShaderModule(m_window->device(), fragShader, nullptr);

    qDebug("Pipeline mit Mesh erstellt");

    qDebug("Pipeline created");
}

void VulkanRenderer::initSwapChainResources() {}
void VulkanRenderer::releaseSwapChainResources() {}

void VulkanRenderer::releaseResources() {
    auto* df = m_window->vulkanInstance()->deviceFunctions(m_window->device());
    if (m_pipeline)             df->vkDestroyPipeline(m_window->device(), m_pipeline, nullptr);
    if (m_pipelineLayout)       df->vkDestroyPipelineLayout(m_window->device(), m_pipelineLayout, nullptr);
    if (m_descPool)             df->vkDestroyDescriptorPool(m_window->device(), m_descPool, nullptr);
    if (m_descSetLayout)        df->vkDestroyDescriptorSetLayout(m_window->device(), m_descSetLayout, nullptr);
    if (m_vertexBuffer)         df->vkDestroyBuffer(m_window->device(), m_vertexBuffer, nullptr);
    if (m_vertexBufferMemory)   df->vkFreeMemory(m_window->device(), m_vertexBufferMemory, nullptr);
    if (m_indexBuffer)          df->vkDestroyBuffer(m_window->device(), m_indexBuffer, nullptr);
    if (m_indexBufferMemory)    df->vkFreeMemory(m_window->device(), m_indexBufferMemory, nullptr);
    if (m_uniformBuffer)        df->vkDestroyBuffer(m_window->device(), m_uniformBuffer, nullptr);
    if (m_uniformBufferMemory)  df->vkFreeMemory(m_window->device(), m_uniformBufferMemory, nullptr);
    if (m_uniformBufferMapped) {
        df->vkUnmapMemory(m_window->device(), m_uniformBufferMemory);
        m_uniformBufferMapped = nullptr;
    }
}
void VulkanRenderer::startNextFrame() {
    auto* df = m_window->vulkanInstance()->deviceFunctions(m_window->device());
    VkCommandBuffer cb = m_window->currentCommandBuffer();
    const QSize sz = m_window->swapChainImageSize();

    // MVP Matrix berechnen
    QMatrix4x4 proj;
    proj.perspective(60.0f, sz.width() / float(sz.height()), 0.01f, 100.0f);
    // Vulkan Y-Achse ist invertiert gegenüber OpenGL
    proj.scale(1.0f, -1.0f, 1.0f);


    float yaw, pitch, zoom;
    {
        QMutexLocker lock(&m_mutex);
        yaw   = m_yaw;
        pitch = m_pitch;
        zoom = m_zoom;
    }

    QMatrix4x4 view;
    view.lookAt({0, 2, zoom}, {0, 0, 0}, {0, 1, 0});


    QMatrix4x4 model;
    model.scale(0.05f);
    model.rotate(yaw,   0, 1, 0);
    model.rotate(pitch, 1, 0, 0);

    QMatrix4x4 mvp = proj * view * model;

    // Uniform Buffer updaten
    // UBO mit mvp + model
    struct UBOData {
        float mvp[16];
        float model[16];
    };

    UBOData uboData;
    memcpy(uboData.mvp,   mvp.constData(),   sizeof(float) * 16);
    memcpy(uboData.model, model.constData(), sizeof(float) * 16);

    void* data;
    memcpy(m_uniformBufferMapped, &uboData, sizeof(UBOData));


    // RenderPass
    VkClearColorValue clearColor = {{ 0.1f, 0.12f, 0.15f, 1.0f }};
    VkClearDepthStencilValue clearDS = { 1.0f, 0 };
    VkClearValue clearValues[2];
    clearValues[0].color = clearColor;
    clearValues[1].depthStencil = clearDS;

    VkRenderPassBeginInfo rpBeginInfo{};
    rpBeginInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass      = m_window->defaultRenderPass();
    rpBeginInfo.framebuffer     = m_window->currentFramebuffer();
    rpBeginInfo.renderArea.extent.width  = sz.width();
    rpBeginInfo.renderArea.extent.height = sz.height();
    rpBeginInfo.clearValueCount = 2;
    rpBeginInfo.pClearValues    = clearValues;

    df->vkCmdBeginRenderPass(cb, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    df->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    VkViewport viewport{};
    viewport.width    = static_cast<float>(sz.width());
    viewport.height   = static_cast<float>(sz.height());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    df->vkCmdSetViewport(cb, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent.width  = sz.width();
    scissor.extent.height = sz.height();
    df->vkCmdSetScissor(cb, 0, 1, &scissor);

    df->vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_pipelineLayout, 0, 1, &m_descSet, 0, nullptr);

    VkDeviceSize offset = 0;
    df->vkCmdBindVertexBuffers(cb, 0, 1, &m_vertexBuffer, &offset);
    df->vkCmdBindIndexBuffer(cb, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    df->vkCmdDrawIndexed(cb, m_indexCount, 1, 0, 0, 0);

    df->vkCmdEndRenderPass(cb);
    m_window->frameReady();
    m_window->requestUpdate();
}