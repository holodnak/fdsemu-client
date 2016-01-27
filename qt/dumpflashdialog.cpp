#include <QMessageBox>
#include <QFileDialog>
#include "dumpflashdialog.h"
#include "ui_dumpflashdialog.h"
#include "mainwindow.h"

void DumpFlashDialog::read_callback(void *data,uint32_t bytes)
{
    DumpFlashDialog *fw = (DumpFlashDialog*)data;

    fw->ui->progressBar->setValue(bytes);
    qApp->processEvents();
}

DumpFlashDialog::DumpFlashDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DumpFlashDialog)
{
    ui->setupUi(this);
    ui->progressBar->setValue(0);
    ui->progressBar->setRange(0,1);
    ui->label_3->setText("");
}

DumpFlashDialog::~DumpFlashDialog()
{
    delete ui;
}

static bool qstring_to_uint32(QString str, uint32_t *num)
{
    char *s = (char*)str.toStdString().c_str();
    char *end = 0;
    int base = 10;

    //hex
    if(s[0] == '$') {
        base = 16;
        s++;
    }

    //hex alternate format
    else if(s[0] == '0' && tolower(s[1]) == 'x') {
        base = 16;
        s+= 2;
    }

    *num = strtoul(s,&end,base);

    //validate it is correct input
    if(base == 10) {
        if(s[strspn(s, "0123456789")] == 0) {
            return(true);
        }
    }
    else {
        if(s[strspn(s, "0123456789abcdefABCDEF")] == 0) {
            return(true);
        }
    }
    return(false);
}

void DumpFlashDialog::on_pushButton_clicked()
{
    uint32_t start, end, len;
    QString str, filename;
    uint8_t *buf;

    ui->progressBar->setValue(0);
    ui->progressBar->setRange(0,1);
    start = end = 0;

    qApp->processEvents();

    //see if we are dumping the whole flash
    if(ui->checkBox->isChecked()) {
        start = 0;
        end = dev.FlashSize - 1;
    }

    //only dumping a range
    else {
        if(qstring_to_uint32(ui->lineEdit->text(),&start) == false || qstring_to_uint32(ui->lineEdit_2->text(),&end) == false) {
            QMessageBox::information(NULL,"Formatting error","Please use decimal or hexidecimal numbers only.\n\nFor hexidecimal please prefix the number with '$' or '0x', for example: $8000 or 0x8000.");
            return;
        }
    }

    if(start >= end) {
        QMessageBox::information(NULL,"Invalid range","Please specify a range of bytes larger than 0");
        return;
    }

//    str.sprintf("start = %X\nend = %X",start,end);
//    QMessageBox::information(NULL,"info",str);
    ui->pushButton->setEnabled(false);
    ui->pushButton_2->setEnabled(false);
    str = "";
    filename = QFileDialog::getSaveFileName(this, tr("Save Dump File"),str,tr("FDSemu flash dumps (*.dump)"));
    if(filename != NULL) {
        FILE *fp;

        filename = QDir::toNativeSeparators(filename);
        if((fp = fopen(filename.toStdString().c_str(),"wb")) != 0) {
            str.sprintf("Dumping from $%X to $%X, please wait...",start,end);
            ui->label_3->setText(str);
            qApp->processEvents();

            len = end - start;
            ui->progressBar->setRange(0,len);
            buf = new uint8_t[len];
            dev.Flash->Read(buf,start,len,read_callback, this);
            fwrite(buf,len,1,fp);
            fclose(fp);
            delete[] buf;
        }
        else {
            str.sprintf("Error opening output file: %s", filename.toStdString().c_str());
            QMessageBox::information(NULL,"Error",str);
        }
        close();
    }
}

void DumpFlashDialog::on_pushButton_2_clicked()
{
    close();
}

void DumpFlashDialog::on_checkBox_stateChanged(int arg1)
{
    if(arg1) {
        ui->lineEdit->setEnabled(false);
        ui->lineEdit_2->setEnabled(false);
    }
    else {
        ui->lineEdit->setEnabled(true);
        ui->lineEdit_2->setEnabled(true);
    }
}
