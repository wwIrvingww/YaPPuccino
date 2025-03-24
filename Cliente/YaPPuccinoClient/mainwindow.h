#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtWebSockets/QtWebSockets>
#include <QNetworkAccessManager>
#include <QNetworkReply>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_connectButton_clicked();
    void onConnected();
    void onErrorOccurred(QAbstractSocket::SocketError);
    void onDisconnected();

private:
    Ui::MainWindow *ui;
    QWebSocket socket;
    QNetworkAccessManager http;
};
#endif // MAINWINDOW_H
