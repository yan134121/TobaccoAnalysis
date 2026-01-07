#include "SampleSelectionManager.h"
#include "Logger.h"
#include "core/singletons/StringManager.h"

#include <QMutexLocker>

// 实现线程安全单例
SampleSelectionManager* SampleSelectionManager::instance()
{
    DEBUG_LOG << "SampleSelectionManager::instance()";
    static SampleSelectionManager inst;
    return &inst;
}

SampleSelectionManager::SampleSelectionManager(QObject* parent)
    : QObject(parent)
{
}

void SampleSelectionManager::setSelected(int sampleId, bool selected, const QString& origin)
{
    if (sampleId <= 0) {
        return; // 样本ID无效，直接忽略
    }

    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        const bool currentlySelected = m_selected.contains(sampleId);
        if (selected && !currentlySelected) {
            m_selected.insert(sampleId);
            changed = true;
        } else if (!selected && currentlySelected) {
            m_selected.remove(sampleId);
            changed = true;
        }
    }

    // 仅在状态实际发生变化时发射信号，避免无效刷新
    if (changed) {
        emit selectionChanged(sampleId, selected, origin);
    }
}

void SampleSelectionManager::setSelectedWithType(int sampleId, const QString& dataType, bool selected, const QString& origin)
{
    if (sampleId <= 0) {
        return; // 样本ID无效，直接忽略
    }

    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        const bool currentlySelected = m_selected.contains(sampleId);
        if (selected && !currentlySelected) {
            m_selected.insert(sampleId);
            m_sampleType.insert(sampleId, dataType);
            QSet<int>& bucket = m_selectedByType[dataType];
            bucket.insert(sampleId);
            changed = true;
        } else if (!selected && currentlySelected) {
            m_selected.remove(sampleId);
            // 从类型桶中移除
            QString type = dataType;
            if (type.isEmpty() && m_sampleType.contains(sampleId)) {
                type = m_sampleType.value(sampleId);
            }
            if (!type.isEmpty() && m_selectedByType.contains(type)) {
                m_selectedByType[type].remove(sampleId);
                if (m_selectedByType[type].isEmpty()) {
                    m_selectedByType.remove(type);
                }
            }
            m_sampleType.remove(sampleId);
            changed = true;
        }
    }

    if (changed) {
        emit selectionChanged(sampleId, selected, origin);
        emit selectionChangedByType(sampleId, dataType, selected, origin);
        // // 同步到 StringManager 的样本集合，驱动处理界面导航刷新
        // if (selected) {
        //     StringManager::instance()->addSample(dataType, sampleId);
        // } else {
        //     StringManager::instance()->removeSample(dataType, sampleId);
        // }
    }
}

bool SampleSelectionManager::isSelected(int sampleId) const
{
    if (sampleId <= 0) return false;
    QMutexLocker locker(&m_mutex);
    return m_selected.contains(sampleId);
}

QSet<int> SampleSelectionManager::selectedIds() const
{
    QMutexLocker locker(&m_mutex);
    return m_selected;
}

QSet<int> SampleSelectionManager::selectedIdsByType(const QString& dataType) const
{
    QMutexLocker locker(&m_mutex);
    return m_selectedByType.value(dataType);
}

QHash<QString, int> SampleSelectionManager::selectedCountsAllTypes() const
{
    QHash<QString, int> counts;
    QMutexLocker locker(&m_mutex);
    for (auto it = m_selectedByType.constBegin(); it != m_selectedByType.constEnd(); ++it) {
        counts.insert(it.key(), it.value().size());
    }
    return counts;
}
