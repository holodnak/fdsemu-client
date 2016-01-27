/********************************************************************************
** Form generated from reading UI file 'diskreaddialog.ui'
**
** Created by: Qt User Interface Compiler version 5.5.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DISKREADDIALOG_H
#define UI_DISKREADDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>

QT_BEGIN_NAMESPACE

class Ui_DiskReadDialog
{
public:
    QPlainTextEdit *plainTextEdit;
    QPushButton *pushButton;
    QPushButton *saveButton;
    QPushButton *cancelButton;

    void setupUi(QDialog *DiskReadDialog)
    {
        if (DiskReadDialog->objectName().isEmpty())
            DiskReadDialog->setObjectName(QStringLiteral("DiskReadDialog"));
        DiskReadDialog->resize(612, 398);
        plainTextEdit = new QPlainTextEdit(DiskReadDialog);
        plainTextEdit->setObjectName(QStringLiteral("plainTextEdit"));
        plainTextEdit->setGeometry(QRect(10, 10, 591, 341));
        plainTextEdit->setUndoRedoEnabled(false);
        plainTextEdit->setReadOnly(true);
        pushButton = new QPushButton(DiskReadDialog);
        pushButton->setObjectName(QStringLiteral("pushButton"));
        pushButton->setGeometry(QRect(320, 360, 91, 23));
        saveButton = new QPushButton(DiskReadDialog);
        saveButton->setObjectName(QStringLiteral("saveButton"));
        saveButton->setGeometry(QRect(414, 360, 101, 23));
        cancelButton = new QPushButton(DiskReadDialog);
        cancelButton->setObjectName(QStringLiteral("cancelButton"));
        cancelButton->setGeometry(QRect(520, 360, 75, 23));

        retranslateUi(DiskReadDialog);

        QMetaObject::connectSlotsByName(DiskReadDialog);
    } // setupUi

    void retranslateUi(QDialog *DiskReadDialog)
    {
        DiskReadDialog->setWindowTitle(QApplication::translate("DiskReadDialog", "Dialog", 0));
        plainTextEdit->setPlainText(QApplication::translate("DiskReadDialog", "Output", 0));
        pushButton->setText(QApplication::translate("DiskReadDialog", "Read disk...", 0));
        saveButton->setText(QApplication::translate("DiskReadDialog", "Save and close", 0));
        cancelButton->setText(QApplication::translate("DiskReadDialog", "Cancel", 0));
    } // retranslateUi

};

namespace Ui {
    class DiskReadDialog: public Ui_DiskReadDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_DISKREADDIALOG_H
