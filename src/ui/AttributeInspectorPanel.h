#pragma once

#include <QDockWidget>

class QLabel;
class QTableWidget;
class SpecTableIndex;

class AttributeInspectorPanel : public QDockWidget
{
    Q_OBJECT
public:
    explicit AttributeInspectorPanel(QWidget* parent = nullptr);

    void showSymbol(const QString& name, SpecTableIndex* index);
    void clear();

private:
    QLabel*       m_nameLabel  = nullptr;
    QLabel*       m_typeLabel  = nullptr;
    QTableWidget* m_table      = nullptr;
};
