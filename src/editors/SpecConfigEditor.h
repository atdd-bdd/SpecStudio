#pragma once

#include "BaseEditor.h"
#include "../model/SpecConfig.h"

class QLineEdit;
class QComboBox;
class QCheckBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

class SpecConfigEditor : public BaseEditor
{
    Q_OBJECT

public:
    explicit SpecConfigEditor(const QString& filePath, QWidget* parent = nullptr);

    void load(const QString& path) override;
    bool save() override;

private slots:
    void onLanguageChanged(const QString& language);
    void onBrowseOutputDir();
    void onBrowseConverter();
    void onBrowseProdClassesDir();
    void onCreateProdClassesToggled(bool checked);
    void onExtSpecSelectionChanged();
    void onExtSpecAdd();
    void onExtSpecRemove();
    void onBrowseExtSpecFile();
    void onBrowseExtSpecProdDir();
    void markDirty();

private:
    void populateFromConfig(const SpecConfig& cfg);
    SpecConfig configFromForm() const;
    void saveCurrentExtSpecRow();

    QLineEdit*    m_outputDir      = nullptr;
    QComboBox*    m_language       = nullptr;
    QComboBox*    m_framework      = nullptr;
    QLineEdit*    m_namespace      = nullptr;
    QCheckBox*    m_overwriteGlue  = nullptr;
    QCheckBox*    m_copySpectable  = nullptr;
    QLineEdit*    m_converterPath  = nullptr;
    QPushButton*  m_browseConverter = nullptr;
    QPlainTextEdit* m_imports      = nullptr;
    QLineEdit*    m_tagFilter      = nullptr;
    QLabel*       m_statusLabel    = nullptr;

    QCheckBox*    m_createProdClasses    = nullptr;
    QWidget*      m_prodClassesDetails   = nullptr;
    QLineEdit*    m_prodClassesDir       = nullptr;
    QPushButton*  m_browseProdClassesDir = nullptr;
    QLineEdit*    m_prodClassesPackage   = nullptr;

    // External spectables
    QListWidget*    m_extSpecList          = nullptr;
    QPushButton*    m_extSpecAdd           = nullptr;
    QPushButton*    m_extSpecRemove        = nullptr;
    QWidget*        m_extSpecDetail        = nullptr;
    QLineEdit*      m_extSpecFile          = nullptr;
    QPushButton*    m_browseExtSpecFile    = nullptr;
    QLineEdit*      m_extSpecProdDir       = nullptr;
    QPushButton*    m_browseExtSpecProdDir = nullptr;
    QPlainTextEdit* m_extSpecImports       = nullptr;
    QList<ExternalSpectable> m_extSpectables;
    int             m_extSpecCurrentRow    = -1;
    bool            m_extSpecSyncing       = false;
};
