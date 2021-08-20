#include "client.h"
#include <QTcpSocket>
#include <QDataStream>
#include <QJsonObject>
#include <QJsonDocument>

Client::Client(QObject *parent)
    : QObject(parent)
    , _clientSocket(new QTcpSocket(this))
    , _loggedIn(false)
{
    // коннекты между сигналами клиента и qtcpsocket

    connect(_clientSocket, &QTcpSocket::connected, this, &Client::connected);
    connect(_clientSocket, &QTcpSocket::disconnected, this, &Client::disconnected);
    connect(_clientSocket, &QTcpSocket::readyRead, this, &Client::onReadyRead);
    connect(_clientSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &Client::error);
    connect(_clientSocket, &QTcpSocket::disconnected, this, [this]()->void{_loggedIn = false;});
}

void Client::connectToServer(const QHostAddress &address, quint16 port)
{
    _clientSocket->connectToHost(address, port);
}

void Client::disconnectFromHost()
{
    _clientSocket->disconnectFromHost();
}

void Client::login(const QString &nickname)
{
    // если соединение установлено - записываем в сокет никнейм и тип операции - login
    if (_clientSocket->state() == QAbstractSocket::ConnectedState)
    {
        QDataStream clientStream(_clientSocket);
        QJsonObject message;
        message[QStringLiteral("type")] = QStringLiteral("login");
        message[QStringLiteral("nickname")] = nickname;
        clientStream << QJsonDocument(message).toJson(QJsonDocument::Compact);
    }
}

void Client::sendMessage(const QString &text)
{
    if (text.isEmpty())
        return;

    // записываем сообщение в сокет в json формате
    QDataStream clientStream(_clientSocket);
    QJsonObject message;
    message[QStringLiteral("type")] = QStringLiteral("message");
    message[QStringLiteral("text")] = text;

    clientStream << QJsonDocument(message).toJson();
}

void Client::jsonReceived(const QJsonObject &docObj)
{
    const QJsonValue typeVal = docObj.value(QLatin1String("type"));

    if (typeVal.isNull() || !typeVal.isString())
        return;

    // если авторизация
    if (typeVal.toString().compare(QLatin1String("login"), Qt::CaseInsensitive) == 0)
    {
        // уже авторизован - выходим
        if (_loggedIn)
            return;

        // вытягиваем результат операции
        const QJsonValue resultVal = docObj.value(QLatin1String("success"));
        if (resultVal.isNull() || !resultVal.isBool())
            return;

        // приводим к bool
        const bool loginSuccess = resultVal.toBool();

        // если авторизация успешна - вызов сигнала
        if (loginSuccess)
        {
            emit loggedIn();
            return;
        }

        const QJsonValue reasonVal = docObj.value(QLatin1String("reason"));
        emit loginError(reasonVal.toString());
    }

    // если пришло сообщение
    else if (typeVal.toString().compare(QLatin1String("message"), Qt::CaseInsensitive) == 0)
    {
        const QJsonValue textVal = docObj.value(QLatin1String("text"));
        const QJsonValue senderVal = docObj.value(QLatin1String("sender"));


        if (textVal.isNull() || !textVal.isString())
            return;
        if (senderVal.isNull() || !senderVal.isString())
            return;

        // печать сообщения, если корректные данные
        emit messageReceived(senderVal.toString(), textVal.toString());
    }

    // подключился новый пользователь
    else if (typeVal.toString().compare(QLatin1String("newuser"), Qt::CaseInsensitive) == 0)
    {
        const QJsonValue nicknameVal = docObj.value(QLatin1String("nickname"));
        if (nicknameVal.isNull() || !nicknameVal.isString())
            return;

        // печать на экран
        emit userJoined(nicknameVal.toString());
    }

    // пользователь отключился
    else if (typeVal.toString().compare(QLatin1String("userdisconnected"), Qt::CaseInsensitive) == 0)
    {
        const QJsonValue nicknameVal = docObj.value(QLatin1String("nickname"));
        if (nicknameVal.isNull() || !nicknameVal.isString())
            return;

        // печать на экран
        emit userLeft(nicknameVal.toString());
    }
}

void Client::onReadyRead()
{
    QByteArray jsonData;
    QDataStream socketStream(_clientSocket);


    while (true)
    {
        // считываем инфу из сокета
        socketStream.startTransaction();
        socketStream >> jsonData;

        // если ошибок чтения не произошло - проверяем на ошибки json
        if (socketStream.commitTransaction())
        {
            QJsonParseError parseError;
            const QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);
            if (parseError.error == QJsonParseError::NoError)
            {
                if (jsonDoc.isObject())
                    jsonReceived(jsonDoc.object());
            }
        }

        // ошибка чтения - выход из цикла
        else
        {
            break;
        }
    }
}

