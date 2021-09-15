#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QTimer>
#include "hydraLib.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_tbConnect_clicked();
    void logLine(const QString &line, int elapsed);
    void on_pbSaveData_clicked();
    void on_pbGetData_clicked();
    void on_cb8b10b_stateChanged(int arg1);
    void on_cbGtp_currentIndexChanged(int index);
    void on_pbStartAcq_clicked();
    void timerTimeout();

private:
    Ui::MainWindow *ui;

    HydraConn               *f_conn{nullptr};
    bool                    f_connected{false};
    QNetworkAccessManager   f_netManager;
    bool                    f_downloading{false};
    quint64                 f_downloadSize{0};
    QString                 f_fileName;
    QTimer                  f_timer;
    bool                    f_busy{false};

    void connStateChanged(bool connected);
    void handle_action_Connect();
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void netManagerFinished(QNetworkReply *netReply);
    void logText(QString text);
};
#endif // MAINWINDOW_H
