#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include "qdir.h"
#include "qelapsedtimer.h"
#include <QTcpSocket>
#include <QWidget>
namespace Ui {
class tcpclient;
}

class tcpclient : public QWidget
{
    Q_OBJECT

public:
    explicit tcpclient(QWidget *parent = 0);
    ~tcpclient();

    void setHostAddress(QHostAddress address);
    void setPath(QString path);
    void closeEvent(QCloseEvent *e);
    void newConnect();

    void updataprocess();
    QFile *localFile;
signals:
    void processChange();
private slots:


    void readMessage();

    void displayError(QAbstractSocket::SocketError e);


    void on_cancelBtn_clicked();

    void on_closeBtn_clicked();

private:
    Ui::tcpclient *ui;

    QTcpSocket *tcpClient;
    qint16  tcpPort ;
    qint64 payloadSize ;
    QHostAddress hostAddress;

    qint64 TotalBytes ;
    qint64 bytesReceived;
    qint64 fileNameSize ;
    qint64 blockSize;
    QString fileName;
    QString path;
    QElapsedTimer time;
    char* inBlock;

};

#endif // TCPCLIENT_H
