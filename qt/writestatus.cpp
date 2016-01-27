#include "writestatus.h"
#include "ui_writestatus.h"
#include <QApplication>
#include <QFileInfo>
#include <QMessageBox>
#include "mainwindow.h"

//bytes will always be less than 65500
void WriteStatus::write_callback(void *data,uint32_t bytes)
{
    WriteStatus *fw = (WriteStatus*)data;
    static int side = 0;

    if(bytes & 0x10000000) {
        side = (bytes >> 24) & 0xF;
    }
    fw->ui->progressBar->setValue(bytes + (side * 65500));
    qApp->processEvents();
}

WriteStatus::WriteStatus(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WriteStatus)
{
    ui->setupUi(this);
}

WriteStatus::~WriteStatus()
{
    delete ui;
}

void WriteStatus::write(QString filename)
{
    int sides;
    QString str;
    QFileInfo fileinfo(filename);

    sides = FDS_getDiskSides((char*)filename.toStdString().c_str());
    ui->label->setText(fileinfo.fileName());
    ui->label->adjustSize();
    ui->progressBar->setRange(0,65500 * sides);
    ui->progressBar->setValue(0);
    show();

    if(write_flash((char*)filename.toStdString().c_str(), -1, this, write_callback) == false) {
        hide();
        exec();
    }

    else {
        hide();
    }
}

void WriteStatus::writeloader(QString filename)
{
    QString str;

    ui->label->setText("Updating loader...");
    ui->label->adjustSize();
    ui->progressBar->setRange(0,65500);
    ui->progressBar->setValue(0);
    show();

    if(write_flash((char*)filename.toStdString().c_str(), -2, this, write_callback) == false) {
        hide();
        exec();
    }

    else {
        hide();
    }
}

bool loadfile(char *filename, uint8_t **buf, int *filesize);

void WriteStatus::writefirmware(QString filename)
{
    QString str;
//    QFileInfo fileinfo(filename);

    ui->label->setText("Updating firmware...");
    ui->label->adjustSize();
    ui->progressBar->setRange(0,0x8000);
    ui->progressBar->setValue(0);
    show();

    uint8_t *firmware;
    uint8_t *buf;
    uint32_t *buf32;
    uint32_t chksum;
    int filesize, i;

    //try to load the firmware image
    if (loadfile((char*)filename.toStdString().c_str(), &firmware, &filesize) == false) {
        hide();
        return;
    }

    //create new buffer to hold 32kb of data and clear it
    buf = new uint8_t[0x8000];
    memset(buf, 0, 0x8000);

    //copy firmware loaded to the new buffer
    memcpy(buf, firmware, filesize);
    buf32 = (uint32_t*)buf;

    //free firmware data
    delete[] firmware;

    //insert firmware identifier
    buf32[(0x8000 - 8) / 4] = 0xDEADBEEF;

    //calculate the simple xor checksum
    chksum = 0;
    for (i = 0; i < (0x8000 - 4); i += 4) {
        chksum ^= buf32[i / 4];
    }

    printf("firmware is %d bytes, checksum is $%08X\n", filesize, chksum);

    //insert checksum into the image
    buf32[(0x8000 - 4) / 4] = chksum;

    printf("uploading new firmware");
    //newer firmwares store the firmware image in sram to be updated
    if (dev.Version > 792) {
        printf("uploading new firmware to sram\n");
		if (!dev.Sram->Write(buf, 0x0000, 0x8000)) {
            printf("Write failed.\n");
            hide();
            delete[] buf;
            return;
		}
	}

	//older firmware store the firmware image into flash memory
	else {
		printf("uploading new firmware to flash");
        if (!dev.Flash->Write(buf, 0x8000, 0x8000, write_callback, this)) {
            printf("Write failed.\n");
            hide();
            delete[] buf;
            return;
		}
		printf("\n");
	}
    delete[] buf;

    ui->progressBar->setValue(0x8000);
    ui->label->setText("Rebooting device...please wait...");
    ui->label->adjustSize();
    qApp->processEvents();
    printf("waiting for device to reboot\n");

    dev.UpdateFirmware();
    sleep_ms(5000);

    if (!dev.Open()) {
        QMessageBox::information(NULL,"Error","Error re-opening device.  Try reinserting it and run this program again.");
        hide();
        return;
    }

    str.sprintf("Updated to build %d\n", dev.Version);
    QMessageBox::information(NULL,"Update success",str);

    hide();

}
