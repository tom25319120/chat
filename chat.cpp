#include "chat.h"
#include "qevent.h"
#include<QHostInfo>
#include<QMessageBox>
#include<QDateTime>
#include<QProcess>
#include<QDataStream>
#include<QScrollBar>
#include<QFont>
#include<QNetworkInterface>
#include<QStringList>
#include<QDebug>
#include<QFileDialog>
#include<QColorDialog>
#include<QHostAddress>
#include<QFontComboBox>
#include<QTextEdit>
#include <QTimer>
#include "tcpclient.h"
#include"ui_chat.h"
chat::chat(QWidget *parent,QString xpasvusername,QString xpasuserip) :
    QDialog(parent),
    xpasuserip(xpasuserip),
    xpasusername(xpasvusername),
    xchat(nullptr),
    ui(new Ui::chat),
    server(nullptr),
    color(Qt::white)

{
    ui->setupUi(this);
    ui->textEdit->setFocusPolicy(Qt::StrongFocus);
    ui->textBrowser->setFocusPolicy(Qt::NoFocus);
    ui->textEdit->setFocus();
    ui->textEdit->installEventFilter(this);
    ui->label->setText(tr("与%1私聊中 对方的IP：%2").arg(xpasusername)
                           .arg(xpasuserip));
    newParticipant(getUserName(),QHostInfo::localHostName(),getIp());
    xchat = new QUdpSocket(this);
    xport = 45456;
    xchat->bind( QHostAddress(getIp()),xport);
    connect(xchat,SIGNAL(readyRead()),
            this,SLOT(processPendinDatagrams()));
    server = new TcpServer(this);
    connect(server, SIGNAL(sendFileName(QString)),
            this,SLOT(getFileName(QString)));
}
chat::~chat(){
    xchat->close();
    delete xchat;
    this->server->close();
    delete this->server;
}

void chat::sendMessage(messageType type, QString serverAddress)
{
    QByteArray data;
    QDataStream out(&data,QIODevice::WriteOnly);
    QString localHostName = QHostInfo::localHostName();
    QString address = getIp();
    switch (type) {
    case LeftParticipant:
    {
        out <<type << getUserName() << localHostName<<address;
        break;
    }
    case Message:
    {
        if(ui->textEdit->toPlainText() =="")
        {
            QMessageBox::warning(0,tr("警告"),tr("发送内容不能为空"),QMessageBox::Ok);
            return ;
        }
        else
        {
            QString messagestr = getMessage();
            out <<type << getUserName() << localHostName<<address<< messagestr;
            ui->textBrowser->verticalScrollBar()->setValue(ui->textBrowser->verticalScrollBar()->maximum());
            QTextCursor cursor = ui->textEdit->textCursor();
            ui->textBrowser->setTextCursor(cursor);
            if (messagestr.length() > 512) {
                messagestr = messagestr.left(512);
                // 可选：提示用户消息被截断
            }
            ui->textBrowser->append(tr("[%1](%2):%3").arg(getUserName(),  QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"),messagestr));
        }
        break;
    }
    case FileName:
    {
        out <<type << getUserName() << localHostName<<address<< fileName;
        break;
    }
    case Refuse:
    {
        out <<type << getUserName() << localHostName<<address;
        break;
    }
    default:
        out <<type << getUserName() << localHostName<<address;
        break;
    }
    xchat->writeDatagram(data,data.length(),QHostAddress(xpasuserip),xport);

}
/*
    用于接受传来的信息，并将信息展示到messageBrower上；

*/
void chat::processPendinDatagrams()
{
    while (xchat->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(xchat->pendingDatagramSize());
        xchat->readDatagram(datagram.data(),datagram.size());

        QDataStream in(&datagram,QIODevice::ReadOnly);
        int messageTyep;
        in >> messageTyep;
        QString userName,localHostName,ipAddress,messagestr;

        QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        switch (messageTyep) {
        case Xchat:
            break;

        case Message:
        {
            ui->label->setText(tr("与%1私聊中 对方的IP：%2").arg(xpasusername)
                                                                    .arg(xpasuserip));
            in >> userName >> localHostName >>ipAddress >>messagestr;
            QTextCursor cursor = ui->textEdit->textCursor();
            ui->textBrowser->setTextCursor(cursor);
            ui->textBrowser->append(tr("[%1](%2):%3").arg(userName, time, messagestr));
            break;
        }
        case FileName:
        {
            in >> userName >> localHostName >> ipAddress ;
            QString fileName;

            in >> fileName;
            qDebug() << "fileName:" << fileName;
            hasPendinFile(userName,ipAddress,xpasuserip,fileName);
            break;

        }
        case Refuse:
        {
            in >>userName >> localHostName>>ipAddress;
            int button=QMessageBox::question(this,tr("询问"),tr("对方拒绝接受文件，是否继续发送"),QMessageBox::Yes|QMessageBox::No,QMessageBox::Yes);
            if(button==QMessageBox::Yes)
            {
                on_sendBtn_clicked();
            }
            else{
                return;
            }
            break;
        }
        case LeftParticipant:
        {
            in >> userName >> localHostName>>ipAddress;
            userLeft(userName,localHostName,time);
            break;
        }
        case NewParticipant:
            in >> userName >> localHostName>>ipAddress;
            newParticipant(userName,localHostName,ipAddress);
            break;
        default:
            break;
        }

    }



}

QString chat::getMessage()
{
    QString msg = ui->textEdit->toPlainText();
    ui->textEdit->clear();
    ui->textEdit->setFocus();
    return msg;
}




void chat::getFileName(QString name)
{
    fileName = name;
    sendMessage(FileName);
}


bool chat::eventFilter(QObject *target, QEvent *event)
{
    if(target == ui->textEdit)
    {
        if(event->type() == QEvent::KeyPress)
        {
            QKeyEvent *k = static_cast<QKeyEvent *> (event);
            if(k->key() == Qt::Key_Return)
            {

                on_sendbtn_clicked();
                return true;
            }
        }


    }
    return QWidget::eventFilter(target,event);

}


void chat :: userLeft(QString userName, QString localHostName, QString time)
{
    ui->textBrowser->setTextColor(Qt::gray);
    ui->textBrowser->setCurrentFont(QFont("Times New Roman",10));

    ui->label->setText(tr("用户%1离开会话界面!").arg(userName));
}

QString chat::getUserName()
{

    QStringList envVariables;
    envVariables << "USERNAME.*" << "USER.*" << "USERDOMAIN.*"
                 <<"HOSTNAME.*" << "DOMAINNAME.*";
    QStringList environment = QProcess::systemEnvironment();
    foreach (QString string, envVariables) {
        int index = environment.indexOf(QRegularExpression(string));
        if(index != -1)
        {
            QStringList stringList = environment.at(index).split('=');
            if(stringList.size() == 2)
            {
                return stringList.at(1);
                break;
            }
        }
    }
    return "unknow";

}

QString chat::getIp()
{
    QList<QHostAddress > list = QNetworkInterface::allAddresses();
    foreach (QHostAddress address, list) {
        if(address.protocol() == QAbstractSocket::IPv4Protocol)
            return address.toString();

    }
    return 0;
}

void chat::hasPendinFile(QString userName, QString serverAddress, QString clientAddress, QString fileName)
{
    int btn = QMessageBox::information(this,tr("接收文件"),
                                       tr("来自 %1 (%2)的文件:%3","是否接受")
                                           .arg(userName)
                                           .arg(serverAddress).arg(fileName),
                                       QMessageBox::Yes,QMessageBox::No);
    if(btn == QMessageBox::Yes)
    {
        QString name = QFileDialog::getSaveFileName(0,tr("保存文件"),fileName);
        if(!name.isEmpty())
        {
            tcpclient *client = new tcpclient(this);
            client->setPath(name);
            client->setHostAddress(QHostAddress(serverAddress));
            client->newConnect();
            client->show();
            qDebug() << "客户端已创建与服务端的链接" ;

        }
    }
    else{
        sendMessage(Refuse);
    }
}




void chat::on_boldBtn_clicked(bool checked)
{

    if(checked)
        ui->textEdit->setFontWeight(QFont::Bold);

    else
        ui->textEdit->setFontWeight(QFont::Normal);
    ui->textEdit->setFocus();
}

void chat::on_italicBtn_clicked(bool checked)
{
    ui->textEdit->setFontItalic(checked);
    ui->textEdit->setFocus();

}

void chat::on_underlineBtn_clicked(bool checked)
{
    ui->textEdit->setFontUnderline(checked);
    ui->textEdit->setFocus();

}

void chat::on_colorBtn_clicked()
{
    color = QColorDialog::getColor(color,this);
    if(color.isValid())
    {
        ui->textEdit->setTextColor(color);
        ui->textEdit->setFocus();
    }

}

void chat::on_sendBtn_clicked()
{
    if (server) {
        server->close();   // 关闭窗口（会触发析构，释放端口）
        delete server;
        server = nullptr;
    }
    server=new TcpServer();
    server->show();
    server->initserve();

}

void chat::on_saveBtn_clicked()
{

    if(ui->textBrowser->document()->isEmpty())
    {
        QMessageBox::warning(0,tr("警告"),
                             tr("聊天记录为空,无法保存!"),QMessageBox::Ok);
    }
    else
    {
        QString fileName  = QFileDialog::getSaveFileName(this,tr("保存聊天记录"),tr("聊天记录"),
                                                        tr("文本(*.txt);All File(* . *)"));
        if(!fileName.isEmpty())
            saveFile(fileName);
    }
}

void chat::saveFile(QString fileName)
{
    QFile file(fileName);
    if(!file.open(QFile::WriteOnly | QFile::Text))
    {
        QMessageBox::warning(this,tr("保存文件"),tr("无法保存文件 %1:\n %2")
                                                       .arg(fileName)
                                                       .arg(file.errorString()));
        return ;
    }
    QTextStream out(&file);
    out << ui->textBrowser->toPlainText();


}

void chat::closeEvent(QCloseEvent *ev)
{
    QString localHostName = QHostInfo::localHostName();
    QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    ui->textBrowser->clear();
    ui->textEdit->clear();
    close();
    emit chatclose();
}

void chat::newParticipant(QString username, QString Localname, QString address)
{
    ui->textBrowser->setTextColor(Qt::gray);
    ui->textBrowser->setCurrentFont(QFont("Times New Roman", 10));
    ui->textBrowser->append(tr("%1 在线!").arg(username));
}

void chat::on_fontComboBox_currentFontChanged(const QFont &f)
{
    ui->textEdit->setCurrentFont(f);
    ui->textEdit->setFocus();
}

void chat::on_clearnBtn_clicked()
{
    ui->textBrowser->clear();
}


void chat::on_sizecomboBox_currentTextChanged(const QString &arg1)
{
    ui->textEdit->setFontPointSize(ui->sizecomboBox->currentText().toInt());
    ui->textEdit->setFocus();
}




void chat::on_sendbtn_clicked()
{
    if (ui->textEdit->toPlainText().isEmpty()) {
        QMessageBox::warning(this, "警告", "不能发送空信息！");
        return;
    }
    sendMessage(Message);
    ui->textEdit->clear();
}


void chat::on_closeBtn_clicked()
{
    close();
}

