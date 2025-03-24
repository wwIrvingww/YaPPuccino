#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QUrl>
#include <QDebug>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(&socket, &QWebSocket::connected, this, &MainWindow::onConnected);
    connect(&socket, &QWebSocket::disconnected, this, &MainWindow::onDisconnected);
    connect(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
            this, &MainWindow::onErrorOccurred);

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
            socket.open(QUrl(QString("ws://3.134.168.244:5000?name=%1").arg(username)));
        } else {
            ui->statusbar->showMessage("Error HTTP " + QString::number(code));
            QMessageBox::critical(this, "Error HTTP", "Código: " + QString::number(code));
        }
    });
}

void MainWindow::onConnected()
{
    ui->statusbar->showMessage("Conectado ✅");
    QMessageBox::information(this, "Conexión", "Conectado correctamente al servidor.");
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

