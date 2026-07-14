#ifndef VULKANRENDERER_H
#define VULKANRENDERER_H

#include <QVulkanWindowRenderer>
#include <QMutex>
#include <QImage>

struct Vertex {
    float pos[3];
    float normal[3];
    float uv[2];
};


class VulkanRenderer : public QVulkanWindowRenderer
{
public:
    explicit VulkanRenderer(QVulkanWindow* window);

    void initResources() override;
    void initSwapChainResources() override;
    void releaseSwapChainResources() override;
    void releaseResources() override;
    void startNextFrame() override;

    // mouse
    void setRotation(float yaw, float pitch) {QMutexLocker lock(&m_mutex); m_yaw = yaw; m_pitch = pitch; }
    void setPan(float panX, float panY) { QMutexLocker lock(&m_mutex); m_panX = panX; m_panY = panY; }

    void loadMesh(const QString& path);
    void loadTexture(const QString& path);

    void setZoom(float zoom) {
        QMutexLocker lock(&m_mutex);
        m_zoom = qBound(0.02f, zoom, 50.0f);  // min 0.02 (nah an kleine Objekte ranzoomen), max 50
    }

    float zoom() {
        QMutexLocker lock(&m_mutex);
        return m_zoom;
    }

private:
    QVulkanWindow* m_window;

    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

    // Vertex Buffer
    VkBuffer       m_vertexBuffer       = VK_NULL_HANDLE;
    VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
    uint32_t       m_vertexCount        = 0;

    // Index Buffer
    VkBuffer       m_indexBuffer       = VK_NULL_HANDLE;
    VkDeviceMemory m_indexBufferMemory = VK_NULL_HANDLE;
    uint32_t       m_indexCount        = 0;

    // Uniform Buffer (MVP Matrix)
    VkBuffer              m_uniformBuffer       = VK_NULL_HANDLE;
    VkDeviceMemory        m_uniformBufferMemory = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descSetLayout       = VK_NULL_HANDLE;
    VkDescriptorPool      m_descPool            = VK_NULL_HANDLE;
    VkDescriptorSet       m_descSet             = VK_NULL_HANDLE;

    // Textur
    VkImage        m_textureImage       = VK_NULL_HANDLE;
    VkDeviceMemory m_textureImageMemory = VK_NULL_HANDLE;
    VkImageView    m_textureImageView   = VK_NULL_HANDLE;
    VkSampler      m_textureSampler     = VK_NULL_HANDLE;

    // Rotation
    QMutex m_mutex;
    float m_yaw   = 0.0f;
    float m_pitch = 0.0f;

    float m_zoom = 5.0f;

    // Pan (rechte Maustaste) — Offset entlang der Kamera-Rechts-/Auf-Achse
    float m_panX = 0.0f;
    float m_panY = 0.0f;

    void* m_uniformBufferMapped = nullptr;


    // helper
    VkShaderModule createShaderModule(const QString& path);
    uint32_t       findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void           createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& memory);
    void           loadObj(const QString& path);

    void           createImage(uint32_t width, uint32_t height, VkFormat format,
                      VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkImage& image, VkDeviceMemory& memory);
    VkCommandBuffer beginOneTimeCommands();
    void           endOneTimeCommands(VkCommandBuffer cb);
    void           transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
    void           copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void           uploadTextureImage(const QImage& img);
    void           updateTextureDescriptor();
};

#endif // VULKANRENDERER_H
