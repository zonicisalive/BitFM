#include "BatchRenameDialog.h"
#include <QDateTime>
#include <QDir>
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileInfo>
#include <QRegularExpression>
#include <QMessageBox>
#include <QSet>

BatchRenameDialog::BatchRenameDialog(const QStringList &filePaths, QWidget *parent)
    : QDialog(parent), m_originalPaths(filePaths)
{
    setWindowTitle(tr("Batch Rename — %1 items").arg(filePaths.size()));
    resize(700, 520);
    setupUi();
    updatePreview();
}

void BatchRenameDialog::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // 1. Controls Tabs
    m_modeTabs = new QTabWidget(this);
    m_modeTabs->setFixedHeight(140);
    m_modeTabs->setStyleSheet(ThemeManager::css(QString(
        "QTabWidget::pane { border: 1px solid %1; border-radius: 8px; background: %2; }"
        "QTabBar::tab { background: transparent; padding: 6px 14px; color: %3; border: none; font-size: 12px; }"
        "QTabBar::tab:selected { color: %4; border-bottom: 2px solid %4; font-weight: 600; }"
    ).arg(ThemeManager::BORDER).arg(ThemeManager::BG_SURFACE).arg(ThemeManager::TEXT_SECONDARY).arg(ThemeManager::ACCENT)));

    // Tab 1: Find & Replace
    QWidget *findTab = new QWidget();
    QVBoxLayout *findLayout = new QVBoxLayout(findTab);
    QHBoxLayout *findRow1 = new QHBoxLayout();
    findRow1->addWidget(new QLabel(tr("Find:"), findTab));
    m_findEdit = new QLineEdit(findTab);
    findRow1->addWidget(m_findEdit);
    findRow1->addWidget(new QLabel(tr("Replace:"), findTab));
    m_replaceEdit = new QLineEdit(findTab);
    findRow1->addWidget(m_replaceEdit);
    findLayout->addLayout(findRow1);

    QHBoxLayout *findRow2 = new QHBoxLayout();
    m_useRegexCheck = new QCheckBox(tr("Use Regular Expressions"), findTab);
    m_caseSensitiveCheck = new QCheckBox(tr("Case Sensitive"), findTab);
    findRow2->addWidget(m_useRegexCheck);
    findRow2->addWidget(m_caseSensitiveCheck);
    findRow2->addStretch();
    findLayout->addLayout(findRow2);
    m_modeTabs->addTab(findTab, tr("Find & Replace"));

    // Tab 2: Add Prefix / Suffix
    QWidget *affixTab = new QWidget();
    QVBoxLayout *affixLayout = new QVBoxLayout(affixTab);
    QHBoxLayout *affixRow = new QHBoxLayout();
    affixRow->addWidget(new QLabel(tr("Prefix:"), affixTab));
    m_prefixEdit = new QLineEdit(affixTab);
    affixRow->addWidget(m_prefixEdit);
    affixRow->addWidget(new QLabel(tr("Suffix:"), affixTab));
    m_suffixEdit = new QLineEdit(affixTab);
    affixRow->addWidget(m_suffixEdit);
    affixLayout->addLayout(affixRow);
    affixLayout->addStretch();
    m_modeTabs->addTab(affixTab, tr("Add Prefix / Suffix"));

    // Tab 3: Numbering
    QWidget *numTab = new QWidget();
    QVBoxLayout *numLayout = new QVBoxLayout(numTab);
    QHBoxLayout *numRow = new QHBoxLayout();
    numRow->addWidget(new QLabel(tr("Base Name:"), numTab));
    m_numBaseEdit = new QLineEdit("item", numTab);
    numRow->addWidget(m_numBaseEdit);
    numRow->addWidget(new QLabel(tr("Start:"), numTab));
    m_startNumSpin = new QSpinBox(numTab);
    m_startNumSpin->setRange(1, 99999);
    m_startNumSpin->setValue(1);
    numRow->addWidget(m_startNumSpin);
    numRow->addWidget(new QLabel(tr("Digits:"), numTab));
    m_digitsSpin = new QSpinBox(numTab);
    m_digitsSpin->setRange(1, 8);
    m_digitsSpin->setValue(3);
    numRow->addWidget(m_digitsSpin);
    numLayout->addLayout(numRow);

    m_keepExtCheck = new QCheckBox(tr("Preserve original file extension"), numTab);
    m_keepExtCheck->setChecked(true);
    numLayout->addWidget(m_keepExtCheck);
    m_modeTabs->addTab(numTab, tr("Numbering Sequence"));

    // Tab 4: Change Case
    QWidget *caseTab = new QWidget();
    QVBoxLayout *caseLayout = new QVBoxLayout(caseTab);
    QHBoxLayout *caseRow = new QHBoxLayout();
    caseRow->addWidget(new QLabel(tr("Format:"), caseTab));
    m_caseCombo = new QComboBox(caseTab);
    m_caseCombo->addItems({ tr("lowercase"), tr("UPPERCASE"), tr("Title Case"), tr("camelCase") });
    caseRow->addWidget(m_caseCombo, 1);
    caseLayout->addLayout(caseRow);
    caseLayout->addStretch();
    m_modeTabs->addTab(caseTab, tr("Change Case"));

    mainLayout->addWidget(m_modeTabs);

    // 2. Preview Table
    m_previewTable = new QTableWidget(this);
    m_previewTable->setColumnCount(3);
    m_previewTable->setHorizontalHeaderLabels({ tr("Original Name"), tr("New Name"), tr("Status") });
    m_previewTable->horizontalHeader()->setStretchLastSection(false);
    m_previewTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_previewTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_previewTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_previewTable->verticalHeader()->hide();
    m_previewTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_previewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_previewTable->setStyleSheet(ThemeManager::css(QString(
        "QTableWidget { background: %1; border: 1px solid %2; border-radius: 8px; color: %3; }"
        "QHeaderView::section { background: %4; color: %5; border: none; border-bottom: 1px solid %2; padding: 6px; font-weight: 600; font-size: 11px; }"
    ).arg(ThemeManager::BG_BASE).arg(ThemeManager::BORDER).arg(ThemeManager::TEXT_PRIMARY).arg(ThemeManager::BG_SURFACE).arg(ThemeManager::TEXT_MUTED)));

    mainLayout->addWidget(m_previewTable, 1);

    // 3. Footer
    QHBoxLayout *footerLayout = new QHBoxLayout();
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet(ThemeManager::css(QString("color: %1; font-size: 12px;").arg(ThemeManager::TEXT_SECONDARY)));
    footerLayout->addWidget(m_statusLabel, 1);

    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    connect(m_cancelBtn, &QPushButton::clicked, this, &BatchRenameDialog::reject);
    footerLayout->addWidget(m_cancelBtn);

    m_renameBtn = new QPushButton(tr("Rename Files"), this);
    m_renameBtn->setStyleSheet(ThemeManager::css(QString(
        "QPushButton { background: %1; color: #1e1e2e; font-weight: 600; border-radius: 6px; padding: 6px 16px; }"
        "QPushButton:hover { background: %2; }"
    ).arg(ThemeManager::ACCENT).arg(ThemeManager::ACCENT_PRESS)));
    connect(m_renameBtn, &QPushButton::clicked, this, &BatchRenameDialog::applyRename);
    footerLayout->addWidget(m_renameBtn);

    mainLayout->addLayout(footerLayout);

    // Connect signals to update preview
    connect(m_modeTabs, &QTabWidget::currentChanged, this, &BatchRenameDialog::updatePreview);
    connect(m_findEdit, &QLineEdit::textChanged, this, &BatchRenameDialog::updatePreview);
    connect(m_replaceEdit, &QLineEdit::textChanged, this, &BatchRenameDialog::updatePreview);
    connect(m_useRegexCheck, &QCheckBox::toggled, this, &BatchRenameDialog::updatePreview);
    connect(m_caseSensitiveCheck, &QCheckBox::toggled, this, &BatchRenameDialog::updatePreview);
    connect(m_prefixEdit, &QLineEdit::textChanged, this, &BatchRenameDialog::updatePreview);
    connect(m_suffixEdit, &QLineEdit::textChanged, this, &BatchRenameDialog::updatePreview);
    connect(m_numBaseEdit, &QLineEdit::textChanged, this, &BatchRenameDialog::updatePreview);
    connect(m_startNumSpin, &QSpinBox::valueChanged, this, &BatchRenameDialog::updatePreview);
    connect(m_digitsSpin, &QSpinBox::valueChanged, this, &BatchRenameDialog::updatePreview);
    connect(m_keepExtCheck, &QCheckBox::toggled, this, &BatchRenameDialog::updatePreview);
    connect(m_caseCombo, &QComboBox::currentIndexChanged, this, &BatchRenameDialog::updatePreview);
}

static QString toTitleCase(const QString &str) {
    QStringList words = str.split(' ', Qt::SkipEmptyParts);
    for (QString &w : words) {
        if (!w.isEmpty()) {
            w = w.left(1).toUpper() + w.mid(1).toLower();
        }
    }
    return words.join(' ');
}

void BatchRenameDialog::updatePreview() {
    m_renames.clear();
    m_previewTable->setRowCount(m_originalPaths.size());

    int mode = m_modeTabs->currentIndex();
    QSet<QString> seenNewNames;
    bool hasConflicts = false;
    int changedCount = 0;

    for (int i = 0; i < m_originalPaths.size(); ++i) {
        QFileInfo info(m_originalPaths[i]);
        QString origName = info.fileName();
        QString baseName = info.completeBaseName();
        QString ext = info.suffix().isEmpty() ? QString() : ("." + info.suffix());

        QString newName = origName;

        if (mode == 0) {
            // Find & Replace
            QString findStr = m_findEdit->text();
            QString replStr = m_replaceEdit->text();
            if (!findStr.isEmpty()) {
                if (m_useRegexCheck->isChecked()) {
                    QRegularExpression::PatternOptions opts = m_caseSensitiveCheck->isChecked()
                        ? QRegularExpression::NoPatternOption
                        : QRegularExpression::CaseInsensitiveOption;
                    QRegularExpression re(findStr, opts);
                    newName = origName;
                    newName.replace(re, replStr);
                } else {
                    Qt::CaseSensitivity cs = m_caseSensitiveCheck->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
                    newName = origName;
                    newName.replace(findStr, replStr, cs);
                }
            }
        } else if (mode == 1) {
            // Prefix / Suffix
            QString pfx = m_prefixEdit->text();
            QString sfx = m_suffixEdit->text();
            newName = pfx + baseName + sfx + ext;
        } else if (mode == 2) {
            // Numbering
            QString base = m_numBaseEdit->text();
            int num = m_startNumSpin->value() + i;
            int digits = m_digitsSpin->value();
            QString numStr = QString("%1").arg(num, digits, 10, QChar('0'));
            newName = QString("%1_%2%3").arg(base, numStr, m_keepExtCheck->isChecked() ? ext : QString());
        } else if (mode == 3) {
            // Change Case
            int caseIdx = m_caseCombo->currentIndex();
            if (caseIdx == 0) newName = baseName.toLower() + ext.toLower();
            else if (caseIdx == 1) newName = baseName.toUpper() + ext;
            else if (caseIdx == 2) newName = toTitleCase(baseName) + ext;
            else if (caseIdx == 3) {
                QString title = toTitleCase(baseName).remove(' ');
                if (!title.isEmpty()) title[0] = title[0].toLower();
                newName = title + ext;
            }
        }

        m_renames.append({ m_originalPaths[i], newName });

        QTableWidgetItem *itemOrig = new QTableWidgetItem(origName);
        QTableWidgetItem *itemNew = new QTableWidgetItem(newName);
        QTableWidgetItem *itemStatus = new QTableWidgetItem();

        if (newName != origName) changedCount++;

        QString targetPath = QFileInfo(m_originalPaths[i]).dir().filePath(newName);
        bool clashesOutsideBatch = newName != origName && QFileInfo::exists(targetPath) && !m_originalPaths.contains(targetPath);
        if (seenNewNames.contains(newName)) {
            itemStatus->setText(tr("Duplicate Name"));
            itemStatus->setForeground(QColor("#f38ba8")); // danger
            hasConflicts = true;
        } else if (clashesOutsideBatch) {
            itemStatus->setText(tr("Already Exists"));
            itemStatus->setForeground(QColor("#f38ba8"));
            hasConflicts = true;
        } else if (newName != origName) {
            itemStatus->setText(tr("Renamed"));
            itemStatus->setForeground(QColor("#a6e3a1")); // success
        } else {
            itemStatus->setText(tr("Unchanged"));
            itemStatus->setForeground(QColor("#a6adc8"));
        }

        seenNewNames.insert(newName);

        m_previewTable->setItem(i, 0, itemOrig);
        m_previewTable->setItem(i, 1, itemNew);
        m_previewTable->setItem(i, 2, itemStatus);
    }

    m_renameBtn->setEnabled(!hasConflicts && changedCount > 0);
    if (hasConflicts) {
        m_statusLabel->setText(tr("<font color='#f38ba8'>⚠️ Conflict detected: a target name is duplicated or already exists.</font>"));
    } else {
        m_statusLabel->setText(tr("%1 of %2 files will be renamed.").arg(changedCount).arg(m_originalPaths.size()));
    }
}

void BatchRenameDialog::applyRename() {
    int successCount = 0;
    QString firstError;

    // Two phases: park every changed file under a unique temp name first, so a rename whose
    // target is another batch member's current name (swaps, shifted numbering) cannot fail.
    struct Step { QString tmpPath; QString newName; QString origName; };
    QList<Step> pending;
    const QString stamp = QString::number(QDateTime::currentMSecsSinceEpoch());
    int idx = 0;
    for (const auto &pair : m_renames) {
        const QString &oldPath = pair.first;
        const QString &newName = pair.second;
        if (QFileInfo(oldPath).fileName() == newName) {
            successCount++;
            continue;
        }
        QString tmpName = QString(".bitfm-rename-%1-%2").arg(stamp).arg(idx++);
        QString err;
        if (m_fileOps.renameFile(oldPath, tmpName, &err)) {
            pending.append({ QFileInfo(oldPath).dir().filePath(tmpName), newName, QFileInfo(oldPath).fileName() });
        } else if (firstError.isEmpty()) {
            firstError = err;
        }
    }
    for (const Step &st : pending) {
        QString err;
        if (m_fileOps.renameFile(st.tmpPath, st.newName, &err)) {
            successCount++;
        } else {
            if (firstError.isEmpty()) firstError = err;
            // Leave nothing parked under a temp name: put the original name back
            m_fileOps.renameFile(st.tmpPath, st.origName, nullptr);
        }
    }

    emit filesRenamed();

    if (!firstError.isEmpty()) {
        QMessageBox::warning(this, tr("Rename Completed with Errors"),
            tr("Renamed %1 of %2 items.\nError: %3").arg(successCount).arg(m_renames.size()).arg(firstError));
    }

    accept();
}
