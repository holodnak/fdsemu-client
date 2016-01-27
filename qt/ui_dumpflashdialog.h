/********************************************************************************
** Form generated from reading UI file 'dumpflashdialog.ui'
**
** Created by: Qt User Interface Compiler version 5.5.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DUMPFLASHDIALOG_H
#define UI_DUMPFLASHDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>

QT_BEGIN_NAMESPACE

class Ui_DumpFlashDialog
{
public:
    QCheckBox *checkBox;
    QLineEdit *lineEdit;
    QLineEdit *lineEdit_2;
    QLabel *label;
    QLabel *label_2;
    QPushButton *pushButton;
    QPushButton *pushButton_2;
    QProgressBar *progressBar;
    QLabel *label_3;

    void setupUi(QDialog *DumpFlashDialog)
    {
        if (DumpFlashDialog->objectName().isEmpty())
            DumpFlashDialog->setObjectName(QStringLiteral("DumpFlashDialog"));
        DumpFlashDialog->resize(265, 174);
        checkBox = new QCheckBox(DumpFlashDialog);
        checkBox->setObjectName(QStringLiteral("checkBox"));
        checkBox->setGeometry(QRect(10, 10, 201, 17));
        lineEdit = new QLineEdit(DumpFlashDialog);
        lineEdit->setObjectName(QStringLiteral("lineEdit"));
        lineEdit->setGeometry(QRect(20, 60, 91, 20));
        lineEdit_2 = new QLineEdit(DumpFlashDialog);
        lineEdit_2->setObjectName(QStringLiteral("lineEdit_2"));
        lineEdit_2->setGeometry(QRect(150, 60, 91, 20));
        label = new QLabel(DumpFlashDialog);
        label->setObjectName(QStringLiteral("label"));
        label->setGeometry(QRect(20, 40, 71, 16));
        label_2 = new QLabel(DumpFlashDialog);
        label_2->setObjectName(QStringLiteral("label_2"));
        label_2->setGeometry(QRect(150, 40, 59, 13));
        pushButton = new QPushButton(DumpFlashDialog);
        pushButton->setObjectName(QStringLiteral("pushButton"));
        pushButton->setGeometry(QRect(100, 140, 75, 23));
        pushButton_2 = new QPushButton(DumpFlashDialog);
        pushButton_2->setObjectName(QStringLiteral("pushButton_2"));
        pushButton_2->setGeometry(QRect(180, 140, 75, 23));
        progressBar = new QProgressBar(DumpFlashDialog);
        progressBar->setObjectName(QStringLiteral("progressBar"));
        progressBar->setGeometry(QRect(10, 110, 241, 23));
        progressBar->setValue(24);
        label_3 = new QLabel(DumpFlashDialog);
        label_3->setObjectName(QStringLiteral("label_3"));
        label_3->setGeometry(QRect(10, 90, 241, 16));

        retranslateUi(DumpFlashDialog);

        QMetaObject::connectSlotsByName(DumpFlashDialog);
    } // setupUi

    void retranslateUi(QDialog *DumpFlashDialog)
    {
        DumpFlashDialog->setWindowTitle(QApplication::translate("DumpFlashDialog", "Dump Flash", 0));
        checkBox->setText(QApplication::translate("DumpFlashDialog", "Dump entire flash", 0));
        label->setText(QApplication::translate("DumpFlashDialog", "<html><head/><body><p>Start address</p></body></html>", 0));
        label_2->setText(QApplication::translate("DumpFlashDialog", "End address", 0));
        pushButton->setText(QApplication::translate("DumpFlashDialog", "Dump", 0));
        pushButton_2->setText(QApplication::translate("DumpFlashDialog", "Cancel", 0));
        label_3->setText(QApplication::translate("DumpFlashDialog", "TextLabel", 0));
    } // retranslateUi

};

namespace Ui {
    class DumpFlashDialog: public Ui_DumpFlashDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_DUMPFLASHDIALOG_H
