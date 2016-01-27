#include <stdint.h>
#include "diskreaddialog.h"
#include "ui_diskreaddialog.h"

DiskReadDialog::DiskReadDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DiskReadDialog)
{
    ui->setupUi(this);
    ui->saveButton->setEnabled(false);
    ui->plainTextEdit->setPlainText(QString("Insert disk and press 'Read disk...' button."));
}

DiskReadDialog::~DiskReadDialog()
{
    delete ui;
}

void DiskReadDialog::readdisk()
{
}

void DiskReadDialog::on_cancelButton_clicked()
{
    this->close();
}

bool FDS_readDisk(char *filename_raw, char *filename_bin, char *filename_fds, void(*callback)(void*,int), void *data);
bool FDS_readDisk2(QStringList *messages, uint8_t **rawbuf, int *rawlen, uint8_t **binbuf, int *binlen, void(*callback)(void*,int), void *data);

void DiskReadDialog::on_pushButton_clicked()
{
    QString str;
    QStringList messages;
    uint8_t *rawbuf, *binbuf;
    int rawlen, binlen;
    int i;

    rawbuf = binbuf = 0;
    rawlen = binlen = 0;

    ui->plainTextEdit->setPlainText("");
    ui->saveButton->setEnabled(false);
    ui->cancelButton->setEnabled(false);
    ui->pushButton->setEnabled(false);
    qApp->processEvents();

//    FDS_readDisk(0,0,0,0,0);
    messages.clear();
    if(FDS_readDisk2(&messages,&rawbuf,&rawlen,&binbuf,&binlen,0,0) == false) {
        messages.append("Read failed.");
    }
    qApp->processEvents();
    ui->plainTextEdit->clear();
    for(i=0;i<messages.size();i++) {
        ui->plainTextEdit->moveCursor(QTextCursor::End);
        ui->plainTextEdit->insertPlainText(messages.at(i));
        ui->plainTextEdit->moveCursor(QTextCursor::End);
    }
//    ui->plainTextEdit->setPlainText(str);
    ui->saveButton->setEnabled(true);
    ui->cancelButton->setEnabled(true);
    ui->pushButton->setEnabled(true);
}
