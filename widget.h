#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QUdpSocket>
#include <QTcpServer>
#include <QMap>
#include "chat.h"
#include "qmainwindow.h"

QT_BEGIN_NAMESPACE
namespace Ui { class widget; }
QT_END_NAMESPACE

class TcpServer;

class widget : public QMainWindow
{
    Q_OBJECT

public:
    widget( QMainWindow *parent = nullptr);
    ~widget();

    void newParticipant(QString username, QString Localname, QString address);
    void Participantleft(QString username, QString Localname, QString time);
    void sendMessage(messageType type, QString serverAddress = "");
    void hasPendingfile(QString clientaddress, QString fileName, QString localHostName, QHostAddress address);
    QString getIP();
    QString getUsername();
    QString getmessage();
    void getFilename(QString filename);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watch, QEvent *event) override;

private slots:
    void onTableWidgetCellDoubleClicked(int row, int column);
    void onPrivateChatClosed(const QString &ip);   // 窗口关闭时从map移除

    void on_boldBtn_clicked(bool checked);
    void on_fontComboBox_currentFontChanged(const QFont &f);
    void on_sizecomboBox_currentIndexChanged(int index);
    void on_italicBtn_clicked(bool checked);
    void on_underlineBtn_clicked(bool checked);
    void on_colorBtn_clicked();
    void on_sendbtn_clicked();
    void on_exitpbtn_clicked();
    void on_sendBtn_clicked();
    void on_clearBtn_clicked();
    void on_saveBtn_clicked();

private:
    Ui::widget *ui;
    QUdpSocket *udp;
    int port;
    QString filename;
    QColor color;
    TcpServer *server;
    // 私聊窗口管理：key = 对方IP
    QMap<QString, chat*> privateChats;

    void processPendingDatagram();
};

#endif // WIDGET_H
