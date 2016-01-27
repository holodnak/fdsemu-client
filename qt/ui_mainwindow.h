/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.5.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QAction *actionE_xit;
    QAction *action_Erase;
    QAction *action_Dump;
    QAction *action_About;
    QAction *action_Delete;
    QAction *action_Info;
    QAction *action_Save;
    QAction *actionUpdate_firmware;
    QAction *actionUpdate_loader;
    QAction *action_Read_disk;
    QAction *action_Write_disk;
    QAction *action_Write_disk_image;
    QAction *action_Save_disk_image;
    QAction *action_Dump_flash;
    QAction *action_Restore_dump;
    QWidget *centralWidget;
    QVBoxLayout *verticalLayout;
    QLabel *label;
    QListWidget *listWidget;
    QMenuBar *menuBar;
    QMenu *menu_File;
    QMenu *menu_Operations;
    QMenu *menu_Help;
    QMenu *menu_Disk;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QStringLiteral("MainWindow"));
        MainWindow->resize(541, 440);
        MainWindow->setAcceptDrops(true);
        MainWindow->setStyleSheet(QStringLiteral(""));
        actionE_xit = new QAction(MainWindow);
        actionE_xit->setObjectName(QStringLiteral("actionE_xit"));
        action_Erase = new QAction(MainWindow);
        action_Erase->setObjectName(QStringLiteral("action_Erase"));
        action_Dump = new QAction(MainWindow);
        action_Dump->setObjectName(QStringLiteral("action_Dump"));
        action_Dump->setEnabled(false);
        action_About = new QAction(MainWindow);
        action_About->setObjectName(QStringLiteral("action_About"));
        action_Delete = new QAction(MainWindow);
        action_Delete->setObjectName(QStringLiteral("action_Delete"));
        action_Info = new QAction(MainWindow);
        action_Info->setObjectName(QStringLiteral("action_Info"));
        action_Save = new QAction(MainWindow);
        action_Save->setObjectName(QStringLiteral("action_Save"));
        actionUpdate_firmware = new QAction(MainWindow);
        actionUpdate_firmware->setObjectName(QStringLiteral("actionUpdate_firmware"));
        actionUpdate_loader = new QAction(MainWindow);
        actionUpdate_loader->setObjectName(QStringLiteral("actionUpdate_loader"));
        action_Read_disk = new QAction(MainWindow);
        action_Read_disk->setObjectName(QStringLiteral("action_Read_disk"));
        action_Read_disk->setEnabled(true);
        action_Write_disk = new QAction(MainWindow);
        action_Write_disk->setObjectName(QStringLiteral("action_Write_disk"));
        action_Write_disk->setEnabled(true);
        action_Write_disk_image = new QAction(MainWindow);
        action_Write_disk_image->setObjectName(QStringLiteral("action_Write_disk_image"));
        action_Save_disk_image = new QAction(MainWindow);
        action_Save_disk_image->setObjectName(QStringLiteral("action_Save_disk_image"));
        action_Dump_flash = new QAction(MainWindow);
        action_Dump_flash->setObjectName(QStringLiteral("action_Dump_flash"));
        action_Restore_dump = new QAction(MainWindow);
        action_Restore_dump->setObjectName(QStringLiteral("action_Restore_dump"));
        centralWidget = new QWidget(MainWindow);
        centralWidget->setObjectName(QStringLiteral("centralWidget"));
        verticalLayout = new QVBoxLayout(centralWidget);
        verticalLayout->setSpacing(6);
        verticalLayout->setContentsMargins(11, 11, 11, 11);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        label = new QLabel(centralWidget);
        label->setObjectName(QStringLiteral("label"));

        verticalLayout->addWidget(label);

        listWidget = new QListWidget(centralWidget);
        listWidget->setObjectName(QStringLiteral("listWidget"));
        listWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
        listWidget->setAcceptDrops(true);
        listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
        listWidget->setSortingEnabled(true);

        verticalLayout->addWidget(listWidget);

        MainWindow->setCentralWidget(centralWidget);
        menuBar = new QMenuBar(MainWindow);
        menuBar->setObjectName(QStringLiteral("menuBar"));
        menuBar->setGeometry(QRect(0, 0, 541, 21));
        menu_File = new QMenu(menuBar);
        menu_File->setObjectName(QStringLiteral("menu_File"));
        menu_Operations = new QMenu(menuBar);
        menu_Operations->setObjectName(QStringLiteral("menu_Operations"));
        menu_Help = new QMenu(menuBar);
        menu_Help->setObjectName(QStringLiteral("menu_Help"));
        menu_Disk = new QMenu(menuBar);
        menu_Disk->setObjectName(QStringLiteral("menu_Disk"));
        MainWindow->setMenuBar(menuBar);
        statusBar = new QStatusBar(MainWindow);
        statusBar->setObjectName(QStringLiteral("statusBar"));
        statusBar->setStyleSheet(QStringLiteral("QStatusBar::item {border: none;}"));
        MainWindow->setStatusBar(statusBar);

        menuBar->addAction(menu_File->menuAction());
        menuBar->addAction(menu_Operations->menuAction());
        menuBar->addAction(menu_Disk->menuAction());
        menuBar->addAction(menu_Help->menuAction());
        menu_File->addAction(actionE_xit);
        menu_Operations->addAction(action_Write_disk_image);
        menu_Operations->addAction(action_Save_disk_image);
        menu_Operations->addAction(action_Erase);
        menu_Operations->addSeparator();
        menu_Operations->addAction(actionUpdate_firmware);
        menu_Operations->addSeparator();
        menu_Operations->addAction(action_Dump_flash);
        menu_Operations->addAction(action_Restore_dump);
        menu_Help->addAction(action_About);
        menu_Disk->addAction(action_Read_disk);
        menu_Disk->addAction(action_Write_disk);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QApplication::translate("MainWindow", "FDSemu Client", 0));
        actionE_xit->setText(QApplication::translate("MainWindow", "E&xit", 0));
#ifndef QT_NO_STATUSTIP
        actionE_xit->setStatusTip(QApplication::translate("MainWindow", "Quit FDSemu client app.", 0));
#endif // QT_NO_STATUSTIP
        action_Erase->setText(QApplication::translate("MainWindow", "&Erase disk image...", 0));
#ifndef QT_NO_STATUSTIP
        action_Erase->setStatusTip(QApplication::translate("MainWindow", "Erase a full disk image from the flash.", 0));
#endif // QT_NO_STATUSTIP
#ifndef QT_NO_WHATSTHIS
        action_Erase->setWhatsThis(QApplication::translate("MainWindow", "Erase disk image from flash memory.", 0));
#endif // QT_NO_WHATSTHIS
        action_Dump->setText(QApplication::translate("MainWindow", "&Dump...", 0));
#ifndef QT_NO_STATUSTIP
        action_Dump->setStatusTip(QApplication::translate("MainWindow", "Dump raw flash data to file.", 0));
#endif // QT_NO_STATUSTIP
        action_About->setText(QApplication::translate("MainWindow", "&About...", 0));
#ifndef QT_NO_STATUSTIP
        action_About->setStatusTip(QApplication::translate("MainWindow", "Display information about this program.", 0));
#endif // QT_NO_STATUSTIP
        action_Delete->setText(QApplication::translate("MainWindow", "&Delete", 0));
#ifndef QT_NO_TOOLTIP
        action_Delete->setToolTip(QApplication::translate("MainWindow", "Erase selected disk.", 0));
#endif // QT_NO_TOOLTIP
        action_Info->setText(QApplication::translate("MainWindow", "&Info...", 0));
#ifndef QT_NO_TOOLTIP
        action_Info->setToolTip(QApplication::translate("MainWindow", "Get information about selected disk.", 0));
#endif // QT_NO_TOOLTIP
        action_Save->setText(QApplication::translate("MainWindow", "&Save", 0));
#ifndef QT_NO_TOOLTIP
        action_Save->setToolTip(QApplication::translate("MainWindow", "Save selected disk.", 0));
#endif // QT_NO_TOOLTIP
        actionUpdate_firmware->setText(QApplication::translate("MainWindow", "Update &firmware...", 0));
#ifndef QT_NO_STATUSTIP
        actionUpdate_firmware->setStatusTip(QApplication::translate("MainWindow", "Update the firmware from a file.", 0));
#endif // QT_NO_STATUSTIP
        actionUpdate_loader->setText(QApplication::translate("MainWindow", "Update &loader...", 0));
#ifndef QT_NO_STATUSTIP
        actionUpdate_loader->setStatusTip(QApplication::translate("MainWindow", "Update the game loader from .FDS file.", 0));
#endif // QT_NO_STATUSTIP
        action_Read_disk->setText(QApplication::translate("MainWindow", "&Read disk...", 0));
#ifndef QT_NO_STATUSTIP
        action_Read_disk->setStatusTip(QApplication::translate("MainWindow", "Read disk from the FDS drive.", 0));
#endif // QT_NO_STATUSTIP
        action_Write_disk->setText(QApplication::translate("MainWindow", "&Write disk...", 0));
#ifndef QT_NO_STATUSTIP
        action_Write_disk->setStatusTip(QApplication::translate("MainWindow", "Write a .FDS disk image to a real FDS drive.", 0));
#endif // QT_NO_STATUSTIP
        action_Write_disk_image->setText(QApplication::translate("MainWindow", "&Write disk image...", 0));
#ifndef QT_NO_STATUSTIP
        action_Write_disk_image->setStatusTip(QApplication::translate("MainWindow", "Write a .FDS disk image to the flash.", 0));
#endif // QT_NO_STATUSTIP
        action_Save_disk_image->setText(QApplication::translate("MainWindow", "&Save disk image...", 0));
#ifndef QT_NO_STATUSTIP
        action_Save_disk_image->setStatusTip(QApplication::translate("MainWindow", "Save .FDS format image from an image stored in flash.", 0));
#endif // QT_NO_STATUSTIP
        action_Dump_flash->setText(QApplication::translate("MainWindow", "&Dump flash...", 0));
        action_Restore_dump->setText(QApplication::translate("MainWindow", "&Restore dump...", 0));
        label->setText(QApplication::translate("MainWindow", "TextLabel", 0));
        menu_File->setTitle(QApplication::translate("MainWindow", "&File", 0));
        menu_Operations->setTitle(QApplication::translate("MainWindow", "&Flash", 0));
        menu_Help->setTitle(QApplication::translate("MainWindow", "&Help", 0));
        menu_Disk->setTitle(QApplication::translate("MainWindow", "&Disk", 0));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
