#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>

struct SwitcherItem {
    QString name;
    QString path;
    QString category; // "Place", "Tag", "File", "Folder"
    QString iconName;
    bool isDirectory;
};

class QuickSwitcherDialog : public QDialog {
    Q_OBJECT

public:
    explicit QuickSwitcherDialog(const QString &currentDirectory, QWidget *parent = nullptr);

    void setDirectory(const QString &dirPath);

signals:
    void pathSelected(const QString &path);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onSearchTextChanged(const QString &text);
    void onItemActivated(QListWidgetItem *item);

private:
    void setupUi();
    void indexPlaces();
    void indexCurrentDirectory();
    void filterItems(const QString &query);

    QString m_currentDir;
    QList<SwitcherItem> m_allItems;

    QLineEdit *m_searchEdit;
    QListWidget *m_resultsList;
    QLabel *m_statusLabel;
};
