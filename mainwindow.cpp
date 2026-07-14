#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "vulkanwindow.h"   // ← das fehlt! Ohne dem kennt der Compiler die Vererbung nicht
#include <QWidget>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QButtonGroup>
#include <cmath>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#endif

MainWindow::MainWindow(VulkanWindow* vulkanWindow, QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_vulkanWindow(vulkanWindow)
{
    ui->setupUi(this);

    m_container = QWidget::createWindowContainer(vulkanWindow, this);
    m_container->setFocusPolicy(Qt::StrongFocus);
    m_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_container->setMouseTracking(true);

    QVBoxLayout* layout = new QVBoxLayout(ui->renderView);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_container);

    qApp->installEventFilter(this);

    connect(ui->loadMeshBtn, &QPushButton::clicked, this, &MainWindow::onLoadMesh);

    // Render-Mode-Buttons: exklusiv wie Radiobuttons, aber im Look von QPushButton
    QButtonGroup* renderModeGroup = new QButtonGroup(this);
    renderModeGroup->setExclusive(true);
    renderModeGroup->addButton(ui->wireframeBtn);
    renderModeGroup->addButton(ui->litBtn);
    renderModeGroup->addButton(ui->texturedBtn);

    connect(ui->wireframeBtn, &QPushButton::clicked, this, [this]() {
        if (VulkanRenderer* r = m_vulkanWindow->renderer())
            QMetaObject::invokeMethod(m_vulkanWindow, [r]() {
                r->setRenderMode(VulkanRenderer::RenderMode::Wireframe);
            }, Qt::QueuedConnection);
    });
    connect(ui->litBtn, &QPushButton::clicked, this, [this]() {
        if (VulkanRenderer* r = m_vulkanWindow->renderer())
            QMetaObject::invokeMethod(m_vulkanWindow, [r]() {
                r->setRenderMode(VulkanRenderer::RenderMode::Lit);
            }, Qt::QueuedConnection);
    });
    connect(ui->texturedBtn, &QPushButton::clicked, this, [this]() {
        if (VulkanRenderer* r = m_vulkanWindow->renderer())
            QMetaObject::invokeMethod(m_vulkanWindow, [r]() {
                r->setRenderMode(VulkanRenderer::RenderMode::Textured);
            }, Qt::QueuedConnection);
    });

#ifdef Q_OS_WIN
    // Dunkle Titelleiste, damit sie zum restlichen dunklen Theme passt (Windows 10 20H1+/11)
    BOOL useDarkMode = TRUE;
    DwmSetWindowAttribute(reinterpret_cast<HWND>(winId()), DWMWA_USE_IMMERSIVE_DARK_MODE,
                           &useDarkMode, sizeof(useDarkMode));
#endif
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    // Nur auf Maus-/Rad-Events reagieren, die im Render-View beginnen –
    // sonst dreht sich die Kamera auch beim Klicken/Ziehen auf Buttons etc.
    // (Ein embedded QWindow (createWindowContainer) liefert Input-Events nicht
    // zuverlässig mit obj==m_container, daher stattdessen die Bildschirm-
    // position gegen die Geometrie des Render-Views prüfen.)
    QRect viewRect(m_container->mapToGlobal(QPoint(0, 0)), m_container->size());

    if (event->type() == QEvent::MouseButtonPress) {
        auto* e = static_cast<QMouseEvent*>(event);
        QPoint pos = e->globalPosition().toPoint();
        if (e->button() == Qt::LeftButton && viewRect.contains(pos)) {
            m_dragging     = true;
            m_lastMousePos = pos;
        }
        if (e->button() == Qt::RightButton && viewRect.contains(pos)) {
            m_panning    = true;
            m_lastPanPos = pos;
        }
        return false;
    }
    if (event->type() == QEvent::MouseButtonRelease) {
        auto* e = static_cast<QMouseEvent*>(event);
        if (e->button() == Qt::LeftButton)
            m_dragging = false;
        if (e->button() == Qt::RightButton)
            m_panning = false;
        return false;
    }
    if (event->type() == QEvent::MouseMove) {
        auto* e = static_cast<QMouseEvent*>(event);
        if (m_dragging && (e->buttons() & Qt::LeftButton)) {
            QPoint pos = e->globalPosition().toPoint();
            QPoint delta = pos - m_lastMousePos;
            m_lastMousePos = pos;
            m_yaw   += delta.x() * 0.5f;
            m_pitch += delta.y() * 0.5f;
            // Pitch clampen, sonst kippt das Modell über den Zenit und
            // die Yaw-Richtung wirkt danach invertiert (Gimbal-Flip)
            m_pitch = qBound(-89.0f, m_pitch, 89.0f);

            VulkanRenderer* r = m_vulkanWindow->renderer();
            if (r) {
                float yaw = m_yaw;
                float pitch = m_pitch;
                QMetaObject::invokeMethod(m_vulkanWindow, [r, yaw, pitch]() {
                    r->setRotation(yaw, pitch);
                }, Qt::QueuedConnection);
            }
        }
        if (m_panning && (e->buttons() & Qt::RightButton)) {
            QPoint pos = e->globalPosition().toPoint();
            QPoint delta = pos - m_lastPanPos;
            m_lastPanPos = pos;

            // Pan-Geschwindigkeit skaliert mit dem Zoom, damit das Ziehen bei
            // jeder Entfernung gleich "greifbar" wirkt (nah = feiner, weit weg = schneller).
            float panScale = m_zoom * 0.0015f;
            m_panX += -delta.x() * panScale;
            m_panY +=  delta.y() * panScale;

            VulkanRenderer* r = m_vulkanWindow->renderer();
            if (r) {
                float panX = m_panX;
                float panY = m_panY;
                QMetaObject::invokeMethod(m_vulkanWindow, [r, panX, panY]() {
                    r->setPan(panX, panY);
                }, Qt::QueuedConnection);
            }
        }
        return false;
    }
    if (event->type() == QEvent::Wheel) {
        auto* e = static_cast<QWheelEvent*>(event);
        if (!viewRect.contains(e->globalPosition().toPoint()))
            return QMainWindow::eventFilter(obj, event);

        // Multiplikativ statt additiv zoomen: bei einem festen Schritt pro
        // Mausrad-Tick wird der Zoom nahe der unteren Grenze (kleine, importierte
        // Objekte) sofort abgeschnitten und ist weit weg viel zu grob abgestuft.
        // Ein prozentualer Schritt bleibt in jeder Größenordnung gleich fein.
        float factor = std::pow(0.999f, e->angleDelta().y());
        m_zoom *= factor;
        m_zoom = qBound(0.02f, m_zoom, 50.0f);

        VulkanRenderer* r = m_vulkanWindow->renderer();
        if (r) {
            float zoom = m_zoom;
            QMetaObject::invokeMethod(m_vulkanWindow, [r, zoom]() {
                r->setZoom(zoom);
            }, Qt::QueuedConnection);
        }
        return false;
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::onLoadMesh() {
    QString meshPath = QFileDialog::getOpenFileName(
        this,
        "Mesh laden",
        "",
        "OBJ Dateien (*.obj)"
        );

    if (meshPath.isEmpty()) return;

    VulkanRenderer* r = m_vulkanWindow->renderer();
    if (!r) return;

    r->loadMesh(meshPath);

    QString texPath = QFileDialog::getOpenFileName(
        this,
        "Textur laden (optional)",
        QFileInfo(meshPath).absolutePath(),
        "Bilddateien (*.png *.jpg *.jpeg *.bmp *.tga)"
        );

    if (!texPath.isEmpty())
        r->loadTexture(texPath);
}



