/********************************************************************************
** Form generated from reading UI file 'writestatus.ui'
**
** Created by: Qt User Interface Compiler version 5.5.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_WRITESTATUS_H
#define UI_WRITESTATUS_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>

QT_BEGIN_NAMESPACE

class Ui_WriteStatus
{
public:
    QLabel *label;
    QProgressBar *progressBar;

    void setupUi(QDialog *WriteStatus)
    {
        if (WriteStatus->objectName().isEmpty())
            WriteStatus->setObjectName(QStringLiteral("WriteStatus"));
        WriteStatus->resize(400, 102);
        label = new QLabel(WriteStatus);
        label->setObjectName(QStringLiteral("label"));
        label->setGeometry(QRect(20, 20, 361, 21));
        progressBar = new QProgressBar(WriteStatus);
        progressBar->setObjectName(QStringLiteral("progressBar"));
        progressBar->setGeometry(QRect(20, 60, 361, 23));
        progressBar->setValue(24);

        retranslateUi(WriteStatus);

        QMetaObject::connectSlotsByName(WriteStatus);
    } // setupUi

    void retranslateUi(QDialog *WriteStatus)
    {
        WriteStatus->setWindowTitle(QApplication::translate("WriteStatus", "Writing...", 0));
        label->setText(QApplication::translate("WriteStatus", "TextLabel", 0));
    } // retranslateUi

};

namespace Ui {
    class WriteStatus: public Ui_WriteStatus {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_WRITESTATUS_H
