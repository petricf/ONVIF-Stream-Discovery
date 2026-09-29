#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include "StreamInfo.h"

class OnvifClient;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onDiscoverClicked();
    void onDeviceTimeResult(bool success, const QString &result);
    void onCapabilitiesResult(bool success, const QString &result);
    void onProfilesResult(bool success, const QVector<ProfileInfo> &profiles, const QString &error);
    void onStreamUriResult(const QString &profileToken, bool success, const QString &uri, const QString &error);
    void onSnapshotUriResult(const QString &profileToken, bool success, const QString &uri, const QString &error);
    void onErrorOccurred(const QString &message);

    void addLog(const QString &message);

private:
    void setupUI();
    void resetUI();
    void updateProfileTable(const QVector<ProfileInfo> &profiles);
    void updateStreamResults();
    void updateUrlTable();
    QString maskUrl(const QString &url);
    QString formatVideoInfo(const VideoEncoderConfig &video);
    QString withCredentials(const QString &uri);
    void checkStreamRequestsComplete();

    Ui::MainWindow *ui = nullptr;
    OnvifClient *m_client = nullptr;

    // State
    QString m_deviceUrl;
    QString m_mediaUrl;
    QVector<ProfileInfo> m_profiles;
    QVector<StreamResult> m_streamResults;
    QMap<QString, int> m_profileTokenToRow;
    int m_pendingStreamRequests = 0;
};

#endif // MAINWINDOW_H