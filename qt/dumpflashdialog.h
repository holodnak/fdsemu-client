#ifndef DUMPFLASHDIALOG_H
#define DUMPFLASHDIALOG_H

#include <QDialog>
#include <stdint.h>

namespace Ui {
class DumpFlashDialog;
}

class DumpFlashDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DumpFlashDialog(QWidget *parent = 0);
    ~DumpFlashDialog();

private slots:
    void on_pushButton_clicked();

    void on_pushButton_2_clicked();

    void on_checkBox_stateChanged(int arg1);

private:
    static void read_callback(void *data,uint32_t bytes);
    Ui::DumpFlashDialog *ui;
};

#endif // DUMPFLASHDIALOG_H
