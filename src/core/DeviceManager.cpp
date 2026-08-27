#include "DeviceManager.h"
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStorageInfo>
#include <QDir>
#include <QFileInfo>
#include <unistd.h>

DeviceManager& DeviceManager::instance() {
    static DeviceManager inst;
    return inst;
}

DeviceManager::DeviceManager(QObject *parent)
    : QObject(parent)
{
    refresh();

    // Periodic poll every 3 seconds to detect hotplugged USB drives and Android phones
    connect(&m_pollTimer, &QTimer::timeout, this, &DeviceManager::refresh);
    m_pollTimer.start(3000);

    // Watch media directories for instant notification
    QString gvfsPath = QString("/run/user/%1/gvfs").arg(getuid());
    QString mediaUser = QString("/run/media/%1").arg(qgetenv("USER").constData());
    for (const QString &dir : QStringList({ QString("/media"), mediaUser, gvfsPath })) {
        if (QDir(dir).exists()) {
            m_watcher.addPath(dir);
        }
    }

    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &DeviceManager::refresh);
}

QList<StorageDevice> DeviceManager::devices() const {
    return m_devices;
}

QList<StorageDevice> DeviceManager::networkMounts() const {
    return m_networkMounts;
}

void DeviceManager::refresh() {
    QList<StorageDevice> newDevices;
    QList<StorageDevice> newNetwork;

    // 1. Scan GVFS Mounts (Android MTP, SFTP, SMB, WebDAV, Cameras)
    QString gvfsDir = QString("/run/user/%1/gvfs").arg(getuid());
    if (QDir(gvfsDir).exists()) {
        QDir dir(gvfsDir);
        QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &e : entries) {
            QString name = e.fileName();
            QString fullPath = e.absoluteFilePath();

            StorageDevice dev;
            dev.id = fullPath;
            dev.mountPath = fullPath;
            dev.isMounted = true;

            QStorageInfo st(fullPath);
            if (st.isValid()) {
                dev.totalBytes = st.bytesTotal();
                dev.freeBytes = st.bytesAvailable();
            }

            if (name.startsWith("mtp:host=")) {
                dev.name = tr("Android Device");
                dev.iconName = "multimedia-player-apple-ipod";
                dev.isAndroid = true;
                dev.isRemovable = true;
                newDevices.append(dev);
            } else if (name.startsWith("sftp:host=")) {
                QString hostInfo = name.mid(10);
                dev.name = QString("SFTP: %1").arg(hostInfo.section(',', 0, 0));
                dev.iconName = "network-server";
                dev.isNetwork = true;
                newNetwork.append(dev);
            } else if (name.startsWith("smb-share:") || name.startsWith("smb:")) {
                dev.name = QString("SMB: %1").arg(name.section('=', 1, 1));
                dev.iconName = "network-workgroup";
                dev.isNetwork = true;
                newNetwork.append(dev);
            } else if (name.startsWith("gphoto2:host=")) {
                dev.name = tr("Camera (PTP)");
                dev.iconName = "camera-photo";
                dev.isAndroid = true;
                dev.isRemovable = true;
                newDevices.append(dev);
            }
        }
    }

    // 2. Scan block devices using lsblk
    QProcess lsblkProc;
    lsblkProc.start("lsblk", { "-J", "-o", "NAME,SIZE,TYPE,MOUNTPOINT,LABEL,RM,MODEL" });
    if (lsblkProc.waitForFinished(1500) && lsblkProc.exitCode() == 0) {
        QJsonDocument doc = QJsonDocument::fromJson(lsblkProc.readAllStandardOutput());
        QJsonObject root = doc.object();
        QJsonArray blockdevices = root["blockdevices"].toArray();

        auto parseDevice = [&](const QJsonObject &obj, const QString &parentModel, auto &self) -> void {
            QString type = obj["type"].toString();
            if (type == "loop") return; // Skip snap loops

            QString name = obj["name"].toString();
            QString model = obj["model"].toString();
            if (model.isEmpty()) model = parentModel;
            QString label = obj["label"].toString();
            QString mountpoint = obj["mountpoint"].toString();
            bool isRemovable = obj["rm"].toBool();
            QString size = obj["size"].toString();

            if (type == "part" || type == "disk") {
                if (mountpoint == "[SWAP]" || mountpoint.startsWith("/boot")) {
                    // skip swap
                } else if (!mountpoint.isEmpty() || !label.isEmpty()) {
                    StorageDevice dev;
                    dev.deviceNode = "/dev/" + name;
                    dev.id = dev.deviceNode;
                    dev.mountPath = mountpoint;
                    dev.isMounted = !mountpoint.isEmpty();
                    dev.isRemovable = isRemovable;

                    if (mountpoint == "/") {
                        dev.name = tr("Root (Linux)");
                        dev.iconName = "drive-harddisk-root";
                    } else if (!label.isEmpty()) {
                        dev.name = label;
                        dev.iconName = isRemovable ? "media-flash" : "drive-harddisk";
                    } else if (!model.isEmpty()) {
                        dev.name = QString("%1 (%2)").arg(model, size);
                        dev.iconName = isRemovable ? "media-flash" : "drive-harddisk";
                    } else {
                        dev.name = QString("%1 (%2)").arg(name, size);
                        dev.iconName = isRemovable ? "media-flash" : "drive-harddisk";
                    }

                    if (dev.isMounted) {
                        QStorageInfo st(dev.mountPath);
                        if (st.isValid()) {
                            dev.totalBytes = st.bytesTotal();
                            dev.freeBytes = st.bytesAvailable();
                        }
                    }

                    newDevices.append(dev);
                }
            }

            QJsonArray children = obj["children"].toArray();
            for (const QJsonValue &childVal : children) {
                self(childVal.toObject(), model, self);
            }
        };

        for (const QJsonValue &val : blockdevices) {
            parseDevice(val.toObject(), QString(), parseDevice);
        }
    }

    m_devices = newDevices;
    m_networkMounts = newNetwork;
    emit devicesChanged();
}

bool DeviceManager::mountDevice(const QString &deviceNode, QString *outMountPath, QString *error) {
    if (deviceNode.isEmpty()) return false;

    QProcess proc;
    proc.start("udisksctl", { "mount", "-b", deviceNode });
    if (!proc.waitForFinished(4000) || proc.exitCode() != 0) {
        QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        if (error) *error = err.isEmpty() ? tr("Failed to mount drive") : err;
        return false;
    }

    QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    // Output format: "Mounted /dev/sdX at /media/user/label."
    if (out.contains(" at ")) {
        QString path = out.section(" at ", 1, 1).trimmed();
        if (path.endsWith('.')) path.chop(1);
        if (outMountPath) *outMountPath = path;
    }

    refresh();
    return true;
}

bool DeviceManager::unmountDevice(const QString &mountPath, QString *error) {
    if (mountPath.isEmpty()) return false;

    QProcess proc;
    if (mountPath.contains("/gvfs/")) {
        proc.start("gio", { "mount", "-u", mountPath });
    } else {
        proc.start("udisksctl", { "unmount", "-p", mountPath });
    }

    if (!proc.waitForFinished(4000) || proc.exitCode() != 0) {
        // Fallback to gio mount -u
        proc.start("gio", { "mount", "-u", mountPath });
        if (!proc.waitForFinished(3000) || proc.exitCode() != 0) {
            QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
            if (error) *error = err.isEmpty() ? tr("Failed to unmount device") : err;
            return false;
        }
    }

    refresh();
    return true;
}

bool DeviceManager::connectRemoteServer(const QString &protocol, const QString &host, int port,
                                       const QString &user, const QString &password, const QString &path,
                                       QString *outMountPath, QString *error)
{
    if (host.isEmpty()) {
        if (error) *error = tr("Host address cannot be empty.");
        return false;
    }

    QString scheme = protocol.toLower();
    if (scheme == "sftp" || scheme == "ssh") scheme = "sftp";
    else if (scheme == "ftp") scheme = "ftp";
    else if (scheme == "smb" || scheme == "windows share") scheme = "smb";
    else if (scheme == "webdav") scheme = "dav";

    QString uri;
    if (!user.isEmpty()) {
        uri = QString("%1://%2@%3:%4%5").arg(scheme, user, host).arg(port).arg(path.startsWith('/') ? path : ("/" + path));
    } else {
        uri = QString("%1://%3:%4%5").arg(scheme, host).arg(port).arg(path.startsWith('/') ? path : ("/" + path));
    }

    QProcess proc;
    if (!password.isEmpty()) {
        // Pass password via standard input or environment
        proc.start("gio", { "mount", uri });
        proc.write(password.toUtf8() + "\n");
    } else {
        proc.start("gio", { "mount", uri });
    }

    if (!proc.waitForFinished(6000) || proc.exitCode() != 0) {
        QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        if (error) *error = err.isEmpty() ? tr("Connection failed. Check host, credentials, and port.") : err;
        return false;
    }

    refresh();

    // Look for new mount
    for (const StorageDevice &net : m_networkMounts) {
        if (net.mountPath.contains(host)) {
            if (outMountPath) *outMountPath = net.mountPath;
            return true;
        }
    }

    return true;
}
