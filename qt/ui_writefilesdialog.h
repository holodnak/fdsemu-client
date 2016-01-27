/********************************************************************************
** Form generated from reading UI file 'writefilesdialog.ui'
**
** Created by: Qt User Interface Compiler version 5.5.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_WRITEFILESDIALOG_H
#define UI_WRITEFILESDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>

QT_BEGIN_NAMESPACE

class Ui_WriteFilesDialog
{
public:
    QTreeWidget *treeWidget;
    QPushButton *closeButton;
    QProgressBar *progressBar;
    QProgressBar *diskProgressBar;

    void setupUi(QDialog *WriteFilesDialog)
    {
        if (WriteFilesDialog->objectName().isEmpty())
            WriteFilesDialog->setObjectName(QStringLiteral("WriteFilesDialog"));
        WriteFilesDialog->resize(602, 445);
        WriteFilesDialog->setSizeGripEnabled(true);
        treeWidget = new QTreeWidget(WriteFilesDialog);
        QTreeWidgetItem *__qtreewidgetitem = new QTreeWidgetItem();
        __qtreewidgetitem->setText(2, QStringLiteral("3"));
        __qtreewidgetitem->setText(1, QStringLiteral("2"));
        __qtreewidgetitem->setText(0, QStringLiteral("1"));
        treeWidget->setHeaderItem(__qtreewidgetitem);
        treeWidget->setObjectName(QStringLiteral("treeWidget"));
        treeWidget->setGeometry(QRect(10, 10, 581, 361));
        treeWidget->setColumnCount(3);
        closeButton = new QPushButton(WriteFilesDialog);
        closeButton->setObjectName(QStringLiteral("closeButton"));
        closeButton->setGeometry(QRect(510, 410, 81, 23));
        progressBar = new QProgressBar(WriteFilesDialog);
        progressBar->setObjectName(QStringLiteral("progressBar"));
        progressBar->setGeometry(QRect(10, 380, 581, 23));
        progressBar->setValue(24);
        progressBar->setTextVisible(false);
        diskProgressBar = new QProgressBar(WriteFilesDialog);
        diskProgressBar->setObjectName(QStringLiteral("diskProgressBar"));
        diskProgressBar->setGeometry(QRect(10, 410, 321, 23));
        diskProgressBar->setValue(24);

        retranslateUi(WriteFilesDialog);

        QMetaObject::connectSlotsByName(WriteFilesDialog);
    } // setupUi

    void retranslateUi(QDialog *WriteFilesDialog)
    {
        WriteFilesDialog->setWindowTitle(QApplication::translate("WriteFilesDialog", "Dialog", 0));
        closeButton->setText(QApplication::translate("WriteFilesDialog", "Close", 0));
    } // retranslateUi

};

namespace Ui {
    class WriteFilesDialog: public Ui_WriteFilesDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_WRITEFILESDIALOG_H
