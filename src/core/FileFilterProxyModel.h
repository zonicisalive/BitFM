#pragma once

#include <QSortFilterProxyModel>
#include <QRegularExpression>

class FileFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit FileFilterProxyModel(QObject *parent = nullptr);

    void setSearchPattern(const QString &pattern, bool isRegex = false);
    QString searchPattern() const;
    bool isRegex() const;

    void setKeepFoldersVisible(bool keep);
    bool keepFoldersVisible() const;

    void setDirectoriesOnly(bool dirsOnly);
    bool directoriesOnly() const;

    int matchCount() const;

signals:
    void filterChanged(int matchingCount, int totalCount);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;

private:
    QString m_searchPattern;
    bool m_isRegex = false;
    bool m_keepFoldersVisible = false;
    bool m_directoriesOnly = false;
    QRegularExpression m_regex;
};
