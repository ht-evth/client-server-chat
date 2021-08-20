#include "clientwindow.h"
#include "ui_clientwindow.h"
#include "client.h"
#include "QStandardItemModel"
#include "QHostAddress"
#include "QMessageBox"
#include "QInputDialog"

ClientWindow::ClientWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ClientWindow)
    , _client(new Client(this))
    , _chatModel(new QStandardItemModel(this))
{
    ui->setupUi(this);

    _chatModel->insertColumn(0);
    ui->messagesView->setModel(_chatModel);

    // коннекты сигналов между классом клиент и окном клиента
    connect(_client, &Client::connected, this, &ClientWindow::connectedToServer);
    connect(_client, &Client::loggedIn, this, &ClientWindow::loggedIn);
    connect(_client, &Client::loginError, this, &ClientWindow::loginFailed);
    connect(_client, &Client::messageReceived, this, &ClientWindow::messageReceived);
    connect(_client, &Client::disconnected, this, &ClientWindow::disconnectedFromServer);
    connect(_client, &Client::error, this, &ClientWindow::error);
    connect(_client, &Client::userJoined, this, &ClientWindow::userJoined);
    connect(_client, &Client::userLeft, this, &ClientWindow::userLeft);

    // коннекты для gui
    connect(ui->pb_connect, &QPushButton::clicked, this, &ClientWindow::attemptConnection);
    connect(ui->pb_send, SIGNAL(clicked()), this, SLOT(sendMessage()));
    connect(ui->le_message, SIGNAL(returnPressed()), this, SLOT(sendMessage()));
}

ClientWindow::~ClientWindow()
{
    delete ui;
}

void ClientWindow::attemptConnection()
{
    // попытка соединения - кнопка "connect" недоступна
    ui->pb_connect->setEnabled(false);
    _client->connectToServer(QHostAddress(ui->lineEdit_ip->text()), ui->lineEdit_port->text().toInt());
}

void ClientWindow::connectedToServer()
{
    // соединение установлено - запрос ввода никнейма

    const QString newNickname = QInputDialog::getText(this, tr("Input your nickname"), tr("nickname"));

    if (newNickname.isEmpty())
        return _client->disconnectFromHost();

    // попытка подключиться с введенным никнеймом
    attemptLogin(newNickname);
}

void ClientWindow::attemptLogin(const QString &nickname)
{
    // попытка подключиться с заданным никнеймом
    _client->login(nickname);
}

void ClientWindow::loggedIn()
{
    // если клиент залогинен - активируем gui

    ui->pb_send->setEnabled(true);
    ui->le_message->setEnabled(true);
    ui->messagesView->setEnabled(true);
    _lastNickname.clear(); // очищаем информацию о никнейме, кто писал последним
}

void ClientWindow::loginFailed(const QString &reason)
{
    // Ошибка логина - снова запрос никнейма
    QMessageBox::critical(this, tr("Error"), reason);
    connectedToServer();
}


void ClientWindow::messageReceived(const QString &sender, const QString &text)
{
    // пришло сообщение

    int newRow = _chatModel->rowCount();

    // если пришло сообщение от пользователя, отличного от того, который написал последнее сообщение
    // меняем значение _lastNickname для красивого вывода на экран
    if (_lastNickname != sender)
    {
        _lastNickname = sender; // теперь пишет другой пользователь

        // выводим никнейм пользователя
        QFont boldFont;
        boldFont.setBold(true);
        _chatModel->insertRows(newRow, 2);
        _chatModel->setData(_chatModel->index(newRow, 0), sender + QLatin1Char(':'));
        _chatModel->setData(_chatModel->index(newRow, 0), int(Qt::AlignLeft | Qt::AlignVCenter), Qt::TextAlignmentRole);
        _chatModel->setData(_chatModel->index(newRow, 0), boldFont, Qt::FontRole);
        ++newRow;
    }
    else
    {
        // продолжает писать тот же пользователь
        _chatModel->insertRow(newRow);
    }

    // печатаем сообщение слева на экране и пролистываем экран вниз
    _chatModel->setData(_chatModel->index(newRow, 0), text);
    _chatModel->setData(_chatModel->index(newRow, 0), int(Qt::AlignLeft | Qt::AlignVCenter), Qt::TextAlignmentRole);
    ui->messagesView->scrollToBottom();
}


void ClientWindow::sendMessage()
{
    // записываем сообщение в сокет
    _client->sendMessage(ui->le_message->text());

    // выводим сообщение справа на экране
    const int newRow = _chatModel->rowCount();
    _chatModel->insertRow(newRow);
    _chatModel->setData(_chatModel->index(newRow, 0), ui->le_message->text());
    _chatModel->setData(_chatModel->index(newRow, 0), int(Qt::AlignRight | Qt::AlignVCenter), Qt::TextAlignmentRole);

    // очищаем поле для ввода и пролистываем экран вниз
    ui->le_message->clear();
    ui->messagesView->scrollToBottom();

    _lastNickname.clear(); // очищаем ник предыдущего отправителя
}

void ClientWindow::disconnectedFromServer()
{
    // пропало соединение с сервером
    // вывод сообщения и настраиваем gui

    QMessageBox::warning(this, tr("Disconnected"), tr("The host terminated the connection"));
    ui->pb_send->setEnabled(false);
    ui->le_message->setEnabled(false);
    ui->messagesView->setEnabled(false);
    ui->pb_connect->setEnabled(true);

    _lastNickname.clear(); // очищаем ник предыдущего отправителя
}

void ClientWindow::userJoined(const QString &nickname)
{
    // печать на экран информации о том, что подключен новый пользователь

    const int newRow = _chatModel->rowCount();
    _chatModel->insertRow(newRow);
    _chatModel->setData(_chatModel->index(newRow, 0), tr("%1 is connected").arg(nickname));
    _chatModel->setData(_chatModel->index(newRow, 0), Qt::AlignCenter, Qt::TextAlignmentRole);
    _chatModel->setData(_chatModel->index(newRow, 0), QBrush(Qt::blue), Qt::ForegroundRole);
    ui->messagesView->scrollToBottom();

    _lastNickname.clear();
}

void ClientWindow::userLeft(const QString &nickname)
{
    // печать на экран информации о том, что пользователь отключился

    const int newRow = _chatModel->rowCount();
    _chatModel->insertRow(newRow);
    _chatModel->setData(_chatModel->index(newRow, 0), tr("%1 disconnected").arg(nickname));
    _chatModel->setData(_chatModel->index(newRow, 0), Qt::AlignCenter, Qt::TextAlignmentRole);
    _chatModel->setData(_chatModel->index(newRow, 0), QBrush(Qt::red), Qt::ForegroundRole);
    ui->messagesView->scrollToBottom();

    _lastNickname.clear();
}

void ClientWindow::error(QAbstractSocket::SocketError socketError)
{

    switch (socketError) {
    case QAbstractSocket::RemoteHostClosedError:
    case QAbstractSocket::ProxyConnectionClosedError:
        return;
    case QAbstractSocket::ConnectionRefusedError:
        QMessageBox::critical(this, tr("Error"), tr("the server refused to connect"));
        break;
    case QAbstractSocket::SocketTimeoutError:
        QMessageBox::warning(this, tr("Error"), tr("Timeout error"));
        return;
    case QAbstractSocket::NetworkError:
        QMessageBox::critical(this, tr("Error"), tr("Network error"));
        break;
    case QAbstractSocket::UnknownSocketError:
        QMessageBox::critical(this, tr("Error"), tr("Unknown error"));
        break;
    case QAbstractSocket::TemporaryError:
    case QAbstractSocket::OperationError:
        QMessageBox::warning(this, tr("Error"), tr("The operation failed"));
        return;
    default:
        break;
    }

    ui->pb_connect->setEnabled(true);
    ui->pb_send->setEnabled(false);
    ui->le_message->setEnabled(false);
    ui->messagesView->setEnabled(false);
    _lastNickname.clear();
}

