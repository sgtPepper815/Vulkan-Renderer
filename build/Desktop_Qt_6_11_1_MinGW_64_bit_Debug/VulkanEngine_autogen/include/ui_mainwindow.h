/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QHBoxLayout *mainLayout;
    QWidget *sidebar;
    QVBoxLayout *sidebarLayout;
    QPushButton *loadMeshBtn;
    QSpacerItem *sidebarSpacer;
    QPushButton *wireframeBtn;
    QPushButton *litBtn;
    QPushButton *texturedBtn;
    QWidget *renderView;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(900, 600);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        mainLayout = new QHBoxLayout(centralwidget);
        mainLayout->setSpacing(12);
        mainLayout->setObjectName("mainLayout");
        mainLayout->setContentsMargins(12, 12, 12, 12);
        sidebar = new QWidget(centralwidget);
        sidebar->setObjectName("sidebar");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(sidebar->sizePolicy().hasHeightForWidth());
        sidebar->setSizePolicy(sizePolicy);
        sidebar->setMinimumSize(QSize(180, 0));
        sidebarLayout = new QVBoxLayout(sidebar);
        sidebarLayout->setSpacing(8);
        sidebarLayout->setObjectName("sidebarLayout");
        loadMeshBtn = new QPushButton(sidebar);
        loadMeshBtn->setObjectName("loadMeshBtn");

        sidebarLayout->addWidget(loadMeshBtn);

        sidebarSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        sidebarLayout->addItem(sidebarSpacer);

        wireframeBtn = new QPushButton(sidebar);
        wireframeBtn->setObjectName("wireframeBtn");
        wireframeBtn->setCheckable(true);

        sidebarLayout->addWidget(wireframeBtn);

        litBtn = new QPushButton(sidebar);
        litBtn->setObjectName("litBtn");
        litBtn->setCheckable(true);

        sidebarLayout->addWidget(litBtn);

        texturedBtn = new QPushButton(sidebar);
        texturedBtn->setObjectName("texturedBtn");
        texturedBtn->setCheckable(true);
        texturedBtn->setChecked(true);

        sidebarLayout->addWidget(texturedBtn);


        mainLayout->addWidget(sidebar);

        renderView = new QWidget(centralwidget);
        renderView->setObjectName("renderView");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy1.setHorizontalStretch(1);
        sizePolicy1.setVerticalStretch(1);
        sizePolicy1.setHeightForWidth(renderView->sizePolicy().hasHeightForWidth());
        renderView->setSizePolicy(sizePolicy1);

        mainLayout->addWidget(renderView);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 900, 26));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "Engine", nullptr));
        loadMeshBtn->setText(QCoreApplication::translate("MainWindow", "Load Mesh", nullptr));
        wireframeBtn->setText(QCoreApplication::translate("MainWindow", "Wireframe", nullptr));
        litBtn->setText(QCoreApplication::translate("MainWindow", "Lit", nullptr));
        texturedBtn->setText(QCoreApplication::translate("MainWindow", "Textured", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
