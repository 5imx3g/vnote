#ifndef EXPORTER_H
#define EXPORTER_H

#include <QObject>
#include <QStringList>

#include "exportdata.h"

namespace vnotex
{
    class Notebook;
    class Node;
    class Buffer;

    class Exporter : public QObject
    {
        Q_OBJECT
    public:
        // We need the QWidget as parent.
        explicit Exporter(QWidget *p_parent);

        // Return exported output files.
        QStringList doExport(const ExportOption &p_option, const Buffer *p_buffer);

    signals:
        void progressUpdated(int p_val, int p_maximum);

        void logRequested(const QString &p_log);
    };
}

#endif // EXPORTER_H
