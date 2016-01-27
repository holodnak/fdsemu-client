#ifndef WRITEFILESDIALOG_H
#define WRITEFILESDIALOG_H

#include <QDialog>
#include <stdint.h>

namespace Ui {
class WriteFilesDialog;
}

class WriteFilesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WriteFilesDialog(QWidget *parent = 0);
    ~WriteFilesDialog();
    void addFiles(QStringList &list);
    void writeFiles(QStringList &list);

protected:
    int progressBarBase;
    int diskProgressBarBase;

private slots:
    void on_closeButton_clicked();

private:
    Ui::WriteFilesDialog *ui;
    static void write_callback(void *data,uint32_t bytes);
};

#endif // WRITEFILESDIALOG_H
