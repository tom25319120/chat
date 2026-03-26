#include "tcpserver.h"
#include "qtcpsocket.h"
#include "qthread.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include<ui_tcpserver.h>
TcpServer::TcpServer(QDialog *parent)
    : QDialog(parent)
    , ui(new Ui::tcpserver)
    , tcpServer(nullptr)
    , localFile(nullptr)
    , filesize(0)
    , payloadSize(64*1024)
    , TotalBytes(0)
    , bytesWritten(0)
    , bytestoWrite(0)
    , nexttcp(nullptr)
{
    ui->setupUi(this);
    setWindowTitle("传送文件");
    tcpServer=new QTcpServer;
    connect(tcpServer,&QTcpServer::newConnection,this,&TcpServer::sendMessage);
    connect(this,&TcpServer::processchange,this,&TcpServer::updataprocess);
}

TcpServer::~TcpServer()
{
    delete ui;
    if (localFile) {
        localFile->close();
        delete localFile;
        localFile = nullptr;
    }
    if (tcpServer) {
        tcpServer->close();
        delete tcpServer;
        tcpServer = nullptr;
    }
}

void TcpServer::initserve()
{
    payloadSize=64*1024;
    TotalBytes=0;
    bytesWritten=0;
    bytestoWrite=0;

    ui->label->setText(tr("请选择要传递的文件！"));
    ui->progressBar->reset();
    ui->openBtn->setEnabled(true);
    ui->closeBtn->setEnabled(true);
    ui->sendBtn->setEnabled(false);
    //tcpServer->close();
}

void TcpServer::sendMessage()
{
    nexttcp=tcpServer->nextPendingConnection();
    if (!localFile || !localFile->isOpen()) {
        QMessageBox::warning(this, "错误", "文件未打开！");
        return;
    }
    QFileInfo fileInfo(fileName);
    filesize=fileInfo.size();
    bytestoWrite=filesize;
    qint64 fileNamesize=theFileName.toUtf8().size();
    QByteArray bytes;
    QDataStream out(&bytes,QIODevice::ReadWrite);
    out.setVersion(QDataStream::Qt_4_0);
    out << qint64(0) << qint64(0) << theFileName;
    TotalBytes = fileNamesize+filesize;
    out.device()->seek(0);
    out << TotalBytes << fileNamesize;
    nexttcp->write(bytes);
    connect(nexttcp,&QTcpSocket::bytesWritten,this,&TcpServer::sendtext);
    sendtext();
    time.start();
}

void TcpServer::on_openBtn_clicked()
{
    fileName=QFileDialog::getOpenFileName(this,tr("打开文件"),tr("/home"),tr("file(*.txt *.docx)"));
    if(fileName.isEmpty())
    {
        return;
    }
    QFileInfo fileInfo(fileName);
    theFileName=fileInfo.fileName();
    ui->openBtn->setEnabled(false);
    ui->sendBtn->setEnabled(true);
    ui->label->setText(tr("要发送的文件为%1").arg(theFileName));
    // ✅ 清理旧文件对象
    if (localFile) {
        if (localFile->isOpen()) {
            localFile->close();
        }
        delete localFile;
        localFile = nullptr;
    }

    // ✅ 创建新的文件对象并打开
    localFile = new QFile(fileName);
    if(!localFile->open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, "错误",
                             tr("无法打开文件：%1").arg(localFile->errorString()));
        delete localFile;
        localFile = nullptr;
        return;
    }

    // ✅ 获取文件大小
    filesize = fileInfo.size();

    ui->openBtn->setEnabled(false);
    ui->sendBtn->setEnabled(true);
    ui->label->setText(tr("要发送的文件为：%1 (%2 KB)")
                           .arg(theFileName)
                           .arg(filesize / 1024.0, 0, 'f', 2));

    qDebug() << "文件已打开:" << fileName << "大小:" << filesize << "字节";

}


void TcpServer::on_sendBtn_clicked()
{
    if (!localFile || !localFile->isOpen()) {
        QMessageBox::warning(this, "错误", "请先选择文件！");
        return;
    }

    emit sendFileName(theFileName);

    // 如果已经在监听，先关闭
    if (tcpServer->isListening()) {
        tcpServer->close();
    }

    // 开始监听
    if (!tcpServer->listen(QHostAddress::Any, 6666)) {
        QMessageBox::warning(this, "错误",
                             tr("监听失败：%1").arg(tcpServer->errorString()));
        return;
    }

    ui->sendBtn->setEnabled(false);
    ui->label->setText(tr("等待对方接收..."));
    QThread::msleep(1000);
}


void TcpServer::on_closeBtn_clicked()
{
    close();
}

void TcpServer::sendtext()
{

    if (!localFile || !nexttcp) {
        qDebug() << "sendtext: localFile或nexttcp为空";
        return;
    }
    if (!localFile->isOpen()) {
        qDebug() << "文件未打开，尝试重新打开:" << fileName;
        if (!localFile->open(QIODevice::ReadOnly)) {
            qDebug() << "重新打开文件失败:" << localFile->errorString();
            return;
        }
        // 重置文件指针到正确位置
        localFile->seek(bytesWritten);
        qDebug() << "文件重新打开成功，位置:" << bytesWritten;
    }
    QByteArray block = localFile->read(payloadSize);
    if (!block.isEmpty()) {
        qint64 written = nexttcp->write(block);
        bytesWritten += written;
        bytestoWrite -= written;
        emit processchange();
    }

    // 如果文件发送完毕
    if (bytesWritten >= filesize) {
        localFile->close();
        nexttcp->disconnectFromHost();
        if (tcpServer && tcpServer->isListening()) {
            tcpServer->close();
        }

        // 重置界面状态
        ui->sendBtn->setEnabled(false);
        ui->openBtn->setEnabled(true);
        ui->label->setText(tr("文件发送完成！"));

        // 可选：显示完成消息
        int button=QMessageBox::information(this, "完成", "文件发送成功！");
        if(button==QMessageBox::Ok)close();
    }
}

void TcpServer::updataprocess()
{
    int usetime=time.elapsed();
    if(usetime>0)
    {
        int speed=bytesWritten/usetime;
        int lasttime=bytestoWrite/speed;
        int value=bytesWritten*100.0/filesize;
        ui->progressBar->setMaximum(100);
        ui->progressBar->setValue(value);
        ui->label->setText( tr("已接收 %1MB (%2MB/s)\n"
                              "共%3MB 已用时:%4秒\n"
                              "估计剩余时间:%5秒")
                               .arg(bytesWritten/1024.0/1024.0,0,'f',2)
                               .arg(speed/1000.0/1024.0/1024.0,0,'f',2)
                               .arg(filesize/1024.0/1024.0,0,'f',2)
                               .arg(usetime/1000.0,0,'f',2)
                               .arg(lasttime/1000.0,0,'f',0)
                           );
    }
}
