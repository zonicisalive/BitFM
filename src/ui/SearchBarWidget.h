#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QToolButton>

class SearchBarWidget : public QWidget {
    Q_OBJECT

public:
    explicit SearchBarWidget(QWidget *parent = nullptr);

    void activate();
    void deactivate();
    bool isActive() const;

    void updateMatchCount(int matchCount, int totalCount);

    QSize sizeHint() const override { return QSize(300, 34); }
    QSize minimumSizeHint() const override { return QSize(80, 28); }

signals:
    void searchChanged(const QString &query, bool isRegex);
    void searchClosed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onTextChanged(const QString &text);
    void onRegexToggled(bool checked);
    void onCloseClicked();

private:
    QLineEdit *m_lineEdit;
    QToolButton *m_regexBtn;
    QLabel *m_matchCountLabel;
    QToolButton *m_closeBtn;
};
