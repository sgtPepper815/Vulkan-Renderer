#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPoint>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class VulkanWindow;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(VulkanWindow* vulkanWindow, QWidget* parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow* ui;
    VulkanWindow*   m_vulkanWindow;
    QWidget*        m_container;
    QPoint          m_lastMousePos;
    float           m_yaw   = 0.0f;
    float           m_pitch = 0.0f;

    float m_zoom = 5.0f;

    bool eventFilter(QObject* obj, QEvent* event);

private slots:
    void onLoadMesh();

};

#endif