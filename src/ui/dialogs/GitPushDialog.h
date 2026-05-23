#pragma once

#include <QDialog>

class QLineEdit;

class GitPushDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitPushDialog(QWidget* parent = nullptr);

    QString changeReason() const;

private:
    QLineEdit* m_reasonEdit = nullptr;
};
