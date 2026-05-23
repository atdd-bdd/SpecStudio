#pragma once

#include <QString>

enum class FileType {
    Feature,
    FeatureX,
    Csv,
    Markdown,
    Text,
    Other
};

inline FileType fileTypeFromPath(const QString& path)
{
    QString ext = path.section('.', -1).toLower();
    if (ext == "feature")  return FileType::Feature;
    if (ext == "featurex") return FileType::FeatureX;
    if (ext == "csv" || ext == "xls" || ext == "xlsx") return FileType::Csv;
    if (ext == "md")       return FileType::Markdown;
    if (ext == "txt")      return FileType::Text;
    return FileType::Other;
}

inline QString fileTypeDisplayName(FileType t)
{
    switch (t) {
        case FileType::Feature:  return "Feature";
        case FileType::FeatureX: return "FeatureX";
        case FileType::Csv:      return "Spreadsheet";
        case FileType::Markdown: return "Markdown";
        case FileType::Text:     return "Text";
        default:                 return "File";
    }
}
