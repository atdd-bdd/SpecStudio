#pragma once

#include "PlainTextEditor.h"
#include <QStringList>

class QMenu;
class SpecTableIndex;

class SpecTableEditor : public PlainTextEditor
{
    Q_OBJECT

public:
    explicit SpecTableEditor(const QString& filePath, QWidget* parent = nullptr);

    void setIndex(SpecTableIndex* index) { m_index = index; }
    void refreshDynamicCompletions();

    bool save() override;

signals:
    void goToDefinitionRequested(const QString& filePath, int line);
    void findReferencesRequested(const QString& symbolName);
    void renameSymbolRequested(const QString& symbolName);
    void symbolAtCursor(const QString& name);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void populateContextMenu(QMenu* menu) override;

private:
    bool handleTableTabKey();
    void formatAllTables();
    void insertTableRow();
    void deleteTableRow();
    void transposeTable();
    void showHoverPreview(const QPoint& viewportPos, const QPoint& globalPos);
    void autoInsertTableHeader();
    void extractAsAttributeSet();
    void extractAsDefine();

    SpecTableIndex* m_index          = nullptr;
    QStringList     m_staticKeywords;
};
