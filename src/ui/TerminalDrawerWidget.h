#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QProcess>

class TerminalDrawerWidget : public QWidget {
    Q_OBJECT

public:
    explicit TerminalDrawerWidget(QWidget *parent = nullptr);
    ~TerminalDrawerWidget() override;

    void setDirectory(const QString &dirPath);
    QString currentDirectory() const;

signals:
    void closeRequested();
    void directoryChanged(const QString &newDir);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void executeCommand();
    void onProcessReadyRead();
    void onProcessFinished(int exitCode);
    void openInExternalTerminal();
    void clearConsole();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupUi();
    void appendHtmlOutput(const QString &html);
    void appendPlainTextOutput(const QString &text, const QString &color = QString());
    void updatePromptDisplay();
    void handleTabCompletion();

    QString m_currentDir;
    QProcess *m_process = nullptr;
    QStringList m_history;
    int m_historyIndex = -1;

    QTextEdit *m_console;
    QLineEdit *m_cmdInput;
    QLabel *m_promptLabel;
    QLabel *m_pathBadge;
};
