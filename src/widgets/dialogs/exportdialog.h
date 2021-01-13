#ifndef EXPORTDIALOG_H
#define EXPORTDIALOG_H

#include "scrolldialog.h"

#include <export/exportdata.h>

class QGroupBox;
class QPushButton;
class QComboBox;
class QCheckBox;
class QLineEdit;
class QProgressBar;
class QPlainTextEdit;

namespace vnotex
{
    class Notebook;
    class Node;
    class Buffer;
    class Exporter;

    class ExportDialog : public ScrollDialog
    {
        Q_OBJECT
    public:
        // Current notebook/folder/buffer.
        ExportDialog(const Notebook *p_notebook,
                     const Node *p_folder,
                     const Buffer *p_buffer,
                     QWidget *p_parent = nullptr);

    protected:
        void rejectedButtonClicked() Q_DECL_OVERRIDE;

    private:
        enum AdvancedSettings
        {
            General,
            Max
        };

        void setupUI();

        QGroupBox *setupSourceGroup(QWidget *p_parent);

        QGroupBox *setupTargetGroup(QWidget *p_parent);

        QGroupBox *setupAdvancedGroup(QWidget *p_parent);

        QWidget *setupGeneralAdvancedSettings(QWidget *p_parent);

        void setupButtonBox();

        QString getOutputDir() const;

        void initOptions();

        void restoreFields(const ExportOption &p_option);

        void saveFields(ExportOption &p_option);

        void startExport();

        void appendLogLine(const QString &p_text);

        void updateUIOnExport();

        // Return exported files count.
        int doExport(ExportOption p_option);

        Exporter *getExporter();

        // Managed by QObject.
        Exporter *m_exporter = nullptr;

        const Notebook *m_notebook = nullptr;

        const Node *m_folder = nullptr;

        const Buffer *m_buffer = nullptr;

        // Last exported single file.
        QString m_exportedFile;

        bool m_exportOngoing = false;

        QPushButton *m_exportBtn = nullptr;

        QPushButton *m_openDirBtn = nullptr;

        QPushButton *m_copyContentBtn = nullptr;

        QComboBox *m_sourceComboBox = nullptr;

        QComboBox *m_targetFormatComboBox = nullptr;

        QCheckBox *m_transparentBgCheckBox = nullptr;

        QComboBox *m_renderingStyleComboBox = nullptr;

        QComboBox *m_syntaxHighlightStyleComboBox = nullptr;

        QLineEdit *m_outputDirLineEdit = nullptr;

        QProgressBar *m_progressBar = nullptr;

        QCheckBox *m_recursiveCheckBox = nullptr;

        QCheckBox *m_exportAttachmentsCheckBox = nullptr;

        QVector<QWidget *> m_advancedSettings;

        QPlainTextEdit *m_consoleTextEdit = nullptr;

        static ExportOption s_option;
    };
}

#endif // EXPORTDIALOG_H
