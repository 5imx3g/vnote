#include "exporter.h"

#include <QWidget>

#include <notebook/notebook.h>
#include <notebook/node.h>
#include <buffer/buffer.h>

using namespace vnotex;

Exporter::Exporter(QWidget *p_parent)
    : QObject(p_parent)
{
}

QStringList Exporter::doExport(const ExportOption &p_option, const Buffer *p_buffer)
{
    return QStringList();
}
