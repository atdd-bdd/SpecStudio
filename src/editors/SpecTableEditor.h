#pragma once

#include "PlainTextEditor.h"
#include <QStringList>

class QMenu;
class QTextBlock;
class SpecTableIndex;

class SpecTableEditor : public PlainTextEditor
{
    Q_OBJECT

public:
    explicit SpecTableEditor(const QString& filePath, QWidget* parent = nullptr);

    void setIndex(SpecTableIndex* index) { m_index = index; }
    // The owning project's root folder — used as a fallback for resolving
    // relative Import/Insert paths when no solution root has been set.
    void setProjectRoot(const QString& root) { m_projectRoot = root; }
    // The solution's base folder — the Select File... dialogs (from the
    // Import/Insert context menu) start here, since inserted/imported files
    // are normally kept somewhere within the solution.
    void setSolutionRoot(const QString& root) { m_solutionRoot = root; }
    void refreshDynamicCompletions();

    bool save() override;

signals:
    void goToDefinitionRequested(const QString& filePath, int line);
    void findReferencesRequested(const QString& symbolName);
    void findStepUsagesRequested(const QString& keyword, const QString& stepText);
    void renameSymbolRequested(const QString& symbolName);
    void symbolAtCursor(const QString& name);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void populateContextMenu(QMenu* menu) override;

private:
    bool handleTableTabKey();
    bool tryExpandSnippet();
    bool browseImportFile();
    bool browseInsertFile(QChar openQuote, QChar closeQuote);
    void formatAllTables();
    void fixTrailingContinuations();
    void insertTableRow();
    void deleteTableRow();
    void transposeTable();
    void showHoverPreview(const QPoint& viewportPos, const QPoint& globalPos);
    void autoInsertTableHeader();
    void checkAdHocTableAttributeSet();
    void insertTableHeaderForCurrentStep();
    void createAttributeSetFromExamplesTable();
    void offerCreateAttributeSetFromTable(QTextBlock ownerLineBlock, const QString& existingName,
                                          const QStringList& headers, bool ownerIsStepLine);
    void extractAsAttributeSet();
    void extractAsDefine();
    void toggleLineComment();
    void editMultilineComment();
    void importCsv();
    static QVector<QStringList> parseCsvFile(const QString& filePath);

    SpecTableIndex* m_index          = nullptr;
    QString         m_projectRoot;
    QString         m_solutionRoot;
    QStringList     m_staticKeywords;
};
