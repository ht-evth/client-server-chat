#ifndef MYSERVER_H
#define MYSERVER_H

#include <QObject>
#include "QTcpServer"
class ServerWorker;

class myserver: public QTcpServer
{
    Q_OBJECT
    Q_DISABLE_COPY(myserver)

public:
    explicit myserver(QObject *parent = nullptr);
    ~myserver();

protected:
    void incomingConnection(qintptr socketDescriptor) override; // входящее соединение

signals:
    void newMessage(const QByteArray &message);

public:
    void logMessage(const QString &msg);

private slots:
    void broadcast(const QJsonObject &message, ServerWorker *exclude);
    void jsonReceived(ServerWorker *sender, const QJsonObject &doc);
    void userDisconnected(ServerWorker *sender);
    void userError(ServerWorker *sender);

private:
    void jsonFromLoggedOut(ServerWorker *sender, const QJsonObject &doc);
    void jsonFromLoggedIn(ServerWorker *sender, const QJsonObject &doc);
    void sendJson(ServerWorker *destination, const QJsonObject &message);
    QVector<ServerWorker *> _clients;
};

#endif // MYSERVER_H
