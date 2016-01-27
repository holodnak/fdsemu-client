#ifndef DISKREADDIALOG_H
#define DISKREADDIALOG_H

#include <QDialog>

namespace Ui {
class DiskReadDialog;
}

class DiskReadDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DiskReadDialog(QWidget *parent = 0);
    ~DiskReadDialog();
    void readdisk();

private slots:
    void on_cancelButton_clicked();

    void on_pushButton_clicked();

private:
    Ui::DiskReadDialog *ui;
};

#endif // DISKREADDIALOG_H
