/****************************************************************************
** Copyright (c) 2020, Fougue Ltd. <http://www.fougue.pro>
** All rights reserved.
** See license at https://github.com/fougue/mayo/blob/master/LICENSE.txt
****************************************************************************/

#pragma once

#include "io_reader.h"
#include "io_writer.h"
#include <IGESCAFControl_Reader.hxx>
#include <IGESCAFControl_Writer.hxx>

namespace Mayo {
namespace IO {

// Opencascade-based reader for IGES file format
class OccIgesReader : public Reader {
public:
    OccIgesReader();
    bool readFile(const QString& filepath, TaskProgress* progress) override;
    bool transfer(DocumentPtr doc, TaskProgress* progress) override;

private:
    IGESCAFControl_Reader m_reader;
};

// Opencascade-based writer for IGES file format
class OccIgesWriter : public Writer {
public:
    bool transfer(Span<const ApplicationItem> appItems, TaskProgress* progress) override;
    bool writeFile(const QString& filepath, TaskProgress* progress) override;

private:
    IGESCAFControl_Writer m_writer;
};

} // namespace IO
} // namespace Mayo
