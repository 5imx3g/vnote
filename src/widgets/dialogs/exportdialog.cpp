#include "exportdialog.h"

#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QProgressBar>
#include <QFileInfo>
#include <QFileDialog>
#include <QUrl>
#include <QPlainTextEdit>
#include <QCoreApplication>

#include <notebook/notebook.h>
#include <notebook/node.h>
#include <buffer/buffer.h>
#include <widgets/widgetsfactory.h>
#include <core/thememgr.h>
#include <core/configmgr.h>
#include <core/vnotex.h>
#include <utils/widgetutils.h>
#include <utils/fileutils.h>
#include <utils/clipboardutils.h>
#include <export/exporter.h>

using namespace vnotex;

ExportOption ExportDialog::s_option;

ExportDialog::ExportDialog(const Notebook *p_notebook,
                           const Node *p_folder,
                           const Buffer *p_buffer,
                           QWidget *p_parent)
    : ScrollDialog(p_parent),
      m_notebook(p_notebook),
      m_folder(p_folder),
      m_buffer(p_buffer)
{
    setupUI();

    initOptions();

    restoreFields(s_option);

    connect(this, &QDialog::finished,
            this, [this]() {
                saveFields(s_option);
            });
}

void ExportDialog::setupUI()
{
    auto widget = new QWidget(this);
    setCentralWidget(widget);

    auto mainLayout = new QVBoxLayout(widget);

    auto sourceBox = setupSourceGroup(widget);
    mainLayout->addWidget(sourceBox);

    auto targetBox = setupTargetGroup(widget);
    mainLayout->addWidget(targetBox);

    auto advancedBox = setupAdvancedGroup(widget);
    mainLayout->addWidget(advancedBox);

    {
        m_consoleTextEdit = WidgetsFactory::createPlainTextConsole(widget);
        m_consoleTextEdit->hide();
        mainLayout->addWidget(m_consoleTextEdit);
    }

    {
        m_progressBar = new QProgressBar(widget);
        m_progressBar->setRange(0, 0);
        m_progressBar->hide();
        mainLayout->addWidget(m_progressBar);
    }

    setupButtonBox();

    setWindowTitle(tr("Export"));
}

QGroupBox *ExportDialog::setupSourceGroup(QWidget *p_parent)
{
    auto box = new QGroupBox(tr("Source"), p_parent);
    auto layout = WidgetsFactory::createFormLayout(box);

    {
        m_sourceComboBox = WidgetsFactory::createComboBox(box);
        if (m_buffer) {
            m_sourceComboBox->addItem(tr("Current Buffer (%1)").arg(m_buffer->getName()),
                                      static_cast<int>(ExportSource::CurrentBuffer));
        }
        if (m_folder && m_folder->isContainer()) {
            m_sourceComboBox->addItem(tr("Current Folder (%1)").arg(m_folder->getName()),
                                      static_cast<int>(ExportSource::CurrentFolder));
        }
        if (m_notebook) {
            m_sourceComboBox->addItem(tr("Current Notebook (%1)").arg(m_notebook->getName()),
                                      static_cast<int>(ExportSource::CurrentNotebook));
        }
        layout->addRow(tr("Source:"), m_sourceComboBox);
    }

    {
        // TODO: Source format filtering.
    }

    return box;
}

QGroupBox *ExportDialog::setupTargetGroup(QWidget *p_parent)
{
    auto box = new QGroupBox(tr("Target"), p_parent);
    auto layout = WidgetsFactory::createFormLayout(box);

    {
        m_targetFormatComboBox = WidgetsFactory::createComboBox(box);
        m_targetFormatComboBox->addItem(tr("Markdown"),
                                        static_cast<int>(ExportFormat::Markdown));
        m_targetFormatComboBox->addItem(tr("HTML"),
                                        static_cast<int>(ExportFormat::HTML));
        m_targetFormatComboBox->addItem(tr("PDF"),
                                        static_cast<int>(ExportFormat::PDF));
        m_targetFormatComboBox->addItem(tr("Custom"),
                                        static_cast<int>(ExportFormat::Custom));
        layout->addRow(tr("Format:"), m_targetFormatComboBox);
    }

    {
        m_transparentBgCheckBox = WidgetsFactory::createCheckBox(tr("Use transparent background"), box);
        layout->addRow(m_transparentBgCheckBox);
    }

    {
        const auto webStyles = VNoteX::getInst().getThemeMgr().getWebStyles();

        m_renderingStyleComboBox = WidgetsFactory::createComboBox(box);
        layout->addRow(tr("Rendering style:"), m_renderingStyleComboBox);
        for (const auto &pa : webStyles) {
            m_renderingStyleComboBox->addItem(pa.first, pa.second);
        }

        m_syntaxHighlightStyleComboBox = WidgetsFactory::createComboBox(box);
        layout->addRow(tr("Syntax highlighting style:"), m_syntaxHighlightStyleComboBox);
        for (const auto &pa : webStyles) {
            m_syntaxHighlightStyleComboBox->addItem(pa.first, pa.second);
        }
    }

    {
        auto outputLayout = new QHBoxLayout();

        m_outputDirLineEdit = WidgetsFactory::createLineEdit(box);
        outputLayout->addWidget(m_outputDirLineEdit);

        auto browseBtn = new QPushButton(tr("Browse"), box);
        outputLayout->addWidget(browseBtn);
        connect(browseBtn, &QPushButton::clicked,
                this, [this]() {
                    QString initPath = getOutputDir();
                    if (!QFileInfo::exists(initPath)) {
                        initPath = ConfigMgr::getDocumentOrHomePath();
                    }

                    QString dirPath = QFileDialog::getExistingDirectory(this,
                                                                        tr("Select Export Output Directory"),
                                                                        initPath,
                                                                        QFileDialog::ShowDirsOnly
                                                                        | QFileDialog::DontResolveSymlinks);

                    if (!dirPath.isEmpty()) {
                        m_outputDirLineEdit->setText(dirPath);
                    }
                });

        layout->addRow(tr("Output directory:"), outputLayout);
    }

    return box;
}

QGroupBox *ExportDialog::setupAdvancedGroup(QWidget *p_parent)
{
    auto box = new QGroupBox(tr("Advanced"), p_parent);
    auto layout = new QVBoxLayout(box);

    m_advancedSettings.resize(AdvancedSettings::Max);

    m_advancedSettings[AdvancedSettings::General] = setupGeneralAdvancedSettings(box);
    layout->addWidget(m_advancedSettings[AdvancedSettings::General]);

    return box;
}

QWidget *ExportDialog::setupGeneralAdvancedSettings(QWidget *p_parent)
{
    QWidget *widget = new QWidget(p_parent);
    auto layout = WidgetsFactory::createFormLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    {
        m_recursiveCheckBox = WidgetsFactory::createCheckBox(tr("Process sub-folders"), widget);
        layout->addRow(m_recursiveCheckBox);
    }

    {
        m_exportAttachmentsCheckBox = WidgetsFactory::createCheckBox(tr("Export attachments"), widget);
        layout->addRow(m_exportAttachmentsCheckBox);
    }

    return widget;
}

void ExportDialog::setupButtonBox()
{
    setDialogButtonBox(QDialogButtonBox::Close);

    auto box = getDialogButtonBox();

    m_exportBtn = box->addButton(tr("Export"), QDialogButtonBox::ActionRole);
    connect(m_exportBtn, &QPushButton::clicked,
            this, &ExportDialog::startExport);

    m_openDirBtn = box->addButton(tr("Open Directory"), QDialogButtonBox::ActionRole);
    connect(m_openDirBtn, &QPushButton::clicked,
            this, [this]() {
                auto dir = getOutputDir();
                if (!dir.isEmpty()) {
                    WidgetUtils::openUrlByDesktop(QUrl::fromLocalFile(dir));
                }
            });

    m_copyContentBtn = box->addButton(tr("Copy Content"), QDialogButtonBox::ActionRole);
    m_copyContentBtn->setToolTip(tr("Copy exported file content"));
    m_copyContentBtn->setEnabled(false);
    connect(m_copyContentBtn, &QPushButton::clicked,
            this, [this]() {
                if (m_exportedFile.isEmpty()) {
                    return;
                }

                const auto content = FileUtils::readTextFile(m_exportedFile);
                if (!content.isEmpty()) {
                    ClipboardUtils::setTextToClipboard(content);
                }
            });
}

QString ExportDialog::getOutputDir() const
{
    return m_outputDirLineEdit->text();
}

void ExportDialog::initOptions()
{
    if (!s_option.m_renderingStyleFile.isEmpty()) {
        return;
    }

    const auto &theme = VNoteX::getInst().getThemeMgr().getCurrentTheme();
    s_option.m_renderingStyleFile = theme.getFile(Theme::File::WebStyleSheet);
    s_option.m_syntaxHighlightStyleFile = theme.getFile(Theme::File::HighlightStyleSheet);

    s_option.m_outputDir = ConfigMgr::getDocumentOrHomePath();
}

void ExportDialog::restoreFields(const ExportOption &p_option)
{
    {
        int idx = m_sourceComboBox->findData(static_cast<int>(p_option.m_source));
        if (idx != -1) {
            m_sourceComboBox->setCurrentIndex(idx);
        }
    }

    {
        int idx = m_targetFormatComboBox->findData(static_cast<int>(p_option.m_targetFormat));
        if (idx != -1) {
            m_targetFormatComboBox->setCurrentIndex(idx);
        }
    }

    m_transparentBgCheckBox->setChecked(p_option.m_useTransparentBg);

    {
        int idx = m_renderingStyleComboBox->findData(p_option.m_renderingStyleFile);
        if (idx != -1) {
            m_renderingStyleComboBox->setCurrentIndex(idx);
        }
    }

    {
        int idx = m_syntaxHighlightStyleComboBox->findData(p_option.m_syntaxHighlightStyleFile);
        if (idx != -1) {
            m_syntaxHighlightStyleComboBox->setCurrentIndex(idx);
        }
    }

    m_outputDirLineEdit->setText(p_option.m_outputDir);

    m_recursiveCheckBox->setChecked(p_option.m_recursive);

    m_exportAttachmentsCheckBox->setChecked(p_option.m_exportAttachments);
}

void ExportDialog::saveFields(ExportOption &p_option)
{
    p_option.m_source = static_cast<ExportSource>(m_sourceComboBox->currentData().toInt());
    p_option.m_targetFormat = static_cast<ExportFormat>(m_targetFormatComboBox->currentData().toInt());
    p_option.m_useTransparentBg = m_transparentBgCheckBox->isChecked();
    p_option.m_renderingStyleFile = m_renderingStyleComboBox->currentData().toString();
    p_option.m_syntaxHighlightStyleFile = m_syntaxHighlightStyleComboBox->currentData().toString();
    p_option.m_outputDir = getOutputDir();
    p_option.m_recursive = m_recursiveCheckBox->isChecked();
    p_option.m_exportAttachments = m_exportAttachmentsCheckBox->isChecked();
}

void ExportDialog::startExport()
{
    if (m_exportOngoing) {
        return;
    }

    // On start.
    {
        m_exportedFile.clear();
        m_exportOngoing = true;
        m_consoleTextEdit->show();
        m_consoleTextEdit->clear();
        updateUIOnExport();
    }

    saveFields(s_option);

    int ret = doExport(s_option);
    appendLogLine(tr("%n file(s) exported", "", ret));

    // On end.
    {
        m_exportOngoing = false;
        updateUIOnExport();
    }
}

void ExportDialog::rejectedButtonClicked()
{
    if (m_exportOngoing) {
        // Just cancel the export.
        // TODO.
    } else {
        Dialog::rejectedButtonClicked();
    }
}

void ExportDialog::appendLogLine(const QString &p_text)
{
    m_consoleTextEdit->appendPlainText(">>> " + p_text);
    m_consoleTextEdit->ensureCursorVisible();
    QCoreApplication::sendPostedEvents();
}

void ExportDialog::updateUIOnExport()
{
    m_exportBtn->setEnabled(!m_exportOngoing);
    m_progressBar->setVisible(m_exportOngoing);
    m_copyContentBtn->setEnabled(!m_exportedFile.isEmpty());
}

int ExportDialog::doExport(ExportOption p_option)
{
    // TODO: Check ExportOption.

    int exportedFilesCount = 0;

    switch (s_option.m_source) {
    case ExportSource::CurrentBuffer:
    {
        Q_ASSERT(m_buffer);
        const auto outputFiles = getExporter()->doExport(p_option, m_buffer);
        exportedFilesCount = outputFiles.size();
        if (exportedFilesCount == 1 && p_option.m_targetFormat == ExportFormat::HTML) {
            m_exportedFile = outputFiles.first();
        }
        break;
    }

    case ExportSource::CurrentFolder:
    {
        Q_ASSERT(m_folder);
        break;
    }

    case ExportSource::CurrentNotebook:
    {
        Q_ASSERT(m_notebook);
        break;
    }
    }

    return exportedFilesCount;
}

Exporter *ExportDialog::getExporter()
{
    if (!m_exporter) {
        m_exporter = new Exporter(this);
    }
    return m_exporter;
}
