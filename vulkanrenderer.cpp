#include "vulkanrenderer.h"
#include <QVulkanFunctions>
#include <QFile>
#include <QTextStream>
#include <QMatrix4x4>
#include <QVector3D>

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
    QVector<std::array<float,2>> texCoords;
    QVector<Vertex>              vertices;
    QVector<uint32_t>            indices;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.startsWith("vt ")) {
            QStringList p = line.split(' ', Qt::SkipEmptyParts);
            texCoords.append({ p[1].toFloat(), p[2].toFloat() });
        } else if (line.startsWith("v ")) {
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
                bool hasUv = indices_str.size() > 1 && !indices_str[1].isEmpty();
                uint32_t uvIdx = hasUv ? indices_str[1].toUInt() - 1 : 0;

                Vertex v;
                v.pos[0]    = positions[posIdx][0];
                v.pos[1]    = positions[posIdx][1];
                v.pos[2]    = positions[posIdx][2];
                v.normal[0] = normals.isEmpty() ? 0.0f : normals[nrmIdx][0];
                v.normal[1] = normals.isEmpty() ? 0.0f : normals[nrmIdx][1];
                v.normal[2] = normals.isEmpty() ? 1.0f : normals[nrmIdx][2];
                v.uv[0]     = (hasUv && !texCoords.isEmpty()) ? texCoords[uvIdx][0] : 0.0f;
                v.uv[1]     = (hasUv && !texCoords.isEmpty()) ? 1.0f - texCoords[uvIdx][1] : 0.0f;

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

// ── One-Time Command Buffer Helper ────────────────────────
VkCommandBuffer VulkanRenderer::beginOneTimeCommands() {
    auto* df = m_window->vulkanInstance()->deviceFunctions(m_window->device());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = m_window->graphicsCommandPool();
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cb;
    df->vkAllocateCommandBuffers(m_window->device(), &allocInfo, &cb);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    df->vkBeginCommandBuffer(cb, &beginInfo);
    return cb;
}

void VulkanRenderer::endOneTimeCommands(VkCommandBuffer cb) {
    auto* df = m_window->vulkanInstance()->deviceFunctions(m_window->device());
    df->vkEndCommandBuffer(cb);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cb;

    VkQueue queue = m_window->graphicsQueue();
    df->vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    df->vkQueueWaitIdle(queue);

    df->vkFreeCommandBuffers(m_window->device(), m_window->graphicsCommandPool(), 1, &cb);
}

// ── Image Helper ──────────────────────────────────────────
void VulkanRenderer::createImage(uint32_t width, uint32_t height, VkFormat format,
                                  VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                                  VkImage& image, VkDeviceMemory& memory) {
    auto* df = m_window->vulkanInstance()->deviceFunctions(m_window->device());

    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = format;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = usage;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    df->vkCreateImage(m_window->device(), &imageInfo, nullptr, &image);

    VkMemoryRequirements memReqs;
    df->vkGetImageMemoryRequirements(m_window->device(), image, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, properties);
    df->vkAllocateMemory(m_window->device(), &allocInfo, nullptr, &memory);
    df->vkBindImageMemory(m_window->device(), image, memory, 0);
}

void VulkanRenderer::transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
    auto* df = m_window->vulkanInstance()->deviceFunctions(m_window->device());
    VkCommandBuffer cb = beginOneTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = oldLayout;
    barrier.newLayout                       = newLayout;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    VkPipelineStageFlags srcStage, dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        qFatal("Nicht unterstuetzter Layout-Uebergang");
    }

    df->vkCmdPipelineBarrier(cb, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endOneTimeCommands(cb);
}

void VulkanRenderer::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    auto* df = m_window->vulkanInstance()->deviceFunctions(m_window->device());
    VkCommandBuffer cb = beginOneTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset                    = 0;
    region.bufferRowLength                 = 0;
    region.bufferImageHeight                = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset                     = {0, 0, 0};
    region.imageExtent                     = {width, height, 1};

    df->vkCmdCopyBufferToImage(cb, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endOneTimeCommands(cb);
}

// ── Textur laden ──────────────────────────────────────────
void VulkanRenderer::uploadTextureImage(const QImage& imgIn) {
    auto* df = m_window->vulkanInstance()->deviceFunctions(m_window->device());
    df->vkDeviceWaitIdle(m_window->device());

    QImage img = imgIn.convertToFormat(QImage::Format_RGBA8888);
    VkDeviceSize imageSize = VkDeviceSize(img.width()) * img.height() * 4;

    if (m_textureImageView)   df->vkDestroyImageView(m_window->device(), m_textureImageView, nullptr);
    if (m_textureImage)       df->vkDestroyImage(m_window->device(), m_textureImage, nullptr);
    if (m_textureImageMemory) df->vkFreeMemory(m_window->device(), m_textureImageMemory, nullptr);
    m_textureImageView   = VK_NULL_HANDLE;
    m_textureImage       = VK_NULL_HANDLE;
    m_textureImageMemory = VK_NULL_HANDLE;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingMemory);

    void* data;
    df->vkMapMemory(m_window->device(), stagingMemory, 0, imageSize, 0, &data);
    memcpy(data, img.constBits(), size_t(imageSize));
    df->vkUnmapMemory(m_window->device(), stagingMemory);

    createImage(uint32_t(img.width()), uint32_t(img.height()), VK_FORMAT_R8G8B8A8_SRGB,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                m_textureImage, m_textureImageMemory);

    transitionImageLayout(m_textureImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, m_textureImage, uint32_t(img.width()), uint32_t(img.height()));
    transitionImageLayout(m_textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    df->vkDestroyBuffer(m_window->device(), stagingBuffer, nullptr);
    df->vkFreeMemory(m_window->device(), stagingMemory, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = m_textureImage;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.layerCount     = 1;
    df->vkCreateImageView(m_window->device(), &viewInfo, nullptr, &m_textureImageView);

    if (!m_textureSampler) {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter    = VK_FILTER_LINEAR;
        samplerInfo.minFilter    = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.borderColor  = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.maxLod       = 1.0f;
        df->vkCreateSampler(m_window->device(), &samplerInfo, nullptr, &m_textureSampler);
    }

    updateTextureDescriptor();
}

void VulkanRenderer::updateTextureDescriptor() {
    auto* df = m_window->vulkanInstance()->deviceFunctions(m_window->device());

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView   = m_textureImageView;
    imageInfo.sampler     = m_textureSampler;

    VkWriteDescriptorSet descWrite{};
    descWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descWrite.dstSet          = m_descSet;
    descWrite.dstBinding      = 1;
    descWrite.descriptorCount = 1;
    descWrite.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descWrite.pImageInfo      = &imageInfo;
    df->vkUpdateDescriptorSets(m_window->device(), 1, &descWrite, 0, nullptr);
}

void VulkanRenderer::loadTexture(const QString& path) {
    QImage img(path);
    if (img.isNull()) {
        qWarning("Textur konnte nicht geladen werden: %s", qPrintable(path));
        return;
    }
    uploadTextureImage(img);
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

    // Descriptor Set Layout — binding 0: UBO, binding 1: Textur-Sampler
    VkDescriptorSetLayoutBinding bindings[2]{};
    bindings[0].binding         = 0;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    bindings[1].binding         = 1;
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 2;
    layoutInfo.pBindings    = bindings;
    df->vkCreateDescriptorSetLayout(m_window->device(), &layoutInfo, nullptr, &m_descSetLayout);

    // Descriptor Pool
    VkDescriptorPoolSize poolSizes[2]{};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets       = 1;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes    = poolSizes;
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

    // Default-Textur (1x1 weiß), bis der Nutzer eine eigene lädt
    QImage defaultTex(1, 1, QImage::Format_RGBA8888);
    defaultTex.fill(Qt::white);
    uploadTextureImage(defaultTex);

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

    // Vertex Input — pos(0) + normal(1) + uv(2)
    VkVertexInputBindingDescription binding{};
    binding.binding   = 0;
    binding.stride    = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[3]{};
    attrs[0].binding  = 0;
    attrs[0].location = 0;
    attrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset   = offsetof(Vertex, pos);
    attrs[1].binding  = 0;
    attrs[1].location = 1;
    attrs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset   = offsetof(Vertex, normal);
    attrs[2].binding  = 0;
    attrs[2].location = 2;
    attrs[2].format   = VK_FORMAT_R32G32_SFLOAT;
    attrs[2].offset   = offsetof(Vertex, uv);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount   = 1;
    vertexInput.pVertexBindingDescriptions      = &binding;
    vertexInput.vertexAttributeDescriptionCount = 3;
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

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset     = 0;
    pushConstantRange.size       = sizeof(int32_t);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &m_descSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;
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

    // Wireframe-Pipeline — gleicher Vertex-Shader, ein flacher Farb-Fragment-Shader,
    // Polygon-Mode LINE statt FILL. Braucht das GPU-Feature "fillModeNonSolid",
    // das in vulkanwindow.cpp per setEnabledFeaturesModifier() aktiviert wird.
    VkShaderModule wireVertShader = createShaderModule(":/shader/triangle.vert.spv");
    VkShaderModule wireFragShader = createShaderModule(":/shader/wireframe.frag.spv");

    VkPipelineShaderStageCreateInfo wireStages[2]{};
    wireStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    wireStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    wireStages[0].module = wireVertShader;
    wireStages[0].pName  = "main";
    wireStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    wireStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    wireStages[1].module = wireFragShader;
    wireStages[1].pName  = "main";

    VkPipelineRasterizationStateCreateInfo wireRasterizer = rasterizer;
    wireRasterizer.polygonMode = VK_POLYGON_MODE_LINE;
    wireRasterizer.cullMode    = VK_CULL_MODE_NONE;

    VkGraphicsPipelineCreateInfo wirePipelineInfo = pipelineInfo;
    wirePipelineInfo.pStages             = wireStages;
    wirePipelineInfo.pRasterizationState = &wireRasterizer;
    df->vkCreateGraphicsPipelines(m_window->device(), VK_NULL_HANDLE, 1, &wirePipelineInfo, nullptr, &m_wireframePipeline);

    df->vkDestroyShaderModule(m_window->device(), wireVertShader, nullptr);
    df->vkDestroyShaderModule(m_window->device(), wireFragShader, nullptr);

    qDebug("Pipeline mit Mesh erstellt");

    qDebug("Pipeline created");
}

void VulkanRenderer::initSwapChainResources() {}
void VulkanRenderer::releaseSwapChainResources() {}

void VulkanRenderer::releaseResources() {
    auto* df = m_window->vulkanInstance()->deviceFunctions(m_window->device());
    if (m_pipeline)             df->vkDestroyPipeline(m_window->device(), m_pipeline, nullptr);
    if (m_wireframePipeline)    df->vkDestroyPipeline(m_window->device(), m_wireframePipeline, nullptr);
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
    if (m_textureSampler)      df->vkDestroySampler(m_window->device(), m_textureSampler, nullptr);
    if (m_textureImageView)    df->vkDestroyImageView(m_window->device(), m_textureImageView, nullptr);
    if (m_textureImage)        df->vkDestroyImage(m_window->device(), m_textureImage, nullptr);
    if (m_textureImageMemory)  df->vkFreeMemory(m_window->device(), m_textureImageMemory, nullptr);
}
void VulkanRenderer::startNextFrame() {
    auto* df = m_window->vulkanInstance()->deviceFunctions(m_window->device());
    VkCommandBuffer cb = m_window->currentCommandBuffer();
    const QSize sz = m_window->swapChainImageSize();

    // MVP Matrix berechnen
    QMatrix4x4 proj;
    proj.perspective(60.0f, sz.width() / float(sz.height()), 0.005f, 100.0f);
    // Vulkan Y-Achse ist invertiert gegenüber OpenGL
    proj.scale(1.0f, -1.0f, 1.0f);


    float yaw, pitch, zoom, panX, panY;
    RenderMode renderMode;
    {
        QMutexLocker lock(&m_mutex);
        yaw        = m_yaw;
        pitch      = m_pitch;
        zoom       = m_zoom;
        panX       = m_panX;
        panY       = m_panY;
        renderMode = m_renderMode;
    }

    // Kamera-Basis (rechts/oben) aus der unverschobenen Blickrichtung ableiten,
    // damit Pan unabhängig von Yaw/Pitch (die nur das Modell drehen) entlang der
    // Bildschirmachsen der Kamera funktioniert.
    QVector3D eye0(0.0f, 2.0f, zoom);
    QVector3D center0(0.0f, 0.0f, 0.0f);
    QVector3D worldUp(0.0f, 1.0f, 0.0f);
    QVector3D forward = (center0 - eye0).normalized();
    QVector3D right   = QVector3D::crossProduct(forward, worldUp).normalized();
    QVector3D camUp   = QVector3D::crossProduct(right, forward).normalized();
    QVector3D panOffset = right * panX + camUp * panY;

    QMatrix4x4 view;
    view.lookAt(eye0 + panOffset, center0 + panOffset, worldUp);


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

    VkPipeline activePipeline = (renderMode == RenderMode::Wireframe) ? m_wireframePipeline : m_pipeline;
    df->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline);

    int32_t useTexture = (renderMode == RenderMode::Textured) ? 1 : 0;
    df->vkCmdPushConstants(cb, m_pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(int32_t), &useTexture);

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