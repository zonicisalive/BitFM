#pragma once

#include <QObject>
#include <QString>
#include <QColor>
#include <QHash>
#include <QStringList>
#include <QList>

struct TagInfo {
    QString name;
    QString displayName;
    QColor color;
};

class TagManager : public QObject {
    Q_OBJECT

public:
    static TagManager& instance();

    static const QList<TagInfo>& availableTags();
    static TagInfo tagByName(const QString &name);

    void setTag(const QString &filePath, const QString &tagName);
    void removeTag(const QString &filePath);
    QString getTag(const QString &filePath) const;
    QColor getTagColor(const QString &filePath) const;
    QStringList getFilesForTag(const QString &tagName) const;

signals:
    void tagsChanged();

private:
    explicit TagManager(QObject *parent = nullptr);
    ~TagManager() override = default;

    void loadFromSettings();
    void saveToSettings();

    QHash<QString, QString> m_fileTags; // filePath -> tagName
};
