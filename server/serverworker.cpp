#include "serverworker.h"
#include <QJsonDocument>
#include <QDataStream>
#include <QJsonObject>

ServerWorker::ServerWorker(QObject *parent)
    : QObject(parent)
    , _socket(new QTcpSocket(this))
{
    // коннекты между сигналами сокета и serverworker
    connect(_socket, &QTcpSocket::readyRead, this, &ServerWorker::receiveJson);
    connect(_socket, &QTcpSocket::disconnected, this, &ServerWorker::disconnectedFromClient);
    connect(_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &ServerWorker::error);
}

bool ServerWorker::setSocketDescriptor(qintptr socketDescriptor)
{
    return _socket->setSocketDescriptor(socketDescriptor);
}

QString ServerWorker::getNickname() const
{
    return _nickname;
}

void ServerWorker::setNickname(const QString &nickname)
{
    _nickname = nickname;
}


void ServerWorker::sendJson(const QJsonObject &json)
{
    // выводим в лог сообщение об отправленном json
    // и записываем в сокет сам json

    const QByteArray jsonData = QJsonDocument(json).toJson(QJsonDocument::Compact);
    emit logMessage(QLatin1String("Sending by ") + getNickname() + QLatin1String(" - ") + QString::fromUtf8(jsonData));
    QDataStream socketStream(_socket);
    socketStream << jsonData;
}

void ServerWorker::receiveJson()
{
    QByteArray jsonData;
    QDataStream socketStream(_socket);


    while (true)
    {
        // считываем json из сокета
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
                    emit jsonReceived(jsonDoc.object());
                else
                    emit logMessage(QLatin1String("invalid message: ") + QString::fromUtf8(jsonData));
            }
            else
            {
                emit logMessage(QLatin1String("invalid message: ") + QString::fromUtf8(jsonData));
            }
        }

        // ошибка чтения - выход из цикла
        else
        {
            break;
        }
    }
}


void ServerWorker::disconnectFromClient()
{
    _socket->disconnectFromHost();
}
