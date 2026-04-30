#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_textEdit_cursorPositionChanged();

    void on_actionNew_triggered();

    bool on_actionSave_triggered();

    bool on_actionSave_As_triggered();

    void on_actionOpen_triggered();

    void on_actionExit_triggered();

    void on_actionCut_triggered();

    void on_actionCopy_triggered();

    void on_actionPaste_triggered();

    void on_actionUndo_triggered();

    void on_actionRedo_triggered();

    void on_actionFont_triggered();

    void on_actionText_Color_triggered();

    void on_actionBg_Color_triggered();

    void on_actionAbout_triggered();

    void on_actionPrint_triggered();

    void on_actionZoom_In_triggered();

    void on_actionZoom_Out_triggered();

    void on_actionRestor_to_Normal_Zoom_triggered();

private:
    Ui::MainWindow *ui;

    bool maybeSave();
};
#endif // MAINWINDOW_H
