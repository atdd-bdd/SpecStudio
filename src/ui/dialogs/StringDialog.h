#pragma once

#include <QDialog>

class QPlainTextEdit;

class StringDialog : public QDialog {
    Q_OBJECT
public:
    explicit StringDialog(const QString& content, QWidget* parent = nullptr);
    QString content() const;

private:
    QPlainTextEdit* m_edit;
};
