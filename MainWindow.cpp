#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "OnvifClient.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QDateTime>
#include <QUrl>
#include <QScrollBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupUI();
    m_client = new OnvifClient(this);

    connect(m_client, &OnvifClient::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(ui->discoverButton, &QPushButton::clicked, this, &MainWindow::onDiscoverClicked);

    setWindowTitle(tr("ONVIF Stream Discovery"));
    resize(1100, 800);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUI()
{
    // Configure profile table headers
    ui->profileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->profileTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->profileTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->profileTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->profileTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    ui->profileTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ui->profileTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);

    // Configure URL table headers
    ui->urlTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->urlTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->urlTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
}

void MainWindow::onDiscoverClicked()
{
    QString ip = ui->ipEdit->text().trimmed();
    QString user = ui->userEdit->text().trimmed();
    QString pass = ui->passEdit->text();
    int port = ui->portSpin->value();
    QString path = ui->pathEdit->text().trimmed();
    bool https = ui->httpsCheck->isChecked();
    int timeout = ui->timeoutSpin->value();

    if (ip.isEmpty() || user.isEmpty() || pass.isEmpty()) {
        QMessageBox::warning(this, tr("Missing Information"), tr("Please fill in IP, username, and password."));
        return;
    }

    if (!path.startsWith('/')) {
        path = '/' + path;
    }

    m_deviceUrl = QString("%1://%2:%3%4")
        .arg(https ? "https" : "http")
        .arg(ip)
        .arg(port)
        .arg(path);

    resetUI();
    ui->discoverButton->setEnabled(false);
    ui->discoverButton->setText(tr("Discovering..."));
    ui->progressBar->setVisible(true);

    addLog(tr("== ONVIF stream discovery for %1 ==").arg(ip));
    addLog(tr("Device service: %1").arg(m_deviceUrl));
    addLog("");

    m_client->setCredentials(user, pass);
    m_client->setTimeout(timeout);

    // Step 1: Get device time (unauthenticated)
    addLog(tr("-- Checking device reachability --"));
    m_client->getSystemDateAndTime(m_deviceUrl,
        [this](bool success, const QString &result) {
            onDeviceTimeResult(success, result);
        });
}

void MainWindow::onDeviceTimeResult(bool success, const QString &result)
{
    if (success && !result.isEmpty() && !result.startsWith("Could not")) {
        addLog(tr("  Device UTC time: %1").arg(result));
    } else {
        addLog(tr("  (could not read device time: %1)").arg(result));
    }
    addLog("");

    // Step 2: Get capabilities
    addLog(tr("-- GetCapabilities (locating Media service) --"));
    m_client->getCapabilities(m_deviceUrl,
        [this](bool success, const QString &result) {
            onCapabilitiesResult(success, result);
        });
}

void MainWindow::onCapabilitiesResult(bool success, const QString &result)
{
    if (success) {
        m_mediaUrl = result;
        addLog(tr("  Media service XAddr: %1").arg(m_mediaUrl));
        addLog("");

        // Step 3: Get profiles
        addLog(tr("-- GetProfiles --"));
        m_client->getProfiles(m_mediaUrl,
            [this](bool success, const QVector<ProfileInfo> &profiles, const QString &error) {
                onProfilesResult(success, profiles, error);
            });
    } else {
        addLog(tr("  GetCapabilities failed: %1").arg(result));
        addLog("");
        onErrorOccurred(result);
    }
}

void MainWindow::onProfilesResult(bool success, const QVector<ProfileInfo> &profiles, const QString &error)
{
    if (success) {
        m_profiles = profiles;
        updateProfileTable(profiles);

        for (const auto &p : profiles) {
            addLog(tr("  Profile token: %1  (name: %2)").arg(p.token, p.name));
            addLog(tr("    Video: %1").arg(formatVideoInfo(p.video)));
        }
        addLog("");

        // Step 4 & 5: Get stream and snapshot URIs for each profile
        addLog(tr("-- GetStreamUri / GetSnapshotUri per profile --"));
        m_pendingStreamRequests = profiles.size() * 2;
        m_streamResults.clear();
        m_streamResults.resize(profiles.size());

        for (int i = 0; i < profiles.size(); ++i) {
            const ProfileInfo &profile = profiles[i];
            addLog(tr("  [%1] token=%2").arg(profile.name, profile.token));

            m_client->getStreamUri(m_mediaUrl, profile.token,
                [this, i, profile](bool success, const QString &uri, const QString &error) {
                    onStreamUriResult(profile.token, success, uri, error);
                });

            m_client->getSnapshotUri(m_mediaUrl, profile.token,
                [this, i, profile](bool success, const QString &uri, const QString &error) {
                    onSnapshotUriResult(profile.token, success, uri, error);
                });
        }
    } else {
        addLog(QString("  GetProfiles failed: %1").arg(error));
        addLog("");
        onErrorOccurred(error);
    }
}

void MainWindow::onStreamUriResult(const QString &profileToken, bool success, const QString &uri, const QString &err)
{
    int profileIndex = -1;
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles[i].token == profileToken) {
            profileIndex = i;
            break;
        }
    }

    if (profileIndex >= 0) {
        StreamResult &result = m_streamResults[profileIndex];
        result.profileName = m_profiles[profileIndex].name;
        result.video = m_profiles[profileIndex].video;

        if (success && !uri.isEmpty()) {
            result.rtspUriRaw = uri;
            result.rtspUri = withCredentials(uri);
            addLog(tr("    RTSP:     %1").arg(maskUrl(result.rtspUri)));
            addLog(tr("    (raw, no creds): %1").arg(uri));
        } else {
            addLog(tr("    RTSP:     not returned (%1)").arg(err));
        }
    }

    checkStreamRequestsComplete();
}

void MainWindow::onSnapshotUriResult(const QString &profileToken, bool success, const QString &uri, const QString &)
{
    int profileIndex = -1;
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles[i].token == profileToken) {
            profileIndex = i;
            break;
        }
    }

    if (profileIndex >= 0) {
        StreamResult &result = m_streamResults[profileIndex];

        if (success && !uri.isEmpty()) {
            result.snapshotUri = withCredentials(uri);
            addLog(tr("    Snapshot: %1").arg(maskUrl(result.snapshotUri)));
        } else {
            // Silently ignore missing snapshot URI
        }
    }

    checkStreamRequestsComplete();
}

void MainWindow::checkStreamRequestsComplete()
{
    m_pendingStreamRequests--;
    if (m_pendingStreamRequests <= 0) {
        updateStreamResults();
        updateUrlTable();

        addLog("");
        addLog(tr("== Summary =="));
        bool anyResults = false;
        for (const StreamResult &result : m_streamResults) {
            if (!result.rtspUri.isEmpty()) {
                anyResults = true;
                addLog(tr("- %1: %2").arg(result.profileName, maskUrl(result.rtspUri)));
                addLog(tr("    %1").arg(formatVideoInfo(result.video)));
            }
        }

        if (!anyResults) {
            addLog(tr("No RTSP stream URIs were returned. Double-check credentials and that"));
            addLog(tr("the ONVIF service path/port are correct for this device."));
        } else {
            addLog("");
            addLog(tr("Use these URLs directly in VLC, ffmpeg, or your NVR/recording software."));
        }

        ui->discoverButton->setEnabled(true);
        ui->discoverButton->setText(tr("Discover Streams"));
        ui->progressBar->setVisible(false);
    }
}

void MainWindow::updateProfileTable(const QVector<ProfileInfo> &profiles)
{
    ui->profileTable->setRowCount(profiles.size());
    m_profileTokenToRow.clear();

    for (int i = 0; i < profiles.size(); ++i) {
        const ProfileInfo &p = profiles[i];
        m_profileTokenToRow[p.token] = i;

        ui->profileTable->setItem(i, 0, new QTableWidgetItem(p.token));
        ui->profileTable->setItem(i, 1, new QTableWidgetItem(p.name));

        QString resolution = QString("%1x%2").arg(p.video.width).arg(p.video.height);
        if (p.video.width == 0 || p.video.height == 0) {
            resolution = "N/A";
        }
        ui->profileTable->setItem(i, 2, new QTableWidgetItem(resolution));

        QString fps = p.video.fps > 0 ? QString::number(p.video.fps) : "N/A";
        ui->profileTable->setItem(i, 3, new QTableWidgetItem(fps));

        QString codec = p.video.encoding;
        if (!p.video.codecProfile.isEmpty()) {
            codec += QString(" (%1)").arg(p.video.codecProfile);
        }
        if (codec.isEmpty()) codec = "N/A";
        ui->profileTable->setItem(i, 4, new QTableWidgetItem(codec));

        QString bitrate = p.video.bitrateKbps > 0 ? QString("%1 kbps").arg(p.video.bitrateKbps) : "N/A";
        ui->profileTable->setItem(i, 5, new QTableWidgetItem(bitrate));

        QString gov = p.video.govLength > 0 ? QString::number(p.video.govLength) : "N/A";
        ui->profileTable->setItem(i, 6, new QTableWidgetItem(gov));
    }
}

void MainWindow::updateStreamResults()
{
    // The log already contains the stream results
}

void MainWindow::updateUrlTable()
{
    ui->urlTable->setRowCount(0);
    
    for (const StreamResult &result : m_streamResults) {
        if (result.rtspUri.isEmpty()) continue;
        
        int row = ui->urlTable->rowCount();
        
        // Video Stream (RTSP with credentials)
        ui->urlTable->insertRow(row);
        ui->urlTable->setItem(row, 0, new QTableWidgetItem(result.profileName));
        ui->urlTable->setItem(row, 1, new QTableWidgetItem("Video Stream (RTSP)"));
        ui->urlTable->setItem(row, 2, new QTableWidgetItem(maskUrl(result.rtspUri)));
        
        // Video Stream (raw)
        row = ui->urlTable->rowCount();
        ui->urlTable->insertRow(row);
        ui->urlTable->setItem(row, 0, new QTableWidgetItem(result.profileName));
        ui->urlTable->setItem(row, 1, new QTableWidgetItem("Video Stream (raw)"));
        ui->urlTable->setItem(row, 2, new QTableWidgetItem(result.rtspUriRaw));
        
        // Snapshot (if available)
        if (!result.snapshotUri.isEmpty()) {
            row = ui->urlTable->rowCount();
            ui->urlTable->insertRow(row);
            ui->urlTable->setItem(row, 0, new QTableWidgetItem(result.profileName));
            ui->urlTable->setItem(row, 1, new QTableWidgetItem("Snapshot (JPEG)"));
            ui->urlTable->setItem(row, 2, new QTableWidgetItem(maskUrl(result.snapshotUri)));
        }
    }
}

QString MainWindow::maskUrl(const QString &url)
{
    QUrl u(url);
    if (!u.password().isEmpty()) {
        u.setPassword("***");
    }
    return u.toString();
}

QString MainWindow::formatVideoInfo(const VideoEncoderConfig &video)
{
    if (!video.isValid()) {
        return tr("(no VideoEncoderConfiguration in profile — device may not expose it here)");
    }

    QStringList parts;
    if (video.width > 0 && video.height > 0) {
        parts << QString("%1x%2").arg(video.width).arg(video.height);
    }
    if (video.fps > 0) {
        parts << tr("%1 fps").arg(video.fps);
    }
    if (!video.encoding.isEmpty()) {
        QString codec = video.encoding;
        if (!video.codecProfile.isEmpty()) {
            codec += tr(" (%1)").arg(video.codecProfile);
        }
        parts << codec;
    }
    if (video.bitrateKbps > 0) {
        parts << tr("%1 kbps").arg(video.bitrateKbps);
    }
    if (video.govLength > 0) {
        parts << tr("GOV %1").arg(video.govLength);
    }
    if (!video.quality.isEmpty()) {
        parts << tr("quality %1").arg(video.quality);
    }

    return parts.isEmpty() ? tr("(VideoEncoderConfiguration present but no fields populated)") : parts.join(", ");
}

QString MainWindow::withCredentials(const QString &uri)
{
    QUrl u(uri);
    if (u.isValid() && !u.host().isEmpty()) {
        u.setUserName(QUrl::toPercentEncoding(ui->userEdit->text()));
        u.setPassword(QUrl::toPercentEncoding(ui->passEdit->text()));
    }
    return u.toString();
}

void MainWindow::resetUI()
{
    ui->profileTable->setRowCount(0);
    ui->urlTable->setRowCount(0);
    ui->logText->clear();
    m_mediaUrl.clear();
    m_profiles.clear();
    m_streamResults.clear();
    m_profileTokenToRow.clear();
    m_pendingStreamRequests = 0;
}

void MainWindow::onErrorOccurred(const QString &message)
{
    addLog(tr("ERROR: %1").arg(message));
    QMessageBox::critical(this, tr("Error"), message);
    ui->discoverButton->setEnabled(true);
    ui->discoverButton->setText(tr("Discover Streams"));
    ui->progressBar->setVisible(false);
}

void MainWindow::addLog(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    ui->logText->append(QString("[%1] %2").arg(timestamp, message));
    ui->logText->verticalScrollBar()->setValue(ui->logText->verticalScrollBar()->maximum());
}