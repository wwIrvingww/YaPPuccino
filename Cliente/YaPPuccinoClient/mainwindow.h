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
    void on_enviarMsgGeneral_clicked();
    void onTextMessageReceived(const QString &message);
    void onBinaryMessageReceived(const QByteArray &data);
    void onManualStatusChange(int index);
    void onrefreshUserListClick();
    void onSearchNameClicked();

private:
    Ui::MainWindow *ui;
    QWebSocket socket;
    QNetworkAccessManager http;
    QString currentUser;
    QStringListModel *userModel;
    QHash<QString, QString> userStates;
};
#endif // MAINWINDOW_H
