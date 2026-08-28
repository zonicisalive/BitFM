#pragma once

#include <QObject>
#include <QDBusAbstractAdaptor>
#include <QStringList>

class MainWindow;

class FileManager1Adaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.FileManager1")

public:
    explicit FileManager1Adaptor(QObject *parent);

public slots:
    void ShowItems(const QStringList &URIs, const QString &startup_id);
    void ShowFolders(const QStringList &URIs, const QString &startup_id);
    void ShowItemProperties(const QStringList &URIs, const QString &startup_id);

signals:
    void showItemsRequested(const QStringList &uris, const QString &startupId);
    void showFoldersRequested(const QStringList &uris, const QString &startupId);
    void showItemPropertiesRequested(const QStringList &uris, const QString &startupId);
};

class FileManager1Service : public QObject {
    Q_OBJECT

public:
    explicit FileManager1Service(MainWindow *window, QObject *parent = nullptr);
    bool registerService();

private slots:
    void onShowItems(const QStringList &uris, const QString &startupId);
    void onShowFolders(const QStringList &uris, const QString &startupId);
    void onShowItemProperties(const QStringList &uris, const QString &startupId);

private:
    MainWindow *m_window;
    FileManager1Adaptor *m_adaptor;
};
