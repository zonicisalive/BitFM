#include "TerminalDrawerWidget.h"
#include <QPainter>
#include "ThemeManager.h"
#include "UserEnvironment.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QScrollBar>
#include <QRegularExpression>
#include <QApplication>
#include <QIcon>

static QString ansiToHtml(const QString &text) {
    QString html;
    html.reserve(text.size() * 2);

    static const QRegularExpression ansiRegex(R"(\x1B\[([0-9;]*)m)");

    int lastPos = 0;
    auto it = ansiRegex.globalMatch(text);
    bool inSpan = false;

    while (it.hasNext()) {
        auto match = it.next();
        QString plain = text.mid(lastPos, match.capturedStart() - lastPos);
        html += plain.toHtmlEscaped().replace("\n", "<br>");
        lastPos = match.capturedEnd();

        QString codes = match.captured(1);
        if (codes.isEmpty() || codes == "0") {
            if (inSpan) {
                html += "</span>";
                inSpan = false;
            }
        } else {
            QString color;
            QStringList parts = codes.split(';');
            for (const QString &p : parts) {
                int code = p.toInt();
                switch (code) {
                    case 30: color = "#45475a"; break; // Black
                    case 31: color = "#f38ba8"; break; // Red
                    case 32: color = "#a6e3a1"; break; // Green
                    case 33: color = "#f9e2af"; break; // Yellow
                    case 34: color = "#89b4fa"; break; // Blue
                    case 35: color = "#cba6f7"; break; // Magenta
                    case 36: color = "#94e2d5"; break; // Cyan
                    case 37: color = "#cdd6f4"; break; // White
                    case 90: color = "#585b70"; break; // Bright Black
                    case 91: color = "#f38ba8"; break; // Bright Red
                    case 92: color = "#a6e3a1"; break; // Bright Green
                    case 93: color = "#f9e2af"; break; // Bright Yellow
                    case 94: color = "#89b4fa"; break; // Bright Blue
                    case 95: color = "#cba6f7"; break; // Bright Magenta
                    case 96: color = "#94e2d5"; break; // Bright Cyan
                    case 97: color = "#ffffff"; break; // Bright White
                    default: break;
                }
            }
            if (!color.isEmpty()) {
                if (inSpan) html += "</span>";
                html += QString("<span style='color: %1; font-weight: 500;'>").arg(color);
                inSpan = true;
            }
        }
    }

    if (lastPos < text.size()) {
        html += text.mid(lastPos).toHtmlEscaped().replace("\n", "<br>");
    }
    if (inSpan) {
        html += "</span>";
    }

    return html;
}

TerminalDrawerWidget::TerminalDrawerWidget(QWidget *parent)
    : QWidget(parent), m_currentDir(UserEnvironment::realUserHome())
{
    setupUi();
    setDirectory(UserEnvironment::realUserHome());
}

TerminalDrawerWidget::~TerminalDrawerWidget() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(300);
    }
}

void TerminalDrawerWidget::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Modern Header Bar
    QWidget *header = new QWidget(this);
    header->setObjectName("TerminalHeader");
    header->setAttribute(Qt::WA_StyledBackground);
    header->setFixedHeight(ThemeManager::px(38));
    header->setStyleSheet(ThemeManager::css(QString(
        "#TerminalHeader { background: %1; border-top: 1px solid %2; border-bottom: 1px solid %2; }"
    ).arg(ThemeManager::BG_SURFACE, ThemeManager::BORDER)));

    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 0, 10, 0);
    headerLayout->setSpacing(8);

    QLabel *titleIcon = new QLabel("⚡", header);
    titleIcon->setStyleSheet(ThemeManager::css("font-size: 14px; background: transparent;"));
    headerLayout->addWidget(titleIcon);

    QLabel *titleLabel = new QLabel(tr("Terminal (F12)"), header);
    titleLabel->setStyleSheet(ThemeManager::css(QString("font-weight: 700; font-size: 12px; color: %1; background: transparent;").arg(ThemeManager::TEXT_PRIMARY)));
    headerLayout->addWidget(titleLabel);

    // Path pill badge
    m_pathBadge = new QLabel(header);
    m_pathBadge->setStyleSheet(ThemeManager::css(QString(
        "QLabel {"
        "  background: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 5px;"
        "  padding: 2px 8px;"
        "  font-family: monospace;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "}"
    ).arg(ThemeManager::BG_OVERLAY, ThemeManager::ACCENT, ThemeManager::BORDER)));
    headerLayout->addWidget(m_pathBadge);

    headerLayout->addStretch(1);

    QPushButton *clearBtn = new QPushButton(tr("Clear"), header);
    clearBtn->setCursor(Qt::PointingHandCursor);
    clearBtn->setStyleSheet(ThemeManager::css(QString(
        "QPushButton { background: transparent; color: %1; border: none; padding: 4px 10px; border-radius: 5px; font-size: 11px; font-weight: 600; }"
        "QPushButton:hover { background: %2; color: %3; }"
    ).arg(ThemeManager::TEXT_SECONDARY, ThemeManager::ACCENT_SOFT, ThemeManager::TEXT_PRIMARY)));
    connect(clearBtn, &QPushButton::clicked, this, &TerminalDrawerWidget::clearConsole);
    headerLayout->addWidget(clearBtn);

    QPushButton *extTermBtn = new QPushButton(tr("External Terminal"), header);
    extTermBtn->setToolTip(tr("Open directory in system terminal (Kitty / Foot / Alacritty)"));
    extTermBtn->setCursor(Qt::PointingHandCursor);
    extTermBtn->setStyleSheet(ThemeManager::css(QString(
        "QPushButton { background: transparent; color: %1; border: none; padding: 4px 10px; border-radius: 5px; font-size: 11px; font-weight: 600; }"
        "QPushButton:hover { background: %2; color: %3; }"
    ).arg(ThemeManager::TEXT_SECONDARY, ThemeManager::ACCENT_SOFT, ThemeManager::TEXT_PRIMARY)));
    connect(extTermBtn, &QPushButton::clicked, this, &TerminalDrawerWidget::openInExternalTerminal);
    headerLayout->addWidget(extTermBtn);

    QPushButton *closeBtn = new QPushButton("✕", header);
    closeBtn->setFixedSize(24, 24);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(ThemeManager::css(QString(
        "QPushButton { background: transparent; color: %1; border: none; border-radius: 12px /*fixed*/; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background: %2; color: %3; }"
    ).arg(ThemeManager::TEXT_SECONDARY, ThemeManager::ACCENT_SOFT, ThemeManager::TEXT_PRIMARY)));
    connect(closeBtn, &QPushButton::clicked, this, &TerminalDrawerWidget::closeRequested);
    headerLayout->addWidget(closeBtn);

    mainLayout->addWidget(header);

    // Console output text area
    m_console = new QTextEdit(this);
    m_console->setReadOnly(true);
    m_console->setFontFamily("monospace");
    m_console->setStyleSheet(ThemeManager::css(QString(
        "QTextEdit {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: none;"
        "  padding: 10px 14px;"
        "  font-family: 'JetBrains Mono', 'Fira Code', 'DejaVu Sans Mono', 'Monospace', monospace;"
        "  font-size: 12px;"
        "  line-height: 1.45;"
        "}"
    ).arg(ThemeManager::BG_BASE, ThemeManager::TEXT_PRIMARY)));
    mainLayout->addWidget(m_console, 1);

    // Command input pill bar at bottom
    QWidget *inputContainer = new QWidget(this);
    inputContainer->setFixedHeight(ThemeManager::px(42));
    inputContainer->setStyleSheet(ThemeManager::css(QString("background: %1; border-top: 1px solid %2;").arg(ThemeManager::BG_SURFACE, ThemeManager::BORDER)));

    QHBoxLayout *inputContainerLayout = new QHBoxLayout(inputContainer);
    inputContainerLayout->setContentsMargins(12, 5, 12, 5);

    QWidget *inputPill = new QWidget(inputContainer);
    inputPill->setStyleSheet(ThemeManager::css(QString(
        "background-color: %1;"
        "border: 1px solid %2;"
        "border-radius: 6px;"
    ).arg(ThemeManager::BG_BASE, ThemeManager::BORDER)));

    QHBoxLayout *inputLayout = new QHBoxLayout(inputPill);
    inputLayout->setContentsMargins(10, 0, 10, 0);
    inputLayout->setSpacing(8);

    m_promptLabel = new QLabel(inputPill);
    m_promptLabel->setStyleSheet(ThemeManager::css(QString(
        "color: %1; font-family: monospace; font-weight: bold; font-size: 12px; background: transparent; border: none;"
    ).arg(ThemeManager::ACCENT)));
    inputLayout->addWidget(m_promptLabel);

    m_cmdInput = new QLineEdit(inputPill);
    m_cmdInput->setStyleSheet(ThemeManager::css(
        "QLineEdit {"
        "  background: transparent;"
        "  color: #ffffff;"
        "  border: none;"
        "  font-family: 'JetBrains Mono', 'Fira Code', 'DejaVu Sans Mono', monospace;"
        "  font-size: 12px;"
        "  padding: 0;"
        "}"
    ));
    m_cmdInput->setPlaceholderText(tr("Type a command… (e.g. ls -la, git status, cargo build) · Tab to complete"));
    m_cmdInput->installEventFilter(this);
    connect(m_cmdInput, &QLineEdit::returnPressed, this, &TerminalDrawerWidget::executeCommand);
    inputLayout->addWidget(m_cmdInput, 1);

    inputContainerLayout->addWidget(inputPill);
    mainLayout->addWidget(inputContainer);

    auto updateStyles = [header, titleLabel, clearBtn, extTermBtn, inputContainer, inputPill, this]() {
        update();
        header->setStyleSheet(ThemeManager::css(QString(
            "#TerminalHeader { background: transparent; border-bottom: 1px solid %1; }"
        ).arg(ThemeManager::BORDER)));

        titleLabel->setStyleSheet(ThemeManager::css(QString("font-weight: 700; font-size: 12px; color: %1; background: transparent;").arg(ThemeManager::TEXT_PRIMARY)));

        m_pathBadge->setStyleSheet(ThemeManager::css(QString(
            "QLabel {"
            "  background: %1;"
            "  color: %2;"
            "  border: 1px solid %3;"
            "  border-radius: 5px;"
            "  padding: 2px 8px;"
            "  font-family: monospace;"
            "  font-size: 11px;"
            "  font-weight: 600;"
            "}"
        ).arg(ThemeManager::BG_OVERLAY, ThemeManager::ACCENT, ThemeManager::BORDER)));

        clearBtn->setStyleSheet(ThemeManager::css(QString(
            "QPushButton { background: transparent; color: %1; border: none; padding: 4px 10px; border-radius: 5px; font-size: 11px; font-weight: 600; }"
            "QPushButton:hover { background: %2; color: %3; }"
        ).arg(ThemeManager::TEXT_SECONDARY, ThemeManager::ACCENT_SOFT, ThemeManager::TEXT_PRIMARY)));

        extTermBtn->setStyleSheet(ThemeManager::css(QString(
            "QPushButton { background: transparent; color: %1; border: none; padding: 4px 10px; border-radius: 5px; font-size: 11px; font-weight: 600; }"
            "QPushButton:hover { background: %2; color: %3; }"
        ).arg(ThemeManager::TEXT_SECONDARY, ThemeManager::ACCENT_SOFT, ThemeManager::TEXT_PRIMARY)));

        m_console->setStyleSheet(ThemeManager::css(QString(
            "QTextEdit {"
            "  background-color: %1;"
            "  color: %2;"
            "  border: none;"
            "  padding: 10px 14px;"
            "  font-family: 'JetBrains Mono', 'Fira Code', 'DejaVu Sans Mono', 'Monospace', monospace;"
            "  font-size: 12px;"
            "  line-height: 1.45;"
            "}"
        ).arg(ThemeManager::BG_BASE, ThemeManager::TEXT_PRIMARY)));

        inputContainer->setStyleSheet(ThemeManager::css(QString("background: transparent; border-top: 1px solid %1;").arg(ThemeManager::BORDER)));

        inputPill->setStyleSheet(ThemeManager::css(QString(
            "background-color: %1;"
            "border: 1px solid %2;"
            "border-radius: 6px;"
        ).arg(ThemeManager::BG_BASE, ThemeManager::BORDER)));

        m_promptLabel->setStyleSheet(ThemeManager::css(QString(
            "color: %1; font-family: monospace; font-weight: bold; font-size: 12px; background: transparent; border: none;"
        ).arg(ThemeManager::ACCENT)));

        m_cmdInput->setStyleSheet(ThemeManager::css(QString(
            "QLineEdit {"
            "  background: transparent;"
            "  color: %1;"
            "  border: none;"
            "  font-family: 'JetBrains Mono', 'Fira Code', 'DejaVu Sans Mono', monospace;"
            "  font-size: 12px;"
            "  padding: 0;"
            "}"
        ).arg(ThemeManager::TEXT_PRIMARY)));
    };

    updateStyles();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, updateStyles);

    appendHtmlOutput(QString(
        "<span style='color: %1; font-weight: bold;'>⚡ BitFM Integrated Shell</span> "
        "<span style='color: %2;'>· Ready in %3</span><br/>"
    ).arg(ThemeManager::ACCENT, ThemeManager::TEXT_MUTED, m_currentDir));
}

void TerminalDrawerWidget::setDirectory(const QString &dirPath) {
    QString clean = QDir::cleanPath(dirPath);
    if (!clean.isEmpty() && QDir(clean).exists() && m_currentDir != clean) {
        m_currentDir = clean;
        updatePromptDisplay();
    }
}

QString TerminalDrawerWidget::currentDirectory() const {
    return m_currentDir;
}

void TerminalDrawerWidget::updatePromptDisplay() {
    QString shortDir = m_currentDir;
    QString home = UserEnvironment::realUserHome();
    if (shortDir == home) {
        shortDir = "~";
    } else if (shortDir.startsWith(home + "/")) {
        shortDir.replace(0, home.length(), "~");
    }

    m_pathBadge->setText(QString("📂 %1").arg(shortDir));
    m_promptLabel->setText(QString("❯"));
}

void TerminalDrawerWidget::clearConsole() {
    m_console->clear();
}

void TerminalDrawerWidget::appendHtmlOutput(const QString &html) {
    m_console->moveCursor(QTextCursor::End);
    m_console->insertHtml(html);
    m_console->moveCursor(QTextCursor::End);
    m_console->verticalScrollBar()->setValue(m_console->verticalScrollBar()->maximum());
}

void TerminalDrawerWidget::appendPlainTextOutput(const QString &text, const QString &color) {
    if (!color.isEmpty()) {
        appendHtmlOutput(QString("<span style='color: %1;'>%2</span>").arg(color, text.toHtmlEscaped().replace("\n", "<br>")));
    } else {
        appendHtmlOutput(ansiToHtml(text));
    }
}

void TerminalDrawerWidget::executeCommand() {
    QString cmd = m_cmdInput->text().trimmed();
    if (cmd.isEmpty()) return;

    m_history.append(cmd);
    m_historyIndex = m_history.size();
    m_cmdInput->clear();

    QString user = UserEnvironment::realUserName();
    QString shortDir = m_currentDir;
    QString home = UserEnvironment::realUserHome();
    if (shortDir == home) {
        shortDir = "~";
    } else if (shortDir.startsWith(home + "/")) {
        shortDir.replace(0, home.length(), "~");
    }

    // Format rich prompt badge in console
    QString promptHtml = QString(
        "<div style='margin-top: 8px;'>"
        "<span style='color: #89b4fa; font-weight: bold;'>%1</span>"
        "<span style='color: #6c7086;'>@</span>"
        "<span style='color: #a6e3a1; font-weight: bold;'>bitfm</span> "
        "<span style='color: #f9e2af;'>%2</span> "
        "<span style='color: #cba6f7; font-weight: bold;'>❯</span> "
        "<span style='color: #ffffff; font-weight: 600;'>%3</span>"
        "</div>"
    ).arg(user, shortDir, cmd.toHtmlEscaped());

    appendHtmlOutput(promptHtml);

    // Built-in cd command
    if (cmd == "cd" || cmd.startsWith("cd ")) {
        QString targetDir = cmd.mid(2).trimmed();
        if (targetDir.isEmpty() || targetDir == "~") {
            m_currentDir = home;
        } else {
            if (targetDir.startsWith("~")) {
                targetDir.replace(0, 1, home);
            }
            QDir d(m_currentDir);
            if (d.cd(targetDir)) {
                m_currentDir = d.absolutePath();
            } else {
                appendHtmlOutput(QString("<span style='color: #f38ba8;'>cd: no such file or directory: %1</span><br/>").arg(targetDir.toHtmlEscaped()));
                return;
            }
        }
        updatePromptDisplay();
        emit directoryChanged(m_currentDir);
        return;
    } else if (cmd == "clear") {
        clearConsole();
        return;
    } else if (cmd == "exit") {
        emit closeRequested();
        return;
    }

    if (m_process) {
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            m_process->waitForFinished(300);
        }
        delete m_process;
    }

    m_process = new QProcess(this);
    m_process->setWorkingDirectory(m_currentDir);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("CLICOLOR", "1");
    env.insert("CLICOLOR_FORCE", "1");
    env.insert("TERM", "xterm-256color");
    m_process->setProcessEnvironment(env);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &TerminalDrawerWidget::onProcessReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, &TerminalDrawerWidget::onProcessReadyRead);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) { onProcessFinished(code); });

    // Run command inside bash with color aliases
    m_process->start("/bin/bash", { "-c", cmd });
}

void TerminalDrawerWidget::onProcessReadyRead() {
    if (!m_process) return;
    QByteArray out = m_process->readAllStandardOutput();
    if (!out.isEmpty()) appendPlainTextOutput(QString::fromUtf8(out));

    QByteArray err = m_process->readAllStandardError();
    if (!err.isEmpty()) appendPlainTextOutput(QString::fromUtf8(err));
}

void TerminalDrawerWidget::onProcessFinished(int exitCode) {
    Q_UNUSED(exitCode);
    appendHtmlOutput("<br/>");
}

void TerminalDrawerWidget::openInExternalTerminal() {
    QStringList terms = { "foot", "kitty", "ptyxis", "alacritty", "gnome-terminal", "konsole", "xterm" };
    for (const QString &t : terms) {
        if (QProcess::startDetached(t, {}, m_currentDir)) return;
    }
}

void TerminalDrawerWidget::handleTabCompletion() {
    QString text = m_cmdInput->text();
    int cursor = m_cmdInput->cursorPosition();
    QString before = text.left(cursor);
    int lastSpace = before.lastIndexOf(' ');
    QString word = (lastSpace == -1) ? before : before.mid(lastSpace + 1);

    if (word.isEmpty()) return;

    QDir dir(m_currentDir);
    QString prefix = word;
    QString subDir;
    if (word.contains('/')) {
        int slash = word.lastIndexOf('/');
        subDir = word.left(slash + 1);
        prefix = word.mid(slash + 1);
        dir.cd(subDir);
    }

    QStringList matches = dir.entryList(QStringList() << prefix + "*", QDir::AllEntries | QDir::NoDotAndDotDot);
    if (matches.size() == 1) {
        QString completion = subDir + matches.first();
        if (QFileInfo(dir.absoluteFilePath(matches.first())).isDir()) completion += "/";
        QString newText = text.left(lastSpace + 1) + completion + text.mid(cursor);
        m_cmdInput->setText(newText);
        m_cmdInput->setCursorPosition(lastSpace + 1 + completion.length());
    } else if (matches.size() > 1) {
        appendHtmlOutput(QString("<span style='color: #a6adc8;'>%1</span><br/>").arg(matches.join("   ").toHtmlEscaped()));
    }
}

bool TerminalDrawerWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_cmdInput && event->type() == QEvent::KeyPress) {
        QKeyEvent *k = static_cast<QKeyEvent*>(event);
        if (k->key() == Qt::Key_Up) {
            if (!m_history.isEmpty()) {
                m_historyIndex = qBound(0, m_historyIndex - 1, m_history.size() - 1);
                m_cmdInput->setText(m_history[m_historyIndex]);
            }
            return true;
        } else if (k->key() == Qt::Key_Down) {
            if (!m_history.isEmpty()) {
                m_historyIndex = qBound(0, m_historyIndex + 1, m_history.size());
                if (m_historyIndex < m_history.size()) {
                    m_cmdInput->setText(m_history[m_historyIndex]);
                } else {
                    m_cmdInput->clear();
                }
            }
            return true;
        } else if (k->key() == Qt::Key_Tab) {
            handleTabCompletion();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TerminalDrawerWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    ThemeManager::paintCard(p, rect());
}
