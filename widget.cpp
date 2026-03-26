#include "widget.h"
#include "qscrollbar.h"
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

    udp = new QUdpSocket(this);
    port = 9999;
    udp->bind(port, QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint);

    ui->textEdit->setFont(ui->fontComboBox->currentFont());
    ui->textEdit->setFontPointSize(ui->sizecomboBox->currentText().toInt());

    connect(udp, &QUdpSocket::readyRead, this, &widget::processPendingDatagram);
    sendMessage(NewParticipant);

    connect(ui->saveBtn, &QToolButton::clicked, this, &widget::on_saveBtn_clicked);
    ui->textEdit->installEventFilter(this);
    ui->messageBrowser->installEventFilter(this);
    ui->userTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(ui->userTable, &QTableWidget::cellDoubleClicked,
            this, &widget::onTableWidgetCellDoubleClicked);
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
    bool isEmpty = ui->userTable->findItems(Localname, Qt::MatchExactly).isEmpty();

    if (isEmpty) {
        QTableWidgetItem *user = new QTableWidgetItem(username);
        QTableWidgetItem *host = new QTableWidgetItem(Localname);
        QTableWidgetItem *ip = new QTableWidgetItem(address);
        ui->userTable->insertRow(0);
        ui->userTable->setItem(0, 0, user);
        ui->userTable->setItem(0, 1, host);
        ui->userTable->setItem(0, 2, ip);

        ui->messageBrowser->setTextColor(Qt::gray);
        ui->messageBrowser->setCurrentFont(QFont("Times New Roman", 10));
        ui->messageBrowser->append(tr("%1 在线!").arg(username));
        ui->usernum->setText(tr("在线人数:%1").arg(ui->userTable->rowCount()));
        sendMessage(NewParticipant);
    }
}

void widget::Participantleft(QString username, QString Localname, QString time)
{
    Q_UNUSED(Localname);   // 本函数未使用主机名，消除警告

    // 找到该用户对应的 IP，用于关闭私聊窗口
    QList<QTableWidgetItem*> items = ui->userTable->findItems(username, Qt::MatchExactly);
    if (!items.isEmpty()) {
        int row = items.first()->row();
        QString userIP = ui->userTable->item(row, 2)->text();

        // 关闭该用户的私聊窗口（如果存在）
        if (privateChats.contains(userIP)) {
            privateChats[userIP]->close();   // close() 会触发 chatclose 信号，自动从 map 移除
        }

        ui->userTable->removeRow(row);
    }

    ui->messageBrowser->setTextColor(Qt::gray);
    ui->messageBrowser->setCurrentFont(QFont("Times New Roman", 10));
    ui->messageBrowser->append(tr("%1于%2 下线!").arg(username, time));
    ui->usernum->setText(tr("在线人数:%1").arg(ui->userTable->rowCount()));
}

void widget::sendMessage(messageType type, QString serverAddress)
{
    QByteArray data;
    QDataStream out(&data, QIODeviceBase::WriteOnly);
    QString hostname = QHostInfo::localHostName();
    QString address = getIP();
    QString username = getUsername();
    out << type << username << hostname;

    switch (type) {
    case NewParticipant:
        out << address;
        break;
    case Message:
    {
        if (ui->textEdit->toPlainText() == "") {
            QMessageBox::warning(this, "警告", "不能发送空信息！");
            return;
        } else {
            out << address << getmessage();
            ui->messageBrowser->verticalScrollBar()->setValue(ui->messageBrowser->verticalScrollBar()->maximum());
        }
        break;
    }
    case LeftParticipant:
        break;
    case FileName:
    {
        QString clientaddress;
        int row = ui->userTable->selectedItems().first()->row();
        clientaddress = ui->userTable->item(row, 2)->text();
        out << address << clientaddress << filename;
        break;
    }
    case Refuse:
        out << address << serverAddress;
        break;
    case Xchat:
        out << address;
        break;
    default:
        break;
    }
    udp->writeDatagram(data, data.length(), QHostAddress::Broadcast, port);
}

void widget::hasPendingfile(QString clientaddress, QString fileName, QString localHostName, QHostAddress address)
{
    qDebug() << "hasPendingfile";
    QString localaddress = getIP();
    if (clientaddress == localaddress) {
        int button = QMessageBox::information(this, "信息",
                                              tr("你有一份来自%1的文件是否接收").arg(localHostName),
                                              QMessageBox::Yes | QMessageBox::No,
                                              QMessageBox::Yes);
        if (button == QMessageBox::Yes) {
            QString path = QFileDialog::getSaveFileName(this, "保存文件", "/home",
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
    QString host = QHostInfo::localHostName();
    Participantleft(getUsername(), host,
                    QDateTime::currentDateTime().toString("yyyy-MM-dd hh-mm-ss"));
    // 私聊窗口在析构函数中统一关闭，此处无需额外处理
    QWidget::closeEvent(event);
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
    Q_UNUSED(column);

    QString receiveName = ui->userTable->item(row, 0)->text();
    QString receiveHost = ui->userTable->item(row, 1)->text();
    QString receiveIP   = ui->userTable->item(row, 2)->text();

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
    sendMessage(Xchat);

    ui->messageBrowser->append(tr("已向 %1 发起私聊请求...").arg(receiveName));
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
            newParticipant(username, localHostName, ip);
            break;
        }
        case LeftParticipant:
        {
            in >> username >> localHostName;
            Participantleft(username, localHostName, currenttime);
            break;
        }
        case Message:
        {
            in >> username >> localHostName >> ip >> messageing;
            QTextCursor cursor = ui->textEdit->textCursor();
            ui->messageBrowser->setTextCursor(cursor);
            ui->messageBrowser->append(tr("[%1](%2):%3").arg(username, currenttime, messageing));
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
            QString address = getIP();
            in >> username >> localHostName >> ip;
            if (address == ip) {
                int button = QMessageBox::information(this, "提示",
                                                      tr("对方拒绝接收文件，是否重发文件？"),
                                                      QMessageBox::Yes | QMessageBox::No);
                if (button == QMessageBox::Yes) {
                    on_sendBtn_clicked();
                } else {
                    if (server) server->close();
                }
            }
            break;
        }
        case Xchat:
        {
            in >> username >> localHostName >> ip;

            int button = QMessageBox::question(this, "私聊询问",
                                               tr("%1 (%2) 发起私聊请求，是否同意？").arg(username, ip),
                                               QMessageBox::Yes | QMessageBox::No);
            if (button == QMessageBox::Yes) {
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
                ui->messageBrowser->append(tr("已同意与 %1 的私聊请求").arg(username));
            } else {
                sendMessage(Refuse, ip);
            }
            break;
        }
        default:
            break;
        }
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

void widget::on_fontComboBox_currentFontChanged(const QFont &f)
{
    ui->textEdit->setCurrentFont(f);
    ui->textEdit->setFocus();
}

void widget::on_sizecomboBox_currentIndexChanged(int index)
{
    Q_UNUSED(index);
    ui->textEdit->setFontPointSize(ui->sizecomboBox->currentText().toInt());
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
    int flag = QMessageBox::information(this, "退出", tr("确定要退出吗？"),
                                        QMessageBox::Yes | QMessageBox::No,
                                        QMessageBox::Yes);
    if (flag == QMessageBox::Yes) {
        close();
    }
}

void widget::on_sendBtn_clicked()
{
    if (ui->userTable->selectedItems().empty()) {
        QMessageBox::warning(this, tr("错误"), tr("没有选择发送人"));
        return;
    }
    server = new TcpServer();
    server->initserve();
    server->show();
    connect(server, &TcpServer::sendFileName, this, &widget::getFilename);
}

void widget::on_clearBtn_clicked()
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
