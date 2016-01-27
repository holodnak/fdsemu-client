#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <stdint.h>
#include "Device.h"
#include "System.h"

extern CDevice dev;

bool write_flash(char *filename, int slot, void *data, void(*callback)(void*,uint32_t));
int FDS_getDiskSides(char *filename);

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    QLabel *statusLabel;

protected:
    void updateList();
    void openFiles(QStringList &list);
    void dragEnterEvent(QDragEnterEvent *event) Q_DECL_OVERRIDE;
    void dropEvent(QDropEvent *event) Q_DECL_OVERRIDE;

private slots:
    void on_actionE_xit_triggered();

    void on_action_Delete_triggered();

    void on_action_About_triggered();

    void on_action_Write_disk_triggered();

    void on_action_Write_disk_image_triggered();

    void on_action_Save_disk_image_triggered();

    void on_action_Save_triggered();

    void on_action_Erase_triggered();

    void on_actionUpdate_firmware_triggered();

    void on_action_Read_disk_triggered();

    void on_action_Dump_flash_triggered();

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
