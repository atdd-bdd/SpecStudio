#include "ExternalEditor.h"

#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QVBoxLayout>

ExternalEditor::ExternalEditor(const QString& filePath,
                                const QString& externalProgram,
                                QWidget* parent)
    : BaseEditor(filePath, parent)
    , m_program(externalProgram)
{
    auto* layout = new QVBoxLayout(this);

    auto* label = new QLabel(tr("External file: %1").arg(filePath), this);
    label->setAlignment(Qt::AlignCenter);

    auto* btn = new QPushButton(tr("Open in %1").arg(externalProgram), this);
    connect(btn, &QPushButton::clicked, this, [this] {
        QProcess::startDetached(m_program, {this->filePath()});
    });

    layout->addStretch();
    layout->addWidget(label);
    layout->addWidget(btn, 0, Qt::AlignCenter);
    layout->addStretch();

    setLayout(layout);
    load(filePath);
}

void ExternalEditor::load(const QString& path)
{
    setFilePath(path);
    setDirty(false);
}

bool ExternalEditor::save()
{
    setDirty(false);
    return true;
}
