#include "TagManager.h"
#include <QSettings>
#include <QFileInfo>

TagManager& TagManager::instance() {
    static TagManager inst;
    return inst;
}

const QList<TagInfo>& TagManager::availableTags() {
    static const QList<TagInfo> tags = {
        { "red",    "Red",    QColor("#f38ba8") },
        { "orange", "Orange", QColor("#fab387") },
        { "yellow", "Yellow", QColor("#f9e2af") },
        { "green",  "Green",  QColor("#a6e3a1") },
        { "blue",   "Blue",   QColor("#89b4fa") },
        { "purple", "Purple", QColor("#cba6f7") }
    };
    return tags;
}

TagInfo TagManager::tagByName(const QString &name) {
    for (const TagInfo &t : availableTags()) {
        if (t.name.compare(name, Qt::CaseInsensitive) == 0) {
            return t;
        }
    }
    return { "", "", QColor() };
}

TagManager::TagManager(QObject *parent)
    : QObject(parent)
{
    loadFromSettings();
}

void TagManager::loadFromSettings() {
    QSettings settings;
    int size = settings.beginReadArray("file_tags");
    m_fileTags.clear();
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QString path = settings.value("path").toString();
        QString tag = settings.value("tag").toString();
        if (!path.isEmpty() && !tag.isEmpty()) {
            m_fileTags.insert(path, tag);
        }
    }
    settings.endArray();
}

void TagManager::saveToSettings() {
    QSettings settings;
    settings.beginWriteArray("file_tags", m_fileTags.size());
    int i = 0;
    for (auto it = m_fileTags.begin(); it != m_fileTags.end(); ++it, ++i) {
        settings.setArrayIndex(i);
        settings.setValue("path", it.key());
        settings.setValue("tag", it.value());
    }
    settings.endArray();
}

void TagManager::setTag(const QString &filePath, const QString &tagName) {
    if (filePath.isEmpty()) return;
    if (tagName.isEmpty()) {
        removeTag(filePath);
        return;
    }

    m_fileTags.insert(filePath, tagName.toLower());
    saveToSettings();
    emit tagsChanged();
}

void TagManager::removeTag(const QString &filePath) {
    if (m_fileTags.remove(filePath) > 0) {
        saveToSettings();
        emit tagsChanged();
    }
}

QString TagManager::getTag(const QString &filePath) const {
    return m_fileTags.value(filePath);
}

QColor TagManager::getTagColor(const QString &filePath) const {
    QString tagName = getTag(filePath);
    if (tagName.isEmpty()) return QColor();
    return tagByName(tagName).color;
}

QStringList TagManager::getFilesForTag(const QString &tagName) const {
    QStringList files;
    QString lower = tagName.toLower();
    for (auto it = m_fileTags.begin(); it != m_fileTags.end(); ++it) {
        if (it.value() == lower && QFileInfo::exists(it.key())) {
            files.append(it.key());
        }
    }
    return files;
}
