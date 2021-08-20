#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QTcpSocket>

class Client : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Client)

public:
    explicit Client(QObject *parent = nullptr);

public slots:
    void connectToServer(const QHostAddress &address, quint16 port);    // подключение к серверу
    void login(const QString &nickname);                                // логин - передает никнейм через сокет (json)
    void sendMessage(const QString &text);                              // отправка сообщения (json)
    void disconnectFromHost();                                          // дисконнект от сервера

private slots:
    void onReadyRead();

signals:
    void connected();
    void loggedIn();
    void loginError(const QString &reason);
    void disconnected();
    void messageReceived(const QString &sender, const QString &text);
    void error(QAbstractSocket::SocketError socketError);
    void userJoined(const QString &nickname);
    void userLeft(const QString &nickname);
private:
    QTcpSocket* _clientSocket;
    bool _loggedIn;
    void jsonReceived(const QJsonObject &doc);

};

#endif // CLIENT_H
