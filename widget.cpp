#include "widget.h"
#include "qscrollbar.h"
#include "qtimer.h"
#include "ui_widget.h"
#include "tcpclient.h"
#include "tcpserver.h"
#include <QColorDialog>
#include <QFileDialog>
#include <QKeyEvent>
#include <QMessageBox>
#include <QDateTime>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QProcess>
#include <QRegularExpression>
#include <QTextCursor>
#include <QDebug>
#include <QTimer>

widget::widget(QMainWindow *parent)
    : QMainWindow(parent)
    , ui(new Ui::widget)
    , server(nullptr)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/LAN"));
    setWindowTitle("局域网聊天室");
    setFixedSize(QSize(800,450));
    ui->userTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->usernum->setText(tr("在线人数:0"));
    udp = new QUdpSocket(this);
    port = 9999;
    udp->bind(port, QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint);

    ui->textEdit->setFont(ui->fontComboBox->currentFont());
    ui->textEdit->setFontPointSize(ui->sizecomboBox->currentText().toInt());
    QTimer::singleShot(100,this,[this](){
        sendMessage(NewParticipant);
        QTimer::singleShot(800, this, [this](){
            sendMessage(RequestionList);
        });
    });
    connect(udp, &QUdpSocket::readyRead, this, &widget::processPendingDatagram);

    ui->textEdit->installEventFilter(this);
    ui->messageBrowser->installEventFilter(this);
    ui->userTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(ui->userTable, &QTableWidget::cellDoubleClicked,
            this, &widget::onTableWidgetCellDoubleClicked);
    heartbeatTimer=new QTimer(this);
    connect(heartbeatTimer,&QTimer::timeout,this,&widget::sendHeartbeat);
    heartbeatTimer->start(30000);
    QTimer *cleanupTimer = new QTimer(this);
    connect(cleanupTimer, &QTimer::timeout, this, &widget::cleanupStaleUsers);
    cleanupTimer->start(60000);
}

widget::~widget()
{
    // 关闭所有私聊窗口
    for (auto chatWindow : privateChats) {
        if (chatWindow) {
            chatWindow->close();
            delete chatWindow;
        }
    }
    privateChats.clear();
    delete ui;
}

void widget::newParticipant(QString username, QString Localname, QString address)
{
    bool isEmpty = ui->userTable->findItems(address, Qt::MatchExactly).isEmpty();
    if (address == getIP()) return;
    if (isEmpty) {
        QTableWidgetItem *user = new QTableWidgetItem(username);
        QTableWidgetItem *host = new QTableWidgetItem(Localname);
        QTableWidgetItem *ip = new QTableWidgetItem(address);
        if(!user||!host||!ip)
        {
            qDebug()<<"创建QTableWidgetItem失败！";
            return;
        }
        ui->userTable->insertRow(0);
        ui->userTable->setItem(0, 0, user);
        ui->userTable->setItem(0, 1, host);
        ui->userTable->setItem(0, 2, ip);

        ui->messageBrowser->setTextColor(Qt::gray);
        ui->messageBrowser->setCurrentFont(QFont("Times New Roman", 10));
        ui->messageBrowser->append(tr("%1 在线!").arg(username));
        ui->usernum->setText(tr("在线人数:%1").arg(ui->userTable->rowCount()));
    }
}

void widget::Participantleft(QString ip, QString usename, QString time)
{
    // 找到该用户对应的 IP，用于关闭私聊窗口
    QList<QTableWidgetItem*> items = ui->userTable->findItems(ip, Qt::MatchExactly);
    if (!items.isEmpty()) {
        int row = items.first()->row();

        // 关闭该用户的私聊窗口（如果存在）
        if (privateChats.contains(ip)) {
            privateChats[ip]->close();   // close() 会触发 chatclose 信号，自动从 map 移除
        }

        ui->userTable->removeRow(row);
    }
    ui->messageBrowser->setTextColor(Qt::gray);
    ui->messageBrowser->setCurrentFont(QFont("Times New Roman", 10));
    ui->messageBrowser->append(tr("%1于%2 下线!").arg(usename, time));
    ui->usernum->setText(tr("在线人数:%1").arg(ui->userTable->rowCount()));
}

void widget::sendMessage(messageType type, QString serverAddress)
{
    QByteArray data;
    QDataStream out(&data, QIODeviceBase::WriteOnly);
    QString hostname = QHostInfo::localHostName();
    QString address = getIP();
    QString username = getUsername();
    switch (type) {
    case NewParticipant:
    {
        out << type << username << hostname<<address;
        break;
    }
    case Message:
    {
        out << type << username << hostname<<address;
        if (ui->textEdit->toPlainText() == "") {
            QMessageBox::warning(this, "警告", "不能发送空信息！");
            return;
        } else {
            out << getmessage();

            ui->messageBrowser->verticalScrollBar()->setValue(ui->messageBrowser->verticalScrollBar()->maximum());
        }
        break;
    }
    case LeftParticipant:
        out << type << username << hostname<<address;
        break;
    case FileName:
    {
        QString clientaddress;
        int row = ui->userTable->selectedItems().first()->row();
        clientaddress = ui->userTable->item(row, 2)->text();
        out << type << username << hostname<<address<< clientaddress << filename;
        break;
    }
    case Refuse:
    {
        out << type << username << hostname<<address;
        break;
    }
    case Xchat:
    {
        out << type << username << hostname<<address<<serverAddress;
        break;
    }
    case RequestionList:
    {
        out << type << username << hostname<<address;
        break;
    }
    default:
        break;
    }
    if(type==UserInfo&&!serverAddress.isEmpty())
    {
        udp->writeDatagram(data,data.length(),QHostAddress(serverAddress),port);
    }
    else{
        udp->writeDatagram(data, data.length(), QHostAddress::Broadcast, port);
    }
}

void widget::hasPendingfile(QString clientaddress, QString fileName, QString localHostName, QHostAddress address)
{
    QString localaddress = getIP();
    if (clientaddress == localaddress) {
        int button = QMessageBox::information(this, "信息",
                                              tr("你有一份来自%1的文件是否接收").arg(localHostName),
                                              QMessageBox::Yes | QMessageBox::No,
                                              QMessageBox::Yes);
        if (button == QMessageBox::Yes) {
            QString path = QFileDialog::getSaveFileName(this, "保存文件", tr("/home/%").arg(fileName),
                                                        "Text files (*.txt);;Images (*.png *.xpm *.jpg);;XML files (*.xml)");
            if (path.isEmpty())
                return;

            tcpclient *client = new tcpclient;
            client->setPath(path);
            client->setHostAddress(address);
            client->newConnect();
            client->show();
        } else {
            sendMessage(Refuse, address.toString());
            return;
        }
    }
    return;
}

QString widget::getIP()
{
    QList<QHostAddress> allip = QNetworkInterface::allAddresses();
    if (!allip.empty()) {
        foreach (QHostAddress tempip, allip) {
            if (tempip.protocol() == QAbstractSocket::IPv4Protocol) {
                return tempip.toString();
            }
        }
    }
    QHostAddress null = QHostAddress::AnyIPv4;
    return null.toString();
}

QString widget::getUsername()
{
    QStringList envVariables;
    envVariables << "USERNAME.*" << "USER.*" << "USERDOMAIN.*"
                 << "HOSTNAME.*" << "DOMAINNAME.*";
    QStringList environment = QProcess::systemEnvironment();
    foreach (QString tempenv, envVariables) {
        QRegularExpression re(tempenv);
        int index = environment.indexOf(re);
        if (index != -1) {
            QStringList list = environment[index].split("=", Qt::SkipEmptyParts);
            if (!list.isEmpty()) {
                return list[1];
            }
        }
    }
    return "nousername";
}

QString widget::getmessage()
{
    return ui->textEdit->toPlainText();

}

void widget::closeEvent(QCloseEvent *event)
{
    int flag = QMessageBox::information(this, "退出", tr("确定要退出吗？"),
                                        QMessageBox::Yes | QMessageBox::No,
                                        QMessageBox::Yes);
    if (flag == QMessageBox::Yes) {
        Participantleft(getIP(), getUsername(),
                        QDateTime::currentDateTime().toString("yyyy-MM-dd hh-mm-ss"));
        qDebug()<<(tr("%1于%2 下线!").arg(getUsername(), QDateTime::currentDateTime().toString("yyyy-MM-dd hh-mm-ss")));
        sendMessage(LeftParticipant);
        close();
    }
    else{
        return;
    }
}

bool widget::eventFilter(QObject *watch, QEvent *event)
{
    if (watch == ui->textEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyev = static_cast<QKeyEvent*>(event);
        if ((keyev->key() == Qt::Key_Return) && !(keyev->modifiers() & Qt::ControlModifier)) {
            on_sendbtn_clicked();
            return true;
        }
    }
    return false;
}

void widget::onTableWidgetCellDoubleClicked(int row, int column)
{
    if (row < 0 || row >= ui->userTable->rowCount()) {
        return;
    }
    QTableWidgetItem *nameItem  = ui->userTable->item(row, 0);
    QTableWidgetItem *hostItem = ui->userTable->item(row, 1);
    QTableWidgetItem *ipItem   = ui->userTable->item(row, 2);
    if (!nameItem || !hostItem || !ipItem) {
        QMessageBox::warning(this, "警告", "无法获取用户信息");
        return;
    }
    QString receiveName = nameItem->text();
    QString receiveHost = hostItem->text();
    QString receiveIP = ipItem->text();
    if (receiveIP.isEmpty()) {
        QMessageBox::warning(this, "警告", "无法获取对方IP地址");
        return;
    }
    if (receiveIP == getIP()) {
        QMessageBox::warning(this, "警告", "不能和自己聊天");
        return;
    }
    // 检查是否已经存在与该用户的私聊窗口
    if (privateChats.contains(receiveIP)) {
        chat *existingChat = privateChats.value(receiveIP);
        existingChat->show();
        existingChat->raise();
        existingChat->activateWindow();
        QMessageBox::information(this, "提示", tr("已存在与 %1 的私聊窗口").arg(receiveName));
        return;
    }
    // 创建新的私聊窗口
    chat *newChat = new chat(nullptr, receiveName, receiveIP);
    newChat->setWindowTitle(tr("与 %1 (%2) 的私聊").arg(receiveName, receiveIP));
    privateChats.insert(receiveIP, newChat);
    // 连接关闭信号，以便从 map 中移除
    connect(newChat, &chat::chatclose, this, [this, receiveIP]() {
        onPrivateChatClosed(receiveIP);
    });

    newChat->show();
    // 发送私聊请求
    sendMessage(Xchat,receiveIP);

    ui->messageBrowser->append(tr("已向 %1 发起私聊请求...").arg(receiveName));
    return;
}

void widget::onPrivateChatClosed(const QString &ip)
{
    if (privateChats.contains(ip)) {
        privateChats.remove(ip);
        qDebug() << "私聊窗口已关闭，当前私聊窗口数量:" << privateChats.size();
    }
}

void widget::processPendingDatagram()
{
    while (udp->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(udp->pendingDatagramSize());
        udp->readDatagram(datagram.data(), udp->pendingDatagramSize());

        QDataStream in(&datagram, QIODeviceBase::ReadOnly);
        int type;
        QString messageing, username, ip, localHostName;
        in >> type;
        QString currenttime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

        switch (type) {
        case NewParticipant:
        {
            in >> username >> localHostName >> ip;
            this->lastSeen[ip]=QDateTime::currentDateTime();
            newParticipant(username, localHostName, ip);
            break;
        }
        case LeftParticipant:
        {
            in >> username >> localHostName>>ip;
            Participantleft(ip, username, currenttime);
            break;
        }
        case Message:
        {
            in >> username >> localHostName >> ip >> messageing;
            QTextCursor cursor = ui->textEdit->textCursor();
            ui->messageBrowser->setTextCursor(cursor);
            ui->messageBrowser->append(tr("[%1](%2):%3").arg(username, currenttime, messageing));
            ui->messageBrowser->verticalScrollBar()->setValue(ui->messageBrowser->verticalScrollBar()->maximum());
            break;
        }
        case FileName:
        {
            QString clientaddress, fileName;
            QHostAddress address;
            in >> username >> localHostName >> ip >> clientaddress >> fileName;
            address.setAddress(ip);
            hasPendingfile(clientaddress, fileName, localHostName, address);
            break;
        }
        case Refuse:
        {
            in >> username >> localHostName >> ip;
            int button = QMessageBox::information(this, "提示",
                                                  tr("对方拒绝接收文件，是否重发文件？"),
                                                  QMessageBox::Yes | QMessageBox::No);
            if (button == QMessageBox::Yes) {
                on_sendBtn_clicked();
            } else {
                if (server) server->close();
            }
            break;
        }
        case Xchat:
        {
            QString address;
            in >> username >> localHostName >> ip>>address;
            if(address==getIP())
            {
                if (privateChats.contains(ip)) {
                    QMessageBox::information(this, "提示",
                                             tr("已存在与 %1 的私聊窗口").arg(username));
                    privateChats[ip]->show();
                    privateChats[ip]->raise();
                    privateChats[ip]->activateWindow();
                    break;
                }
                chat *newChat = new chat(nullptr, username, ip);
                newChat->setWindowTitle(tr("与 %1 (%2) 的私聊").arg(username, ip));
                privateChats.insert(ip, newChat);
                connect(newChat, &chat::chatclose, this, [this, ip]() {
                    onPrivateChatClosed(ip);
                });
                newChat->show();
            }
            break;
        }
        case RequestionList:  // 收到用户列表请求
        {
            in >> username >> localHostName >> ip;
            QString myIP = getIP();

            // 如果请求者不是自己，则回复当前所有在线用户
            if (ip != myIP) {
                for (int i = 0; i < ui->userTable->rowCount(); i++) {
                    QTableWidgetItem *userItem = ui->userTable->item(i, 0);
                    QTableWidgetItem *hostItem = ui->userTable->item(i, 1);
                    QTableWidgetItem *ipItem = ui->userTable->item(i, 2);

                    if (userItem && hostItem && ipItem) {
                        QString user = userItem->text();
                        QString host = hostItem->text();
                        QString userIP = ipItem->text();

                        if (userIP != myIP) {
                            QByteArray responseData;
                            QDataStream responseOut(&responseData, QIODeviceBase::WriteOnly);
                            responseOut << (int)UserInfo << user << host << userIP << ip;
                            udp->writeDatagram(responseData, responseData.length(), QHostAddress(ip), port);
                            qDebug() << "发送用户信息:" << user << userIP << "给" << ip;
                        }
                    }
                }
            }
            break;
        }
        case UserInfo:
        {
            QString user,host,userIP ;
            QString myIP = getIP();
            in>>user>>host>>userIP>>ip;
            if(userIP != myIP)
            {
                bool exists = false;
                for (int i = 0; i < ui->userTable->rowCount(); i++) {
                    QTableWidgetItem *ipItem = ui->userTable->item(i, 2);
                    if (ipItem && ipItem->text() == userIP) {
                        exists = true;
                        break;
                    }
                }

                if (!exists) {
                    newParticipant(user, host, userIP);
                    qDebug() << "添加用户:" << user << "IP:" << userIP;
                } else {
                    qDebug() << "用户已存在，跳过添加:" << user;
                }
            }
            break;
        }
        default:
            break;
        }
    }
}

void widget::sendHeartbeat()
{
    sendMessage(NewParticipant);
}

void widget::cleanupStaleUsers()
{
    QDateTime now = QDateTime::currentDateTime();
    QList<QString> toRemove;
    for (auto it = lastSeen.begin(); it != lastSeen.end(); ++it) {
        if (it.value().secsTo(now) > 90) {  // 超过90秒未收到心跳
            toRemove.append(it.key());
        }
    }
    for (const QString &ip : toRemove) {
        // 从 userTable 中移除该 IP 对应的行
        // 并关闭对应的私聊窗口
        Participantleft(ip, tr("未知用户"), now.toString());
        lastSeen.remove(ip);
    }
}

void widget::getFilename(QString filename)
{
    this->filename = filename;
    sendMessage(FileName);
}

void widget::on_boldBtn_clicked(bool checked)
{
    ui->textEdit->setFontWeight(checked ? QFont::Bold : QFont::Normal);
    ui->textEdit->setFocus();
}


void widget::on_italicBtn_clicked(bool checked)
{
    ui->textEdit->setFontItalic(checked);
}

void widget::on_underlineBtn_clicked(bool checked)
{
    ui->textEdit->setFontUnderline(checked);
}

void widget::on_colorBtn_clicked()
{
    QColor color = QColorDialog::getColor(Qt::white, this, tr("选择颜色"),
                                          QColorDialog::NoEyeDropperButton);
    ui->textEdit->setTextColor(color);
    this->color = color;
}

void widget::on_sendbtn_clicked()
{
    if (ui->textEdit->toPlainText().isEmpty()) {
        QMessageBox::warning(this, "警告", "不能发送空信息！");
        return;
    }
    sendMessage(Message);
    ui->textEdit->clear();
}

void widget::on_exitpbtn_clicked()
{
    close();
}

void widget::on_sendBtn_clicked()
{
    if (server) {
        server->close();
        delete server;
        server = nullptr;
    }
    if (ui->userTable->selectedItems().empty()) {
        QMessageBox::warning(this, tr("错误"), tr("没有选择发送人"));
        return;
    }
    server = new TcpServer();
    server->initserve();
    server->show();
    connect(server, &TcpServer::sendFileName, this, &widget::getFilename);
}

void widget::on_clearnBtn_clicked()
{
    ui->messageBrowser->clear();
}

void widget::on_saveBtn_clicked()
{
    QString path = QFileDialog::getSaveFileName(this, "保存至", "/home", "文本文件 (*.txt);;Word文档 (*.docx)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "无法打开文件:" << file.errorString();
        return;
    }

    QTextStream out(&file);
    out << ui->messageBrowser->toPlainText();
    file.close();
    qDebug() << "文件已保存到:" << path;
}

void widget::on_fontComboBox_currentTextChanged(const QString &arg1)
{
    ui->textEdit->setCurrentFont(arg1);
    ui->textEdit->setFocus();
}


void widget::on_sizecomboBox_currentTextChanged(const QString &arg1)
{
    ui->textEdit->setFontPointSize(ui->sizecomboBox->currentText().toInt());
    ui->textEdit->setFocus();
}


void widget::on_flash_clicked()
{
    sendMessage(RequestionList);
}

