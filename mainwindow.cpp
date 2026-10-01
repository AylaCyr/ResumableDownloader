#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QNetworkRequest>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      networkManager(new QNetworkAccessManager(this))
{
    ui->setupUi(this);
    ui->pauseButton->setEnabled(false);

    connect(ui->browseButton, &QPushButton::clicked, this, [this]()
    {
        QString path = QFileDialog::getExistingDirectory(this, "选择保存位置");

        if (!path.isEmpty())
        {
            ui->pathLineEdit->setText(path);
        }
    });

    connect(ui->downloadButton, &QPushButton::clicked, this, [this]()
    {
        if (downloadState == DownloadState::Downloading && currentReply != nullptr)
        {
            downloadState = DownloadState::Idle;

            ui->statusLabel->setText("正在取消...");
            ui->downloadButton->setEnabled(false);
            ui->pauseButton->setEnabled(false);

            currentReply->abort();
            return;
        }

        if (downloadState == DownloadState::Paused)
        {
            if (currentFile != nullptr)
            {
                currentFile->remove();
                currentFile->deleteLater();
                currentFile = nullptr;
            }

            downloadState = DownloadState::Idle;

            ui->progressBar->setValue(0);
            ui->progressInfoLabel->setText("进度：--");
            ui->speedLabel->setText("速度：--");
            ui->remainingTimeLabel->setText("剩余时间：--");
            ui->statusLabel->setText("状态：下载已取消");

            ui->downloadButton->setText("开始下载");
            ui->downloadButton->setEnabled(true);
            ui->pauseButton->setText("暂停下载");
            ui->pauseButton->setEnabled(false);

            return;
        }

        QString urlText = ui->urlLineEdit->text();
        QString path = ui->pathLineEdit->text();

        if (urlText.isEmpty())
        {
            ui->statusLabel->setText("请输入下载链接");
            return;
        }

        if (path.isEmpty())
        {
            ui->statusLabel->setText("请选择保存位置");
            return;
        }

        QUrl url(urlText);

        if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https") || url.host().isEmpty())
        {
            ui->statusLabel->setText("请输入有效的 HTTP/HTTPS 下载链接");
            return;
        }

        QString fileName = url.fileName();

        if (fileName.isEmpty())
        {
            fileName = "downloaded_file";
        }

        QString filePath = QDir(path).filePath(fileName);

        if (QFile::exists(filePath))
        {
            QMessageBox::StandardButton result = QMessageBox::question(
                this,
                "文件已存在",
                "目标文件已存在，是否覆盖？",
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);

            if (result == QMessageBox::No)
            {
                ui->statusLabel->setText("状态：已取消下载");
                return;
            }
        }

        currentFile = new QFile(filePath, this);

        if (!currentFile->open(QIODevice::WriteOnly))
        {
            ui->statusLabel->setText("文件创建失败");
            currentFile->deleteLater();
            currentFile = nullptr;
            return;
        }

        currentUrl = url;
        totalFileBytes = 0;
        startRequest(0);
    });

    connect(ui->pauseButton, &QPushButton::clicked, this, [this]()
    {
        if (downloadState == DownloadState::Downloading && currentReply != nullptr)
        {
            downloadState = DownloadState::Paused;
            ui->statusLabel->setText("状态：正在暂停...");
            ui->pauseButton->setEnabled(false);
            currentReply->abort();
            return;
        }

        if (downloadState == DownloadState::Paused && currentFile != nullptr)
        {
            qint64 offset = currentFile->size();

            if (!currentFile->open(QIODevice::WriteOnly | QIODevice::Append))
            {
                ui->statusLabel->setText("无法重新打开下载文件");
                return;
            }

            startRequest(offset);
        }
    });
}

void MainWindow::startRequest(qint64 offset)
{
    requestOffset = offset;
    rangeRequestRejected = false;

    QNetworkRequest request(currentUrl);

    if (offset > 0)
    {
        QByteArray range = "bytes=" + QByteArray::number(offset) + "-";
        request.setRawHeader("Range", range);
    }

    currentReply = networkManager->get(request);

    connect(currentReply, &QNetworkReply::metaDataChanged, this, [this]()
    {
        if (requestOffset <= 0)
        {
            return;
        }

        int statusCode = currentReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (statusCode != 206)
        {
            rangeRequestRejected = true;
            downloadState = DownloadState::Paused;
            currentReply->abort();
        }
    });

    downloadState = DownloadState::Downloading;
    speedTimer.start();
    lastReceivedBytes = 0;

    ui->statusLabel->setText("状态：正在下载...");
    ui->downloadButton->setText("取消下载");
    ui->pauseButton->setText("暂停下载");
    ui->pauseButton->setEnabled(true);

    if (offset == 0)
    {
        ui->progressBar->setValue(0);
        ui->progressInfoLabel->setText("进度：0.00 MB / --");
    }

    ui->speedLabel->setText("速度：--");
    ui->remainingTimeLabel->setText("剩余时间：--");

    connect(currentReply, &QNetworkReply::readyRead, this, [this]()
    {
        currentFile->write(currentReply->readAll());
    });

    connect(currentReply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total)
    {
        qint64 actualReceived = requestOffset + received;
        qint64 actualTotal = -1;

        if (total > 0)
        {
            actualTotal = requestOffset + total;
            totalFileBytes = actualTotal;

            int progress = static_cast<int>(actualReceived * 100 / actualTotal);
            ui->progressBar->setValue(progress);
        }

        qint64 elapsedMs = speedTimer.elapsed();

        if (elapsedMs < 500)
        {
            return;
        }

        qint64 receivedBytes = received - lastReceivedBytes;
        double seconds = elapsedMs / 1000.0;
        double bytesPerSecond = receivedBytes / seconds;
        double megabytesPerSecond = bytesPerSecond / 1024.0 / 1024.0;
        double receivedMB = actualReceived / 1024.0 / 1024.0;

        if (actualTotal > 0 && bytesPerSecond > 0)
        {
            double totalMB = actualTotal / 1024.0 / 1024.0;
            qint64 remainingBytes = actualTotal - actualReceived;
            qint64 remainingSeconds = static_cast<qint64>(remainingBytes / bytesPerSecond);

            ui->progressInfoLabel->setText(
                QString("进度：%1 MB / %2 MB").arg(receivedMB, 0, 'f', 2).arg(totalMB, 0, 'f', 2));
            ui->speedLabel->setText(QString("速度：%1 MB/s").arg(megabytesPerSecond, 0, 'f', 2));
            ui->remainingTimeLabel->setText(QString("剩余时间：%1 秒").arg(remainingSeconds));
        }
        else
        {
            ui->progressInfoLabel->setText(QString("进度：%1 MB / 未知").arg(receivedMB, 0, 'f', 2));
            ui->speedLabel->setText(QString("速度：%1 MB/s").arg(megabytesPerSecond, 0, 'f', 2));
            ui->remainingTimeLabel->setText("剩余时间：--");
        }

        lastReceivedBytes = received;
        speedTimer.restart();
    });

    connect(currentReply, &QNetworkReply::finished, this, [this]()
    {
        currentFile->write(currentReply->readAll());
        currentFile->close();

        if (currentReply->error() == QNetworkReply::OperationCanceledError)
        {
            if (rangeRequestRejected)
            {
                ui->statusLabel->setText("服务器不支持断点续传，请取消后重新下载");
                ui->speedLabel->setText("速度：--");
                ui->remainingTimeLabel->setText("剩余时间：--");
                ui->pauseButton->setText("继续下载");
                ui->pauseButton->setEnabled(false);

                currentReply->deleteLater();
                currentReply = nullptr;
                return;
            }

            if (downloadState == DownloadState::Paused)
            {
                qint64 pausedBytes = currentFile->size();
                double pausedMB = pausedBytes / 1024.0 / 1024.0;

                if (totalFileBytes > 0)
                {
                    double totalMB = totalFileBytes / 1024.0 / 1024.0;

                    ui->progressInfoLabel->setText(
                        QString("进度：%1 MB / %2 MB").arg(pausedMB, 0, 'f', 2).arg(totalMB, 0, 'f', 2));

                    int progress = static_cast<int>(pausedBytes * 100 / totalFileBytes);
                    ui->progressBar->setValue(progress);
                }

                ui->statusLabel->setText("状态：下载已暂停");
                ui->speedLabel->setText("速度：--");
                ui->remainingTimeLabel->setText("剩余时间：--");
                ui->pauseButton->setText("继续下载");
                ui->pauseButton->setEnabled(true);

                currentReply->deleteLater();
                currentReply = nullptr;
                return;
            }

            currentFile->remove();
            ui->progressBar->setValue(0);
            ui->progressInfoLabel->setText("进度：--");
            ui->speedLabel->setText("速度：--");
            ui->remainingTimeLabel->setText("剩余时间：--");
            ui->statusLabel->setText("状态：下载已取消");
        }
        else if (currentReply->error() != QNetworkReply::NoError)
        {
            currentFile->remove();
            ui->statusLabel->setText("下载失败：" + currentReply->errorString());
            ui->speedLabel->setText("速度：--");
            ui->remainingTimeLabel->setText("剩余时间：--");
        }
        else
        {
            ui->progressBar->setValue(100);

            qint64 finalBytes = currentFile->size();
            double finalMB = finalBytes / 1024.0 / 1024.0;

            ui->progressInfoLabel->setText(
                QString("进度：%1 MB / %2 MB").arg(finalMB, 0, 'f', 2).arg(finalMB, 0, 'f', 2));
            ui->statusLabel->setText("状态：下载完成");
            ui->speedLabel->setText("速度：--");
            ui->remainingTimeLabel->setText("剩余时间：0 秒");
        }

        currentFile->deleteLater();
        currentReply->deleteLater();
        currentFile = nullptr;
        currentReply = nullptr;
        downloadState = DownloadState::Idle;

        ui->downloadButton->setText("开始下载");
        ui->downloadButton->setEnabled(true);
        ui->pauseButton->setText("暂停下载");
        ui->pauseButton->setEnabled(false);
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}
