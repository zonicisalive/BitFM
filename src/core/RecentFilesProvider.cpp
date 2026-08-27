#include "RecentFilesProvider.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QXmlStreamReader>
#include <algorithm>

RecentFilesProvider& RecentFilesProvider::instance() {
    static RecentFilesProvider inst;
    return inst;
}

RecentFilesProvider::RecentFilesProvider(QObject *parent)
    : QObject(parent)
{
    reload();
}

void RecentFilesProvider::reload() {
    m_items.clear();
    parseXbel();
    emit recentFilesChanged();
}

#include "UserEnvironment.h"

void RecentFilesProvider::parseXbel() {
    QString xbelPath = UserEnvironment::userRecentXbelPath();
    QFile file(xbelPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QXmlStreamReader xml(&file);
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name() == QLatin1String("bookmark")) {
                QString href = xml.attributes().value("href").toString();
                QString visitedStr = xml.attributes().value("visited").toString();
                if (visitedStr.isEmpty()) visitedStr = xml.attributes().value("modified").toString();

                if (!href.isEmpty()) {
                    QString localPath = QUrl(href).toLocalFile();
                    if (localPath.isEmpty() && href.startsWith("file://")) {
                        localPath = href.mid(7);
                    }

                    if (!localPath.isEmpty()) {
                        QFileInfo fi(localPath);
                        if (fi.exists()) {
                            RecentItem item;
                            item.path = fi.absoluteFilePath();
                            item.visitedTime = QDateTime::fromString(visitedStr, Qt::ISODate);
                            if (!item.visitedTime.isValid()) item.visitedTime = fi.lastModified();

                            // Avoid duplicates
                            bool found = false;
                            for (auto &existing : m_items) {
                                if (existing.path == item.path) {
                                    if (item.visitedTime > existing.visitedTime) {
                                        existing.visitedTime = item.visitedTime;
                                    }
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                m_items.append(item);
                            }
                        }
                    }
                }
            }
        }
    }

    // Sort by visitedTime descending
    std::sort(m_items.begin(), m_items.end(), [](const RecentItem &a, const RecentItem &b) {
        return a.visitedTime > b.visitedTime;
    });
}

QStringList RecentFilesProvider::recentFilePaths(int maxCount) const {
    QStringList paths;
    for (int i = 0; i < m_items.size() && i < maxCount; ++i) {
        paths.append(m_items[i].path);
    }
    return paths;
}

QList<RecentItem> RecentFilesProvider::recentItems(int maxCount) const {
    QList<RecentItem> res;
    for (int i = 0; i < m_items.size() && i < maxCount; ++i) {
        res.append(m_items[i]);
    }
    return res;
}

void RecentFilesProvider::addRecentFile(const QString &path) {
    if (path.isEmpty()) return;
    QFileInfo fi(path);
    if (!fi.exists()) return;

    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].path == fi.absoluteFilePath()) {
            m_items[i].visitedTime = QDateTime::currentDateTime();
            std::sort(m_items.begin(), m_items.end(), [](const RecentItem &a, const RecentItem &b) {
                return a.visitedTime > b.visitedTime;
            });
            emit recentFilesChanged();
            return;
        }
    }

    RecentItem item;
    item.path = fi.absoluteFilePath();
    item.visitedTime = QDateTime::currentDateTime();
    m_items.prepend(item);
    emit recentFilesChanged();
}
