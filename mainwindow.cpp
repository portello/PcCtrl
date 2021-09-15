#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QNetworkReply>
#include <QFile>
#include <QFileDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connStateChanged(false);
    connect(&f_netManager, &QNetworkAccessManager::finished, this, &MainWindow::netManagerFinished);


    ui->cbGtpAligned->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->cbGtpAligned->setFocusPolicy(Qt::NoFocus);
    ui->cbDdrLoaded->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->cbDdrLoaded->setFocusPolicy(Qt::NoFocus);
    ui->cbBusy->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    ui->cbBusy->setFocusPolicy(Qt::NoFocus);


    f_timer.setSingleShot(false);
    f_timer.setInterval(500);

    connect(&f_timer, &QTimer::timeout, this, &MainWindow::timerTimeout);
}

MainWindow::~MainWindow()
{
    delete ui;

}

void MainWindow::connStateChanged(bool connected)
{
    if (connected)
    {
        if (f_conn != nullptr)
        {
            ui->tbConnect->setText("Disconnect");
            QString ver0 = f_conn->execCommand("reg 0", 400);
            qDebug() << "ver0 = " << ver0;
            ver0 = ver0.left(ver0.indexOf("\n"));
            QString ver1 = f_conn->execCommand("reg 1", 400);
            qDebug() << "ver1 = " << ver1;
            ver1 = ver1.left(ver1.indexOf("\n"));
            if (ver0.isEmpty() && ver1.isEmpty())
                ui->lblVersion->setText(QString("Connection Error"));
            else
            {
                ui->lblVersion->setText(QString("Version: %1 %2").arg(ver0.toLong(nullptr, 0)).arg(ver1.toLong(nullptr, 0)));
                on_cb8b10b_stateChanged(ui->cb8b10b->checkState());
            }
        }
        f_timer.start();
    }
    else
    {
        f_timer.stop();
        ui->tbConnect->setText("Connect");
        ui->lblVersion->setText(QString("Version:"));
    }
}


void MainWindow::handle_action_Connect()
{
    if (f_connected)
    {
        connStateChanged(false);
        f_connected = false;
//        emit S_disconnected(f_prog);
        if (f_conn != nullptr)
            f_conn->closeAndDelete();
        f_conn = nullptr;
    }
    else
    {
        {
            QHostAddress host = QHostAddress(ui->edIP->text());
            quint16 port = 8090;

            // Main connection
            QIODevice *ioDev = makeLanConnection(host, port);
            if (ioDev == nullptr)
            {
                connStateChanged(false);
                QApplication::restoreOverrideCursor();
                QMessageBox::information(this, tr("Connection"), tr("Connection Error"), QMessageBox::Ok);
                return;
            }
            f_conn = new HydraConn(ioDev, this);

            // Connection for check programmer status
            connect(f_conn, SIGNAL(S_logLine(QString,int)), this, SLOT(logLine(QString,int)));
        }
//        emit S_connected(f_conn);
        f_connected = true;
        connStateChanged(true);
    }

}

void MainWindow::on_tbConnect_clicked()
{
    handle_action_Connect();
}

void MainWindow::timerTimeout()
{
    if (f_conn != nullptr && !f_busy)
    {
        QString resp = f_conn->execCommand("reg 5", 200);
        bool ok;
        quint8 reg = resp.left(resp.indexOf("\n")).toUInt(&ok, 0);
        if (ok)
        {
            ui->cbBusy->setChecked(reg & 0x01);
            ui->cbDdrLoaded->setChecked(reg & 0x10);
            ui->cbGtpAligned->setChecked(reg & 0x80);
        }
    }
}

void MainWindow::logLine(const QString &line, int elapsed)
{
    Q_UNUSED(elapsed);
    long step = line.toLong();
    if (step == 0)
        ui->progressBar->setValue(0);
    else
        ui->progressBar->setValue(step);
    if (step == 100)
        f_busy = false;
}

void MainWindow::on_pbSaveData_clicked()
{
    if (f_conn != nullptr && f_connected)
    {
        ui->progressBar->setValue(0);
        ui->cbBusy->setChecked(true);
        f_busy = true;
        f_conn->sendCmd("bin rd 0x00000");
    }
}

void MainWindow::on_pbGetData_clicked()
{
    f_downloading = true;

    QFileDialog sfd(this, tr("Export ATE configuration"), QString(), tr("All Files(*)"));
    sfd.setDefaultSuffix(QStringLiteral("bin"));
    sfd.selectFile("out.bin");
    sfd.setAcceptMode(QFileDialog::AcceptSave);
    if (sfd.exec())
        f_fileName = sfd.selectedFiles().constFirst();
    else
        return;

    QString addr = QString("http://%1/out.bin").arg(ui->edIP->text());
//    QNetworkReply *reply = f_netManager.get(QNetworkRequest(QUrl(QStringLiteral("http://www.nova-flash.com/wp-content/uploads/novaflash.db"))));
    QNetworkReply *reply = f_netManager.get(QNetworkRequest(QUrl(addr)));
    connect(reply, &QNetworkReply::downloadProgress, this, &MainWindow::downloadProgress);
}


void MainWindow::downloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    Q_UNUSED(bytesTotal)
    ui->progressBar->setValue((bytesReceived * 100 / bytesTotal));
}


void MainWindow::netManagerFinished(QNetworkReply *netReply)
{
    if (!f_downloading)
    {
        if (netReply->operation() == QNetworkAccessManager::HeadOperation)
        {
            int content_length = netReply->header(QNetworkRequest::ContentLengthHeader).toInt();
//            QDate date = netReply->header(QNetworkRequest::LastModifiedHeader).toDate();
            f_downloadSize = content_length;
//            ui->progressBar->setMaximum(f_downloadSize);
            netReply->deleteLater();
        }
    }
    else
    {
        netReply->deleteLater();
        f_downloading = false;
        if (netReply->error())
        {
            QMessageBox msgBox;
            msgBox.setText(tr("Download failed: %1").arg(netReply->errorString()));
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.exec();
        }
        else
        {

            QFile file(QFileInfo(f_fileName).absoluteFilePath());
            file.open(QIODevice::WriteOnly | QIODevice::Truncate);
            if (!file.isOpen() || !file.isWritable())
            {
                QMessageBox msgBox;
                msgBox.setText(tr("Error saving file"));
                msgBox.setIcon(QMessageBox::Critical);
                msgBox.exec();
                file.close();
            }
            file.write(netReply->readAll());
            file.close();
        }
    }
}

void MainWindow::on_cb8b10b_stateChanged(int arg1)
{
    if (arg1)
    {
        logText(f_conn->execCommand("reg 6 1"));
    }
    else
    {
        logText(f_conn->execCommand("reg 6 0"));
    }
}

void MainWindow::on_cbGtp_currentIndexChanged(int index)
{
    if (f_conn == nullptr)
        return;

    switch (index)
    {
        case 0:
            logText(f_conn->execCommand("gtp 1"));
            break;
        case 1:
            logText(f_conn->execCommand("gtp 2"));
            break;
        case 2:
            logText(f_conn->execCommand("gtp 4"));
            break;
        case 3:
            logText(f_conn->execCommand("gtp 5"));
            break;
    }
}

void MainWindow::logText(QString text)
{
    ui->edLog->appendPlainText(text);
}

void MainWindow::on_pbStartAcq_clicked()
{
    if (f_conn != nullptr)
    {
        ui->cbBusy->setChecked(true);
        f_conn->sendCmd("cmd 0x6D");
    }
}
