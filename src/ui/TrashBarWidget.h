#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>

class TrashBarWidget : public QWidget {
    Q_OBJECT

public:
    explicit TrashBarWidget(QWidget *parent = nullptr);

    void updateTrashState(int itemCount, int selectedCount);

signals:
    void restoreRequested();
    void deleteRequested();
    void emptyTrashRequested();

private:
    QLabel *m_iconLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_countLabel = nullptr;

    QPushButton *m_restoreBtn = nullptr;
    QPushButton *m_deleteBtn = nullptr;
    QPushButton *m_emptyBtn = nullptr;
};
