#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QUrl>
#include <QDebug>
#include <QTextBrowser>
#include <QStringListModel>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    userModel = new QStringListModel(this);
    ui->listView->setModel(userModel);

    ui->changeStateComboBox->addItem("Activo", 1);
    ui->changeStateComboBox->addItem("Ocupado", 2);

    connect(&socket, &QWebSocket::binaryMessageReceived, this, &MainWindow::onBinaryMessageReceived);
    connect(&socket, &QWebSocket::connected, this, &MainWindow::onConnected);
    connect(&socket, &QWebSocket::disconnected, this, &MainWindow::onDisconnected);
    connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
            this, &MainWindow::onErrorOccurred);

    connect(&socket, &QWebSocket::textMessageReceived,
            this, &MainWindow::onTextMessageReceived);

    connect(ui->changeStateComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onManualStatusChange);

    connect(ui->refreshUserList, &QPushButton::clicked, this, &MainWindow::onrefreshUserListClick);

    connect(ui->buscarNombreBtn, &QPushButton::clicked, this, &MainWindow::onSearchNameClicked);

}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_connectButton_clicked()
{
    QString username = ui->lineEdit->text().trimmed();
    if(username.isEmpty()) {
        QMessageBox::warning(this, "Error", "Debes ingresar un nombre.");
        return;
    }

    ui->statusbar->showMessage("Verificando usuario...");
    QUrl httpUrl = QUrl(QString("http://3.134.168.244:5000?name=%1").arg(username));
    auto *reply = http.get(QNetworkRequest(httpUrl));
    connect(reply, &QNetworkReply::finished, this, [this, reply, username]() {
        int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
        if(code == 400) {
            ui->statusbar->showMessage("Error 400: El nombre de ya está en uso.");
            QMessageBox::warning(this, "Error 400", "El nombre ya está en uso.");
        } else if(code >= 200 && code < 300) {
            ui->statusbar->showMessage("Conectando WebSocket...");
            currentUser = username;
            socket.open(QUrl(QString("ws://3.134.168.244:5000?name=%1").arg(username)));
        } else {
            ui->statusbar->showMessage("Error HTTP " + QString::number(code));
            QMessageBox::critical(this, "Error HTTP", "Código: " + QString::number(code));
        }
    });
}

void MainWindow::onrefreshUserListClick()
{
    if (socket.isValid() && socket.state() == QAbstractSocket::ConnectedState) {
        QByteArray message;
        message.append(char(1));
        socket.sendBinaryMessage(message);
        ui->statusbar->showMessage(" Solicitando lista de usuarios...");
    } else {
        QMessageBox::warning(this, "Error", "No estás conectado al servidor.");
    }
}

void MainWindow::onSearchNameClicked()
{
    QString target = ui->buscarNombreInput->toPlainText().trimmed();

    if (target.isEmpty()) {
        QMessageBox::warning(this, "Error", "Debes ingresar un nombre de usuario.");
        return;
    }

    QByteArray raw = target.toUtf8();

    QByteArray request;
    request.append(char(2));
    request.append(char(raw.length()));      // Len username
    request.append(raw);                     // Username en UTF-8

    socket.sendBinaryMessage(request);
    ui->statusbar->showMessage("Solicitando historial de: " + target);
}


void MainWindow::onConnected()
{
    ui->statusbar->showMessage("Conectado ✅");
    QMessageBox::information(this, "Conexión", "Conectado correctamente al servidor.");
    QByteArray request;
    request.append(char(1));
    socket.sendBinaryMessage(request);
}

void MainWindow::onErrorOccurred(QAbstractSocket::SocketError)
{
    QString err = socket.errorString();

    qDebug() << "[DEBUG] WebSocket errorString() =" << err;

    if (err.contains("400")) {
        QMessageBox::warning(this, "Error de conexión",
                             "El nombre de usuario ya está en uso.");
    } else {
        QMessageBox::critical(this, "Error de conexión", err);
    }
    ui->statusbar->showMessage("Estado: Error");
}

void MainWindow::onDisconnected()
{
    ui->statusbar->showMessage("Desconectado");
    QMessageBox::information(this, "Desconexión", "Se ha desconectado del servidor.");
}

void MainWindow::on_enviarMsgGeneral_clicked()
{
    QString text = ui->msgGeneralTextEdit->toPlainText().trimmed();
    if (text.isEmpty()) return;

    socket.sendTextMessage(text);

    ui->chatGeneralTextEdit->append(
        "<p align='right'><b>You:</b> " + text.toHtmlEscaped() + "</p>"
        );


    ui->msgGeneralTextEdit->clear();
}


void MainWindow::onTextMessageReceived(const QString &message)
{
    if (message.startsWith(currentUser + ":")) return;
    ui->chatGeneralTextEdit->append(
        "<p align='left'>" + message.toHtmlEscaped() + "</p>"
        );

}

void MainWindow::onBinaryMessageReceived(const QByteArray &data)
{
    qDebug() << "[BINARIO] Mensaje recibido:" << data.toHex(' ').toUpper();
    const auto bytes = reinterpret_cast<const unsigned char*>(data.constData());
    int pos = 0;
    uint8_t code = bytes[pos++];

    if (code == 51) {
        if (pos >= data.size()) return;

        uint8_t numUsers = bytes[pos++];
        userStates.clear();

        for (int i = 0; i < numUsers; ++i) {
            // Se requieren al menos 2 bytes: len + estado
            if (pos + 2 > data.size()) {
                qDebug() << "[WARN] No hay suficientes datos para el usuario #" << i;
                break;
            }

            uint8_t nameLen = bytes[pos++];

            // Asegurarse de que hay suficientes bytes para el nombre y el estado
            if (pos + nameLen >= data.size()) {
                qDebug() << "[WARN] Longitud de nombre inválida para el usuario #" << i;
                break;
            }

            QByteArray rawName(reinterpret_cast<const char*>(bytes + pos), nameLen);
            QString username = QString::fromUtf8(rawName);
            if (username.contains('%')) {
                username = QUrl::fromPercentEncoding(rawName);
            }
            pos += nameLen;

            uint8_t status = bytes[pos++];

            QString estado;
            switch (status) {
            case 0: estado = "DESACTIVADO"; break;
            case 1: estado = "ACTIVO";      break;
            case 2: estado = "OCUPADO";     break;
            case 3: estado = "INACTIVO";    break;
            default: estado = "DESCONOCIDO";
            }

            qDebug() << "Usuario:" << username << "Estado:" << estado;
            userStates[username] = estado;
        }

        // Actualizar la vista
        QStringList rows;
        for (auto it = userStates.constBegin(); it != userStates.constEnd(); ++it) {
            rows << QString("%1 → %2").arg(it.key(), it.value());
        }
        userModel->setStringList(rows);
        return;
    }

    if (code == 50) {
        if (pos >= data.size()) {
            ui->statusbar->showMessage("Error: código de error no recibido.");
            return;
        }

        uint8_t errorCode = bytes[pos++];

        QString errorMsg;
        switch (errorCode) {
        case 1:
            errorMsg = "¡Ups! El usuario no existe.";
            break;
        case 2:
            errorMsg = "¡El estatus enviado es inválido!";
            break;
        case 3:
            errorMsg = "¡El mensaje está vacío!";
            break;
        case 4:
            errorMsg = "¡El mensaje fue enviado a un usuario con estatus desconectado!";
            break;
        default:
            errorMsg = "Error desconocido del servidor.";
            break;
        }

        // Mostrar retroalimentación
        ui->statusbar->showMessage(errorMsg);
        return;
    }

    if (code == 52) {
        // Validar que al menos hay un byte para len
        if (pos >= data.size()) {
            ui->mostrarNombre->setText("Error: mensaje incompleto.");
            return;
        }

        uint8_t nameLen = bytes[pos++];
        if (pos + nameLen > data.size()) {
            ui->mostrarNombre->setText("Error: nombre inválido.");
            return;
        }

        QString username = QString::fromUtf8(reinterpret_cast<const char*>(bytes + pos), nameLen);
        pos += nameLen;

        if (pos >= data.size()) {
            ui->mostrarNombre->setText("¡Ups! Usuario no existe.");
            return;
        }

        uint8_t status = bytes[pos++];

        QString estado;
        switch (status) {
        case 0: estado = "DESACTIVADO"; break;
        case 1: estado = "ACTIVO"; break;
        case 2: estado = "OCUPADO"; break;
        case 3: estado = "INACTIVO"; break;
        default: estado = "DESCONOCIDO";
        }

        ui->mostrarNombre->setText(QString("%1 → %2").arg(username, estado));
        return;
    }

    if (code == 54) {
        if (pos + 2 > data.size()) return;

        uint8_t nameLen = bytes[pos++];
        QString username = QString::fromUtf8(reinterpret_cast<const char*>(bytes + pos), nameLen);
        pos += nameLen;

        uint8_t stateCode = bytes[pos++];

        QString estado;
        switch (stateCode) {
        case 1: estado = "ACTIVO"; break;
        case 2: estado = "OCUPADO"; break;
        case 3: estado = "INACTIVO"; break;
        default: estado = "DESCONOCIDO"; break;
        }

        userStates[username] = estado;

        // Si el cambio es del usuario actual
        if (username == currentUser) {
            ui->changeStateComboBox->blockSignals(true);

            if (stateCode == 3) {
                ui->statusbar->showMessage("Fuiste puesto en estado INACTIVO por inactividad.");

                // Limpiar opciones del combo
                ui->changeStateComboBox->clear();

                // Mostrar "Inactivo" como opción seleccionada
                ui->changeStateComboBox->addItem("Inactivo", 3);
                ui->changeStateComboBox->addItem("Ocupado", 2);  // Única opción disponible
                ui->changeStateComboBox->setCurrentIndex(0);     // Seleccionar "Inactivo"
            } else {
                // Restaurar opciones normales
                ui->changeStateComboBox->clear();
                ui->changeStateComboBox->addItem("Activo", 1);
                ui->changeStateComboBox->addItem("Ocupado", 2);

                int index = ui->changeStateComboBox->findData(stateCode);
                if (index != -1) {
                    ui->changeStateComboBox->setCurrentIndex(index);
                }
            }

            ui->changeStateComboBox->setEnabled(true);
            ui->changeStateComboBox->blockSignals(false);
        }

        // Refrescar UI si quieres
        QStringList rows;
        for (auto it = userStates.constBegin(); it != userStates.constEnd(); ++it) {
            rows << QString("%1 → %2").arg(it.key(), it.value());
        }
        userModel->setStringList(rows);
        return;
    }
}

void MainWindow::onManualStatusChange(int index)
{
    int statusCode = ui->changeStateComboBox->itemData(index).toInt();

    // Prepara mensaje binario
    QByteArray message;
    message.append(char(3)); // code = CHANGE_STATUS
    QByteArray rawName = currentUser.toUtf8();
    message.append(char(rawName.length()));
    message.append(rawName);

    message.append(char(statusCode));

    qDebug() << "[DEBUG] Enviando cambio de estado:" << message.toHex(' ').toUpper();

    socket.sendBinaryMessage(message);

}



