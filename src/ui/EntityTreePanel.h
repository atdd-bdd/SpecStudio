#pragma once

#include <QDockWidget>

class QTreeWidget;
class SpecTableIndex;

class EntityTreePanel : public QDockWidget
{
    Q_OBJECT
public:
    explicit EntityTreePanel(QWidget* parent = nullptr);

    void refresh(SpecTableIndex* index);
    void clear();

signals:
    void navigateToRequested(const QString& filePath, int line);

private:
    QTreeWidget* m_tree = nullptr;
};
