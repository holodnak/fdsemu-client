#include <QFileInfo>
#include <QFileInfoList>
#include <QMessageBox>
#include "writefilesdialog.h"
#include "ui_writefilesdialog.h"
#include "mainwindow.h"

void WriteFilesDialog::write_callback(void *data,uint32_t bytes)
{
    WriteFilesDialog *fw = (WriteFilesDialog*)data;
    static int side = 0;

    if(bytes & 0x10000000) {
        side = (bytes >> 24) & 0xF;
    }
    fw->ui->progressBar->setValue(bytes + fw->progressBarBase + (side * 65500));
    fw->ui->diskProgressBar->setValue(bytes + (side * 65500));
    qApp->processEvents();
}

WriteFilesDialog::WriteFilesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WriteFilesDialog)
{
    ui->setupUi(this);
}

WriteFilesDialog::~WriteFilesDialog()
{
    delete ui;
}

bool write_doctor(char *file, void *data, void(*callback)(void*,uint32_t));
bool is_gamedoctor(char *filename);
bool is_fwnes(char *filename);

bool is_valid_diskimage(char *filename)
{
    if(is_gamedoctor(filename) || is_fwnes(filename))
        return(true);
    return(false);
}

void WriteFilesDialog::addFiles(QStringList &list)
{
    int i;
    int size = 0, totalsize = 0;
    bool success = false;

    QStringList labels;
    QFileInfo fileinfo;

    labels << "Status" << "Sides" << "Filename";
//    QHeaderView *header = new QHeaderView;

    progressBarBase = 0;
    ui->treeWidget->setHeaderLabels(labels);
    ui->treeWidget->header()->resizeSection(1,50);
//    ui->treeWidget->setHeader(header);
    for(i=0;i<list.size();i++) {
        QString filename = list.at(i);
        QString str;
        QVariant v;

        fileinfo.setFile(filename);
//        ui->listWidget->addItem(fileinfo.fileName());
        QTreeWidgetItem *item;

        str.sprintf("%d", (int)(fileinfo.size() / 65500));
        item = new QTreeWidgetItem();
        item->setText(0,"");
        item->setText(1,str);
        item->setText(2,fileinfo.fileName());
        if(is_valid_diskimage((char*)filename.toStdString().c_str()) == true) {
            totalsize += (fileinfo.size() / 65500) * 65500;
        }
        else {
            filename = "";
        }
        v.setValue(filename);
        item->setData(0,Qt::UserRole,v);
        ui->treeWidget->addTopLevelItem(item);
    }

    ui->progressBar->setRange(0,totalsize);
    ui->progressBar->setValue(0);
    ui->diskProgressBar->setValue(0);

    for(i = 0; i < ui->treeWidget->topLevelItemCount(); ++i ) {
        QTreeWidgetItem *item = ui->treeWidget->topLevelItem( i );
        QString filename;
        QVariant v;

        v = item->data(0,Qt::UserRole);
        filename = v.value<QString>();
        if(filename == "") {
            item->setText(0,"Bad format");
            continue;
        }
        fileinfo.setFile(filename);

        ui->diskProgressBar->setRange(0,(fileinfo.size() / 65500) * 65500);
        ui->diskProgressBar->setValue(0);
        item->setText(0,"Writing...");

        if (is_gamedoctor((char*)filename.toStdString().c_str())) {
//            printf("Detected Game Doctor image.\n");
            success = write_doctor((char*)filename.toStdString().c_str(), this, write_callback);
        }
        else {
            success = write_flash((char*)filename.toStdString().c_str(), -1, this, write_callback);
        }

        item->setText(0,(success == false) ? "Failed" : "Success");

        ui->diskProgressBar->setValue((fileinfo.size() / 65500) * 65500);
        progressBarBase += (fileinfo.size() / 65500) * 65500;
        ui->progressBar->setValue(progressBarBase);
    }

    ui->progressBar->setValue(totalsize);
    qApp->processEvents();
}

void WriteFilesDialog::writeFiles(QStringList &list)
{
    show();
    qApp->processEvents();

    ui->closeButton->setEnabled(false);
    addFiles(list);
    ui->closeButton->setEnabled(true);

    exec();
}

void WriteFilesDialog::on_closeButton_clicked()
{
    close();
}
