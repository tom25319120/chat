#ifndef TCPSERVER_H
#define TCPSERVER_H

#include "qdialog.h"
#include "qdir.h"
#include "qelapsedtimer.h"
#include <QWidget>
#include<QTcpServer>
namespace Ui {
class tcpserver;
}

class TcpServer : public QDialog
{
    Q_OBJECT

public:
    explicit TcpServer(QDialog *parent = nullptr);
    ~TcpServer();
    void initserve();
    void sendMessage();

signals:
    void sendFileName(QString);
    void processchange();
private slots:
    void on_openBtn_clicked();

    void on_sendBtn_clicked();
    void on_closeBtn_clicked();
    void sendtext();
    void updataprocess();
private:
    Ui::tcpserver *ui;
    QTcpServer *tcpServer;
    QTcpSocket *clientConnection;
    qint16 tcpPort;
    QFile *localFile ;
    qint64 filesize;
    qint64 payloadSize ;
    qint64 TotalBytes ;
    qint64 bytesWritten ;
    qint64 bytestoWrite;

    QString theFileName;
    QString fileName;

    QTcpSocket* nexttcp;
    QElapsedTimer time;
    QByteArray outBlock;

};

#endif // TCPSERVER_H
