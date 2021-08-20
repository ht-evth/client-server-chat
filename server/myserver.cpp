#include "myserver.h"
#include "serverworker.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>


myserver::myserver(QObject *parent)
    : QTcpServer(parent)
{
    if (listen(QHostAddress::Any, 45000))
        qDebug() << "Listening 45000 port...";
    else
        qDebug() << "Server does'nt started";
}

myserver::~myserver()
{
    for (ServerWorker *worker : _clients)
        worker->disconnectFromClient();

    close();
}

void myserver::logMessage(const QString &msg)
{
    qInfo() << msg + QLatin1Char('\n');
}


void myserver::incomingConnection(qintptr socketDescriptor)
{
    ServerWorker *worker = new ServerWorker(this);

    if (!worker->setSocketDescriptor(socketDescriptor))
    {
        worker->deleteLater();
        return;
    }

    connect(worker, &ServerWorker::disconnectedFromClient, this, std::bind(&myserver::userDisconnected, this, worker));
    connect(worker, &ServerWorker::error, this, std::bind(&myserver::userError, this, worker));
    connect(worker, &ServerWorker::jsonReceived, this, std::bind(&myserver::jsonReceived, this, worker, std::placeholders::_1));
    connect(worker, &ServerWorker::logMessage, this, &myserver::logMessage);

    _clients.append(worker);
    emit logMessage(QStringLiteral("A new user is connected!"));
}

void myserver::sendJson(ServerWorker *destination, const QJsonObject &message)
{
    // отпарвка сообщения конкретному клиенту

    Q_ASSERT(destination);
    destination->sendJson(message);
}

void myserver::broadcast(const QJsonObject &message, ServerWorker *exclude)
{
    // отправляем сообщение всем клиентам, кроме exclude

    for (ServerWorker *worker : _clients)
    {
        Q_ASSERT(worker);
        if (worker == exclude)
            continue;
        sendJson(worker, message);
    }
}


void myserver::jsonReceived(ServerWorker *sender, const QJsonObject &doc)
{
    // печать в лог полученного json

    Q_ASSERT(sender);
    emit logMessage(QLatin1String("JSON received ") + QString::fromUtf8(QJsonDocument(doc).toJson()));

    // если никнейм пустой - ошибка авторизации
    if (sender->getNickname().isEmpty())
        return jsonFromLoggedOut(sender, doc);
    jsonFromLoggedIn(sender, doc);
}


void myserver::userDisconnected(ServerWorker *sender)
{
    // пользователь отключился - удаляем его из списка
    _clients.removeAll(sender);
    const QString nickname = sender->getNickname();

    if (!nickname.isEmpty())
    {
        // выводим сообщение о дисконнекте пользователя
        // и отправляем его всем клиентам

        QJsonObject discMsg;
        discMsg[QStringLiteral("type")] = QStringLiteral("userdisconnected");
        discMsg[QStringLiteral("nickname")] = nickname;
        broadcast(discMsg, nullptr);
        emit logMessage(nickname + QLatin1String(" disconnected"));
    }
    sender->deleteLater();
}


void myserver::userError(ServerWorker *sender)
{
    Q_UNUSED(sender)
    emit logMessage(QLatin1String("the error occurred because of ") + sender->getNickname());
}


void myserver::jsonFromLoggedOut(ServerWorker *sender, const QJsonObject &docObj)
{
    // отправляем пользователю сообщение об успешной авторизации + сообщение о новом пользователе для других пользователей
    // или соответвтующую ошибку авторизации

    Q_ASSERT(sender);
    const QJsonValue typeVal = docObj.value(QLatin1String("type"));

    if (typeVal.isNull() || !typeVal.isString())
        return;

    if (typeVal.toString().compare(QLatin1String("login"), Qt::CaseInsensitive) != 0)
        return;

    const QJsonValue nicknameVal = docObj.value(QLatin1String("nickname"));
    if (nicknameVal.isNull() || !nicknameVal.isString())
        return;

    const QString newNickname = nicknameVal.toString().simplified();
    if (newNickname.isEmpty())
        return;

    for (ServerWorker *worker : qAsConst(_clients))
    {
        if (worker == sender)
            continue;
        if (worker->getNickname().compare(newNickname, Qt::CaseInsensitive) == 0)
        {
            QJsonObject message;
            message[QStringLiteral("type")] = QStringLiteral("login");
            message[QStringLiteral("success")] = false;
            message[QStringLiteral("reason")] = QStringLiteral("duplicate nickname");
            sendJson(sender, message);
            return;
        }
    }

    sender->setNickname(newNickname);
    QJsonObject successMessage;
    successMessage[QStringLiteral("type")] = QStringLiteral("login");
    successMessage[QStringLiteral("success")] = true;
    sendJson(sender, successMessage);

    QJsonObject connectedMessage;
    connectedMessage[QStringLiteral("type")] = QStringLiteral("newuser");
    connectedMessage[QStringLiteral("nickname")] = newNickname;
    broadcast(connectedMessage, sender);
}

void myserver::jsonFromLoggedIn(ServerWorker *sender, const QJsonObject &docObj)
{
    // отправка полученного сообщения всем пользователям

    Q_ASSERT(sender);
    const QJsonValue typeVal = docObj.value(QLatin1String("type"));

    if (typeVal.isNull() || !typeVal.isString())
        return;

    if (typeVal.toString().compare(QLatin1String("message"), Qt::CaseInsensitive) != 0)
        return;

    const QJsonValue textVal = docObj.value(QLatin1String("text"));
    if (textVal.isNull() || !textVal.isString())
        return;

    const QString text = textVal.toString().trimmed();
    if (text.isEmpty())
        return;

    QJsonObject message;
    message[QStringLiteral("type")] = QStringLiteral("message");
    message[QStringLiteral("text")] = text;
    message[QStringLiteral("sender")] = sender->getNickname();
    broadcast(message, sender);
}

