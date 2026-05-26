#pragma once

#include <QDialog>

class QLineEdit;
class QComboBox;
class AppSettings;

class NewFileDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewFileDialog(AppSettings* settings, QWidget* parent = nullptr);

    QString fileName() const;

private:
    QLineEdit* m_nameEdit  = nullptr;
    QComboBox* m_typeCombo = nullptr;
};
