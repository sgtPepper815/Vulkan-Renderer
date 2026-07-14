#include <QApplication>
#include <QVulkanInstance>
#include <QPalette>
#include <QColor>
#include <QStyleFactory>
#include "mainwindow.h"
#include "vulkanwindow.h"

namespace {

void applyDarkTheme(QApplication& app) {
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window,          QColor(30, 31, 34));
    palette.setColor(QPalette::WindowText,      QColor(230, 230, 230));
    palette.setColor(QPalette::Base,            QColor(24, 25, 27));
    palette.setColor(QPalette::AlternateBase,   QColor(43, 45, 49));
    palette.setColor(QPalette::ToolTipBase,     QColor(43, 45, 49));
    palette.setColor(QPalette::ToolTipText,     QColor(230, 230, 230));
    palette.setColor(QPalette::Text,            QColor(230, 230, 230));
    palette.setColor(QPalette::Button,          QColor(43, 45, 49));
    palette.setColor(QPalette::ButtonText,      QColor(230, 230, 230));
    palette.setColor(QPalette::BrightText,      QColor(255, 90, 90));
    palette.setColor(QPalette::Link,            QColor(74, 125, 252));
    palette.setColor(QPalette::Highlight,       QColor(74, 125, 252));
    palette.setColor(QPalette::HighlightedText, QColor(15, 16, 18));
    palette.setColor(QPalette::Disabled, QPalette::Text,       QColor(107, 108, 112));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(107, 108, 112));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(107, 108, 112));
    app.setPalette(palette);

    app.setStyleSheet(R"(
        QWidget {
            font-family: "Segoe UI";
            font-size: 10pt;
        }

        QMenuBar {
            background-color: #17181a;
            border-bottom: 1px solid #2b2d31;
        }
        QMenuBar::item {
            background: transparent;
            padding: 5px 10px;
        }
        QMenuBar::item:selected {
            background-color: #2b2d31;
            border-radius: 4px;
        }

        QMenu {
            background-color: #242528;
            border: 1px solid #34353a;
        }
        QMenu::item:selected {
            background-color: #4a7dfc;
        }

        QStatusBar {
            background-color: #17181a;
            color: #9a9ba0;
            border-top: 1px solid #2b2d31;
        }

        QPushButton {
            background-color: #2b2d31;
            border: 1px solid #3a3b40;
            border-radius: 6px;
            padding: 8px 14px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #34363b;
            border: 1px solid #4a7dfc;
        }
        QPushButton:pressed {
            background-color: #202124;
        }
        QPushButton:disabled {
            color: #6b6c70;
            background-color: #202124;
            border: 1px solid #2a2b2f;
        }

        QWidget#renderView {
            background-color: #101114;
            border: 1px solid #2b2d31;
            border-radius: 6px;
        }

        QWidget#sidebar {
            background-color: #202124;
            border: 1px solid #2b2d31;
            border-radius: 6px;
        }
        QWidget#sidebar QPushButton {
            text-align: left;
        }

        QScrollBar:vertical {
            background: #1e1f22;
            width: 10px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #3a3b40;
            border-radius: 5px;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover {
            background: #4a7dfc;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }

        QToolTip {
            background-color: #2b2d31;
            color: #e6e6e6;
            border: 1px solid #3a3b40;
            padding: 4px;
        }
    )");
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    applyDarkTheme(a);

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
