#ifndef VULKANWINDOW_H
#define VULKANWINDOW_H

#include <QVulkanWindow>
#include "vulkanrenderer.h"

class VulkanWindow : public QVulkanWindow
{
    Q_OBJECT
public:
    VulkanWindow();
    QVulkanWindowRenderer* createRenderer() override;

    VulkanRenderer* renderer() const { return m_renderer; }

private:
    VulkanRenderer* m_renderer = nullptr;
};

#endif // VULKANWINDOW_H
