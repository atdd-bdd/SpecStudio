#pragma once

#include "BaseEditor.h"
#include "../model/SpecConfig.h"

class QLineEdit;
class QComboBox;
class QCheckBox;
class QLabel;
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
    void markDirty();

private:
    void populateFromConfig(const SpecConfig& cfg);
    SpecConfig configFromForm() const;

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
};
