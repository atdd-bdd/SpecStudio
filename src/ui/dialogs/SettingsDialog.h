#pragma once

#include <QDialog>

class AppSettings;
class Project;
class QCheckBox;
class QComboBox;
class QFontComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    SettingsDialog(AppSettings* settings,
                   Project*     currentProject,
                   QWidget*     parent = nullptr);

private:
    void buildGeneralTab(class QTabWidget* tabs);
    void buildEditorTab(QTabWidget* tabs);
    void buildGitTab(QTabWidget* tabs);
    void buildFeaturexTab(QTabWidget* tabs);
    void buildAppearanceTab(QTabWidget* tabs);
    void buildFontsTab(QTabWidget* tabs);

    void loadValues();
    void saveValues();

    AppSettings*  m_settings       = nullptr;
    Project*      m_project        = nullptr;

    // Git fields
    QLineEdit*    m_urlEdit        = nullptr;
    QLineEdit*    m_branchEdit     = nullptr;
    QLineEdit*    m_userEdit       = nullptr;
    QLineEdit*    m_passEdit       = nullptr;

    // FeatureX fields
    QCheckBox*    m_implicitImport = nullptr;
    QCheckBox*    m_uniqueScenario = nullptr;
    QCheckBox*    m_uniqueStep     = nullptr;
    QComboBox*    m_stepScope      = nullptr;

    // General
    QLineEdit*    m_defaultLocEdit = nullptr;

    // Editor association table
    QTableWidget* m_editorTable    = nullptr;

    // Appearance
    QCheckBox*    m_darkTheme      = nullptr;

    // Fonts
    QFontComboBox* m_editorFontCombo = nullptr;
    QSpinBox*      m_editorFontSize  = nullptr;
    QLabel*        m_editorPreview   = nullptr;
    QFontComboBox* m_outputFontCombo = nullptr;
    QSpinBox*      m_outputFontSize  = nullptr;
    QLabel*        m_outputPreview   = nullptr;
    QFontComboBox* m_uiFontCombo     = nullptr;
    QSpinBox*      m_uiFontSize      = nullptr;
    QLabel*        m_uiPreview       = nullptr;
};
