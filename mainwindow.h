#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QElapsedTimer>
#include <QFile>
#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

QT_BEGIN_NAMESPACE
namespace Ui
{
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    enum class DownloadState
    {
        Idle,
        Downloading,
        Paused
    };

    void startRequest(qint64 offset);

    Ui::MainWindow* ui;
    QNetworkAccessManager* networkManager;
    QNetworkReply* currentReply = nullptr;
    QFile* currentFile = nullptr;
    QElapsedTimer speedTimer;

    qint64 lastReceivedBytes = 0;
    qint64 requestOffset = 0;
    qint64 totalFileBytes = 0;

    QUrl currentUrl;
    DownloadState downloadState = DownloadState::Idle;
    bool rangeRequestRejected = false;
};

#endif // MAINWINDOW_H
