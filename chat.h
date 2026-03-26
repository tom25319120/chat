#ifndef CHAT_H
#define CHAT_H

#include <QDialog>

#include<QUdpSocket>
#include<QTcpServer>
#include<QDataStream>
#include<QTime>
#include"tcpserver.h"
#include<QString>
namespace Ui {
class chat;
}


enum messageType{
    Message,
    NewParticipant,
    LeftParticipant,
    Refuse,
    FileName,
    Xchat
};


class chat : public QDialog
{
    Q_OBJECT

public:
    // explicit Chat(QWidget *parent = 0);
    ~chat();
    chat(QWidget *parent,QString pasvusername,QString pasvuserip);

    QString xpasuserip;
    QString xpasusername;
    QUdpSocket *xchat;
    qint32 xport;

    void sendMessage(messageType type,QString serverAddress = "");
    void sendingmessage();
signals:
    void chatclose();
protected:
    void hasPendinFile(QString userName,QString serverAddress,
                       QString clientAddress,QString filename);
    void userLeft(QString userName,QString localHostName,QString time);
    void saveFile(QString fileName);

    bool eventFilter(QObject *target, QEvent *event);

    void closeEvent(QCloseEvent *);


private slots:

    void processPendinDatagrams();
    void getFileName(QString);

    void on_boldBtn_clicked(bool checked);

    void on_italicBtn_clicked(bool checked);

    void on_underlineBtn_clicked(bool checked);

    void on_colorBtn_clicked();

    void on_saveBtn_clicked();

    void on_clearBtn_clicked();

    void on_closeBtn_clicked();

    void on_sendBtn_clicked();

    void on_fontComboBox_currentFontChanged(const QFont &f);

    void on_comboBox_currentIndexChanged(const QString &arg1);

private:
    Ui::chat *ui;
    TcpServer *server;
    QColor color;
    bool savaFile(const QString fileName);

    QString getMessage();
    QString getIp();
    QString getUserName();

    QString message;
    QString fileName;

};

#endif // CHAT_H
