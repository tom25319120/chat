#include "tcpclient.h"
#include "qmessagebox.h"
#include "qthread.h"
#include "ui_tcpclient.h"

#include <QTimer>

tcpclient::tcpclient(QWidget *parent)
    : QWidget(parent)
    ,localFile(nullptr)
    ,ui(new Ui::tcpclient)
    ,tcpClient(NULL)
    ,payloadSize(1024)
{
    ui->setupUi(this);
    TotalBytes = 0;
    bytesReceived = 0;
    fileNameSize = 0;
    tcpClient=new QTcpSocket;
    tcpPort=6666;
    connect(tcpClient,&QTcpSocket::readyRead,this,&tcpclient::readMessage);
    connect(tcpClient,&QTcpSocket::errorOccurred,this,&tcpclient::displayError);
    setWindowTitle("接受文件");
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);
    connect(this,&tcpclient::processChange,this,&tcpclient::updataprocess);
}

tcpclient::~tcpclient()
{
    delete ui;
}
void tcpclient::setHostAddress(QHostAddress address)
{
    this->hostAddress=address;
}

void tcpclient::setPath(QString path)
{
    this->path=path;
}

void tcpclient::closeEvent(QCloseEvent *e)
{
    on_closeBtn_clicked();
}

void tcpclient::readMessage()
{
    // 添加安全检查
    if(!tcpClient || tcpClient->state() != QAbstractSocket::ConnectedState)
    {
        qDebug() << "Socket未连接";
        return;
    }

    // 确保文件对象存在
    if(!localFile)
    {
        localFile = new QFile(path);
        qDebug() << "创建文件对象，保存路径:" << path;
    }

    QDataStream in(tcpClient);
    in.setVersion(QDataStream::Qt_4_0);

    // 接收文件头
    if(bytesReceived <= sizeof(qint64) * 2)
    {
        if(tcpClient->bytesAvailable() >= sizeof(qint64) * 2 && fileNameSize == 0)
        {
            in >> TotalBytes >> fileNameSize;
            qDebug() << "收到文件头: TotalBytes=" << TotalBytes
                     << "fileNameSize=" << fileNameSize;

            if(TotalBytes <= 0 || fileNameSize <= 0 || fileNameSize > 1024)
            {
                qDebug() << "无效的文件头数据!";
                QMessageBox::warning(this, "错误", "接收的文件头数据无效！");
                return;
            }

            bytesReceived += sizeof(qint64) * 2;
        }

        if(tcpClient->bytesAvailable() >= fileNameSize && fileNameSize != 0)
        {
            in >> fileName;

            qDebug() << "收到文件名:" << fileName;
            bytesReceived += fileNameSize;

            if(!localFile->open(QIODevice::WriteOnly))
            {
                QMessageBox::warning(this, "错误",
                                     tr("无法创建文件：%1\n错误信息：%2")
                                         .arg(path)  // ✅ 使用 path
                                         .arg(localFile->errorString()));
                qDebug() << "文件打开失败:" << localFile->errorString();
                return;
            }
            qDebug() << "文件已创建并打开，准备接收数据";
        }
    }

    // 接收文件数据
    if(bytesReceived < TotalBytes && fileNameSize != 0 && localFile && localFile->isOpen())
    {
        qint64 bytesAvailable = tcpClient->bytesAvailable();
        while(bytesAvailable > 0)
        {
            qint64 inBlocksize = qMin(payloadSize, bytesAvailable);
            QByteArray buff;
            buff.resize(inBlocksize);
            qint64 bytesRead = tcpClient->read(buff.data(), inBlocksize);

            if(bytesRead > 0)
            {
                qint64 bytesWritten = localFile->write(buff.data(), bytesRead);
                if(bytesRead == bytesWritten)
                {
                    bytesReceived += bytesWritten;
                    bytesAvailable-=bytesWritten;
                    if(bytesReceived > 0 && TotalBytes > 0)
                    {
                        double progress = (double)bytesReceived / TotalBytes * 100;
                        int intProgress = (int)progress;

                        // 限制范围
                        if(intProgress > 100) intProgress = 100;
                        if(intProgress < 0) intProgress = 0;

                        ui->progressBar->setValue(intProgress);
                    }
                }
            }

        }
    }
    // 发射信号更新显示
    static qint64 lastBytesReceived = 0;
    if(bytesReceived != lastBytesReceived)
    {
        lastBytesReceived = bytesReceived;
        emit processChange();
    }
    QThread::msleep(100);
}

void tcpclient::displayError(QAbstractSocket::SocketError e)
{
    if(e==QAbstractSocket::RemoteHostClosedError)
    {
        return;
    }
    else{
        QMessageBox::warning(this,"错误",tr("出现错误：%1").arg(tcpClient->errorString().toStdString()));
    }
}

void tcpclient::newConnect()
{
    tcpClient->abort();
    blockSize=0;
    tcpClient->connectToHost(hostAddress,tcpPort);
    time.start();
}

void tcpclient::updataprocess()
{
    double usetime=time.elapsed();
    if(usetime>0)
    {
        double speed=bytesReceived/usetime;
        ui->label->setText( tr("已接收 %1MB (%2MB/s)\n"
                              "共%3MB 已用时:%4秒\n"
                              "估计剩余时间:%5秒")
                               .arg(bytesReceived/1024.0/1024.0,0,'f',2)
                               .arg(speed/1000.0/1024.0/1024.0,0,'f',2)
                               .arg(TotalBytes/1024.0/1024.0,0,'f',2)
                               .arg(usetime/1000.0,0,'f',2)
                               .arg((TotalBytes-bytesReceived)/speed/1000.0,0,'f',0)
                           );
    }
    // 5. 如果接收完成
    // 5.1 关闭文件和socket
    // 5.2 显示完成信息
    if(bytesReceived >= TotalBytes && TotalBytes > 0)
    {
        qDebug() << "文件接收完成！";

        if(localFile) {
            localFile->flush();  // 强制刷新缓冲区
            localFile->close();
            delete localFile;
            localFile = nullptr;
        }

        tcpClient->close();
        ui->label->setText(tr("接收文件: %1 完毕").arg(fileName));
        int flag=QMessageBox::information(this, "完成", tr("文件接收成功！\n保存位置：%1").arg(path));
        if(flag==QMessageBox::Ok)close();
    }
}

void tcpclient::on_cancelBtn_clicked()
{
    tcpClient->abort();
    if(localFile&&localFile->isOpen())
    {
        localFile->close();
    }
    close();
}


void tcpclient::on_closeBtn_clicked()
{
    on_cancelBtn_clicked();
}
