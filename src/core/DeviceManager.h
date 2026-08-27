#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QTimer>
#include <QFileSystemWatcher>

struct StorageDevice {
    QString id;
    QString name;
    QString mountPath;
    QString deviceNode; // e.g. /dev/nvme0n1p2
    qint64 totalBytes = 0;
    qint64 freeBytes = 0;
    QString iconName;
    bool isMounted = false;
    bool isRemovable = false;
    bool isAndroid = false;
    bool isNetwork = false;
};

class DeviceManager : public QObject {
    Q_OBJECT

public:
    static DeviceManager& instance();

    QList<StorageDevice> devices() const;
    QList<StorageDevice> networkMounts() const;

    bool mountDevice(const QString &deviceNode, QString *outMountPath = nullptr, QString *error = nullptr);
    bool unmountDevice(const QString &mountPath, QString *error = nullptr);

    bool connectRemoteServer(const QString &protocol, const QString &host, int port,
                             const QString &user, const QString &password, const QString &path,
                             QString *outMountPath = nullptr, QString *error = nullptr);

signals:
    void devicesChanged();

public slots:
    void refresh();

private:
    explicit DeviceManager(QObject *parent = nullptr);
    ~DeviceManager() override = default;

    void scanBlockDevices();
    void scanGvfsMounts();
    void scanStorageInfo();

    QList<StorageDevice> m_devices;
    QList<StorageDevice> m_networkMounts;
    QTimer m_pollTimer;
    QFileSystemWatcher m_watcher;
};
