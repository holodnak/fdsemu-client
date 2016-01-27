#ifndef WRITESTATUS_H
#define WRITESTATUS_H

#include <QDialog>
#include <stdint.h>

namespace Ui {
class WriteStatus;
}

class WriteStatus : public QDialog
{
    Q_OBJECT

public:
    explicit WriteStatus(QWidget *parent = 0);
    ~WriteStatus();

    void write(QString filename);
    void writeloader(QString filename);
    void writefirmware(QString filename);

private:
    Ui::WriteStatus *ui;
    static void write_callback(void *data,uint32_t bytes);
};

#endif // WRITESTATUS_H
