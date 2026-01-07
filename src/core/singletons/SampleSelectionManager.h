#ifndef SAMPLESELECTIONMANAGER_H
#define SAMPLESELECTIONMANAGER_H

#include <QObject>
#include <QSet>
#include <QMutex>
#include <QHash>
#include <QString>

// 集中维护样本选中状态的单例管理器，提供统一的信号与接口
class SampleSelectionManager : public QObject {
    Q_OBJECT
public:
    // 获取单例实例（线程安全的函数内静态）
    static SampleSelectionManager* instance();

    // 设置某个样本的选中状态，并携带来源标识以防回声
    void setSelected(int sampleId, bool selected, const QString& origin);

    // 按数据类型记录选中状态（不会替代旧接口，便于兼容）
    void setSelectedWithType(int sampleId, const QString& dataType, bool selected, const QString& origin);

    // 查询样本是否选中
    bool isSelected(int sampleId) const;

    // 获取所有选中的样本ID集合
    QSet<int> selectedIds() const;

    // 获取指定数据类型下的选中样本ID集合
    QSet<int> selectedIdsByType(const QString& dataType) const;

    // 获取所有数据类型的选中计数
    QHash<QString, int> selectedCountsAllTypes() const;

signals:
    // 样本选中状态变化信号，origin 用于标记来源（例如对话框/导航树）
    void selectionChanged(int sampleId, bool selected, const QString& origin);

    // 携带数据类型的选中变化信号
    void selectionChangedByType(int sampleId, const QString& dataType, bool selected, const QString& origin);

private:
    explicit SampleSelectionManager(QObject* parent = nullptr);

    // 当前选中的样本ID集合
    QSet<int> m_selected;

    // 按数据类型维护选中样本ID集合
    QHash<QString, QSet<int>> m_selectedByType;

    // 样本ID到数据类型的映射，用于快捷清理
    QHash<int, QString> m_sampleType;

    // 保护 m_selected 的互斥锁
    mutable QMutex m_mutex;
};

#endif // SAMPLESELECTIONMANAGER_H
