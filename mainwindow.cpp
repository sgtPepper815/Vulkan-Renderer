#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "vulkanwindow.h"   // ← das fehlt! Ohne dem kennt der Compiler die Vererbung nicht
#include <QWidget>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QFileDialog>

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
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto* e = static_cast<QMouseEvent*>(event);
        m_lastMousePos = e->globalPosition().toPoint();
        return false;
    }
    if (event->type() == QEvent::MouseMove) {
        auto* e = static_cast<QMouseEvent*>(event);
        if (e->buttons() & Qt::LeftButton) {
            QPoint pos = e->globalPosition().toPoint();
            QPoint delta = pos - m_lastMousePos;
            m_lastMousePos = pos;
            m_yaw   += delta.x() * 0.5f;
            m_pitch += delta.y() * 0.5f;

            VulkanRenderer* r = m_vulkanWindow->renderer();
            if (r) {
                float yaw = m_yaw;
                float pitch = m_pitch;
                QMetaObject::invokeMethod(m_vulkanWindow, [r, yaw, pitch]() {
                    r->setRotation(yaw, pitch);
                }, Qt::QueuedConnection);
            }
        }
        return false;
    }
    if (event->type() == QEvent::Wheel) {
        auto* e = static_cast<QWheelEvent*>(event);
        m_zoom -= e->angleDelta().y() * 0.01f;
        m_zoom = qBound(0.5f, m_zoom, 50.0f);

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
    QString path = QFileDialog::getOpenFileName(
        this,
        "Mesh laden",
        "",
        "OBJ Dateien (*.obj)"
        );

    if (path.isEmpty()) return;

    VulkanRenderer* r = m_vulkanWindow->renderer();
    if (r) r->loadMesh(path);
}



