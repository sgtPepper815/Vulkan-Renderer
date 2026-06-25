#include <QApplication>
#include <QVulkanInstance>
#include "mainwindow.h"
#include "vulkanwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Vulkan Instance aufsetzen
    QVulkanInstance inst;
#ifndef NDEBUG
    inst.setLayers({ "VK_LAYER_KHRONOS_validation" });
#endif
    inst.setApiVersion(QVersionNumber(1, 3));

    if (!inst.create())
        qFatal("Vulkan nicht verfügbar: %d", inst.errorCode());

    // VulkanWindow erstellen und an Instance binden
    VulkanWindow* vulkanWindow = new VulkanWindow;
    vulkanWindow->setVulkanInstance(&inst);

    MainWindow w(vulkanWindow);
    w.show();
    return QApplication::exec();
}
