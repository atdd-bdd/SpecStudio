#pragma once

#include <QTabWidget>
#include <QMap>

class BaseEditor;
class Project;

class EditorTabWidget : public QTabWidget
{
    Q_OBJECT

public:
    explicit EditorTabWidget(QWidget* parent = nullptr);

    void openFile(const QString& absolutePath, Project* project = nullptr);
    BaseEditor* currentEditor() const;
    BaseEditor* editorForPath(const QString& absolutePath) const;

    bool saveCurrentFile();
    bool saveAllFiles();

signals:
    void currentFileChanged(const QString& absolutePath);

private slots:
    void onTabCloseRequested(int index);
    void onCurrentChanged(int index);

private:
    bool closeTab(int index);

    QMap<QString, BaseEditor*> m_openFiles;
};
