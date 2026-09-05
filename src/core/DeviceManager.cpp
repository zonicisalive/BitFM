#include "DeviceManager.h"
#include <QTemporaryFile>
#include "UnlockDeviceDialog.h"
#include "UserEnvironment.h"
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStorageInfo>
#include <QDir>
#include <QFileInfo>
#include <QThreadPool>
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

    // Watch /media and /run/media for kernel udev mount events
    QString user = UserEnvironment::realUserName();
    QString mediaUser = QString("/run/media/%1").arg(user);
    QString gvfsPath = QString("/run/user/%1/gvfs").arg(getuid());

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
    if (m_isScanning) return;
    m_isScanning = true;

    uid_t uid = getuid();

    QThreadPool::globalInstance()->start([this, uid]() {
        QList<StorageDevice> newDevices;
        QList<StorageDevice> newNetwork;

        // 1. Scan GVFS Mounts (Android MTP, SFTP, SMB, WebDAV, Cameras)
        QString gvfsDir = QString("/run/user/%1/gvfs").arg(uid);
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
        lsblkProc.start("lsblk", { "-J", "-o", "NAME,SIZE,TYPE,MOUNTPOINT,LABEL,RM,MODEL,FSTYPE,PARTTYPE,PARTLABEL" });
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
                QString fstype = obj["fstype"].toString().toLower();
                QString partType = obj["parttype"].toString().toLower();
                QString partLabel = obj["partlabel"].toString().toLower();

                bool hasChildren = obj.contains("children") && !obj["children"].toArray().isEmpty();

                bool isSystemPart = (partType == "c12a7328-f81f-11d2-ba4b-00a0c93ec93b" || // EFI System Partition
                                     partType == "de94bba4-06d1-4d40-a16a-bfd50179d6ac" || // Windows Recovery
                                     partType == "e3c9e316-0b5c-4db8-817d-f92df00215ae" || // Microsoft Reserved
                                     partType == "4f68bce3-e8cd-4db1-96e7-fbcaf984b709" || // Linux Swap
                                     partType == "21686148-6449-6e6f-744e-656564454649" || // BIOS Boot
                                     partLabel.contains("microsoft reserved") ||
                                     partLabel.contains("recovery") ||
                                     partLabel.contains("efi system") ||
                                     partLabel.contains("bios boot") ||
                                     mountpoint == "[SWAP]" ||
                                     fstype == "swap" ||
                                     mountpoint.startsWith("/boot"));

                if (isSystemPart) {
                    // Skip system / boot / recovery / swap partitions
                } else if (type == "part" || type == "lvm" || type == "crypt" || type.startsWith("raid") ||
                           (type == "disk" && !hasChildren && !fstype.isEmpty())) {
                    // Containers (LUKS/LVM/RAID members) show up through their unlocked/child volume instead
                    if (hasChildren && (fstype == "crypto_luks" || fstype == "lvm2_member" || fstype == "linux_raid_member")) {
                        // fallthrough to children below
                    } else
                    if (!fstype.isEmpty() || !mountpoint.isEmpty() || !label.isEmpty()) {
                        StorageDevice dev;
                        dev.deviceNode = "/dev/" + name;
                        dev.id = dev.deviceNode;
                        dev.mountPath = mountpoint;
                        dev.isMounted = !mountpoint.isEmpty();
                        dev.isRemovable = isRemovable;

                        if (mountpoint == "/") {
                            dev.name = tr("File System");
                            dev.iconName = "drive-harddisk-root";
                        } else if (!label.isEmpty()) {
                            dev.name = label;
                            dev.iconName = isRemovable ? "media-flash" : "drive-harddisk";
                        } else if (!size.isEmpty()) {
                            dev.name = QString("%1 Volume").arg(size);
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

        QMetaObject::invokeMethod(this, [this, newDevices = std::move(newDevices), newNetwork = std::move(newNetwork)]() mutable {
            m_isScanning = false;
            bool changed = (m_devices != newDevices || m_networkMounts != newNetwork);
            if (changed) {
                m_devices = std::move(newDevices);
                m_networkMounts = std::move(newNetwork);
                emit devicesChanged();
            }
        });
    });
}

bool DeviceManager::mountDevice(const QString &deviceNode, QString *outMountPath, QString *error, QWidget *parentWidget) {
    if (deviceNode.isEmpty()) return false;

    // 1. First attempt: standard udisksctl mount (non-interactive)
    QProcess proc;
    proc.start("udisksctl", { "mount", "-b", deviceNode, "--no-user-interaction" });
    if (proc.waitForFinished(3000) && proc.exitCode() == 0) {
        QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (out.contains(" at ")) {
            QString path = out.section(" at ", 1, 1).trimmed();
            if (path.endsWith('.')) path.chop(1);
            if (outMountPath) *outMountPath = path;
        }
        refresh();
        return true;
    }

    QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();

    // 2. Check if device is encrypted (LUKS / BitLocker) or requires privileges
    bool isEncrypted = err.contains("encrypted", Qt::CaseInsensitive)
                    || err.contains("unlock", Qt::CaseInsensitive)
                    || err.contains("crypto", Qt::CaseInsensitive)
                    || err.contains("locked", Qt::CaseInsensitive);

    bool needsAuth = isEncrypted
                  || err.contains("NotAuthorized", Qt::CaseInsensitive)
                  || err.contains("not authorized", Qt::CaseInsensitive)
                  || err.contains("permission", Qt::CaseInsensitive)
                  || err.contains("polkit", Qt::CaseInsensitive);
    if (!needsAuth) {
        if (error) *error = err.isEmpty() ? tr("Failed to mount %1.").arg(deviceNode) : err;
        return false;
    }

    // Find device display name
    QString devName;
    for (const StorageDevice &d : m_devices) {
        if (d.deviceNode == deviceNode) {
            devName = d.name;
            break;
        }
    }
    if (devName.isEmpty()) devName = QFileInfo(deviceNode).fileName();

    // Prompt user for password or passphrase
    UnlockDeviceDialog dlg(deviceNode, devName, isEncrypted, parentWidget);

    while (true) {
        if (dlg.exec() != QDialog::Accepted) {
            if (error) *error = tr("Operation cancelled by user.");
            return false;
        }

        QString pass = dlg.password();
        if (pass.isEmpty()) {
            dlg.setError(isEncrypted ? tr("Passphrase cannot be empty.") : tr("Password cannot be empty."));
            continue;
        }

        dlg.setBusy(true);

        if (isEncrypted) {
            // Unlock with udisksctl unlock
            QTemporaryFile keyFile(QDir(qEnvironmentVariable("XDG_RUNTIME_DIR", QDir::tempPath())).filePath("bitfm-key-XXXXXX"));
            if (!keyFile.open()) {
                dlg.setError(tr("Cannot create temporary key file."));
                continue;
            }
            keyFile.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
            keyFile.write(pass.toUtf8());
            keyFile.flush();

            QProcess unlockProc;
            unlockProc.start("udisksctl", { "unlock", "-b", deviceNode, "--key-file", keyFile.fileName(), "--no-user-interaction" });
            bool unlocked = unlockProc.waitForFinished(15000) && unlockProc.exitCode() == 0;
            keyFile.remove();

            if (!unlocked) {
                QString uerr = QString::fromUtf8(unlockProc.readAllStandardError()).trimmed();
                dlg.setError(uerr.contains("Incorrect", Qt::CaseInsensitive) || uerr.contains("passphrase", Qt::CaseInsensitive)
                             ? tr("Incorrect passphrase. Please try again.")
                             : (uerr.isEmpty() ? tr("Unlock failed.") : uerr));
                continue;
            }

            QString unlockOut = QString::fromUtf8(unlockProc.readAllStandardOutput()).trimmed();
            // Expected format: "Unlocked /dev/sdX as /dev/dm-Z."
            QString clearDevice;
            if (unlockOut.contains(" as ")) {
                clearDevice = unlockOut.section(" as ", 1, 1).trimmed();
                if (clearDevice.endsWith('.')) clearDevice.chop(1);
            }

            if (clearDevice.isEmpty()) {
                clearDevice = deviceNode;
            }

            // Mount the unlocked cleartext device
            QProcess mountProc;
            mountProc.start("udisksctl", { "mount", "-b", clearDevice });
            if (mountProc.waitForFinished(5000) && mountProc.exitCode() == 0) {
                QString out = QString::fromUtf8(mountProc.readAllStandardOutput()).trimmed();
                if (out.contains(" at ")) {
                    QString path = out.section(" at ", 1, 1).trimmed();
                    if (path.endsWith('.')) path.chop(1);
                    if (outMountPath) *outMountPath = path;
                }
                refresh();
                return true;
            } else {
                dlg.setError(tr("Device unlocked, but mounting failed."));
                continue;
            }
        } else {
            // Privileged mount via sudo -S
            QString userName = UserEnvironment::realUserName();
            if (userName.isEmpty()) userName = qgetenv("USER");
            if (userName.isEmpty()) userName = "user";

            QString cleanLabel = QFileInfo(deviceNode).fileName();
            QString targetDir = QString("/media/%1/%2").arg(userName, cleanLabel);

            QProcess sudoProc;
            QString script = QString("mkdir -p '%1' && mount -o user,rw '%2' '%1'").arg(targetDir, deviceNode);
            sudoProc.start("sudo", { "-S", "sh", "-c", script });
            sudoProc.write(pass.toUtf8() + "\n");
            sudoProc.closeWriteChannel();

            if (sudoProc.waitForFinished(6000) && sudoProc.exitCode() == 0) {
                if (outMountPath) *outMountPath = targetDir;
                refresh();
                return true;
            } else {
                QString sudoErr = QString::fromUtf8(sudoProc.readAllStandardError()).trimmed();
                if (sudoErr.contains("incorrect password", Qt::CaseInsensitive) || sudoErr.contains("try again", Qt::CaseInsensitive)) {
                    dlg.setError(tr("Incorrect password. Please try again."));
                } else {
                    dlg.setError(sudoErr.isEmpty() ? tr("Authentication failed.") : sudoErr);
                }
                continue;
            }
        }
    }

    return false;
}

bool DeviceManager::unmountDevice(const QString &mountPath, QString *error) {
    if (mountPath.isEmpty()) return false;

    QProcess proc;
    if (mountPath.contains("/gvfs/")) {
        proc.start("gio", { "mount", "-u", mountPath });
    } else {
        QString node;
        for (const StorageDevice &d : m_devices) {
            if (d.mountPath == mountPath || d.deviceNode == mountPath) { node = d.deviceNode; break; }
        }
        if (node.isEmpty()) node = mountPath.startsWith("/dev/") ? mountPath : QStorageInfo(mountPath).device();
        proc.start("udisksctl", { "unmount", "-b", node, "--no-user-interaction" });
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
    proc.start("gio", { "mount", uri });
    if (!password.isEmpty()) {
        // gio prompts in sequence (anonymous? / user / domain / password); answer every one it may ask
        QByteArray answers;
        if (scheme == "smb") answers += "n\n";
        if (user.isEmpty()) answers += "\n";
        if (scheme == "smb") answers += "WORKGROUP\n";
        answers += password.toUtf8() + "\n";
        proc.write(answers);
    }
    proc.closeWriteChannel();

    if (!proc.waitForFinished(20000) || proc.exitCode() != 0) {
        QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        if (error) *error = err.isEmpty() ? tr("Connection failed. Check host, credentials, and port.") : err;
        return false;
    }

    refresh();

    // refresh() runs asynchronously; resolve the gvfs mount dir directly
    if (outMountPath) {
        QString gvfsRoot = QString("/run/user/%1/gvfs").arg(getuid());
        for (const QString &entry : QDir(gvfsRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (entry.contains("host=" + host, Qt::CaseInsensitive)) {
                *outMountPath = gvfsRoot + "/" + entry;
                break;
            }
        }
    }

    return true;
}
