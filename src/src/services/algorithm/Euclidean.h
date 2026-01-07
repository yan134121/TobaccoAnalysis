#ifndef EUCLIDEAN_H
#define EUCLIDEAN_H

#include "services/algorithm/comparison/IDifferenceStrategy.h" // 仍然需要实现这个接口
#include <QObject> // 仍然需要继承 QObject 以便使用 tr()

/**
 * @brief The Euclidean class
 * 
 * 实现了欧氏距离算法。
 * 
 * 注意：为了与项目架构保持一致，此“算法”类仍然实现 IDifferenceStrategy 接口。
 * 虽然类名没有包含 "Strategy" 后缀，但在代码层面，它扮演着策略的角色。
 */
class Euclidean : public QObject, public IDifferenceStrategy // 【关键】继承 IDifferenceStrategy
{
    Q_OBJECT // QObject 子类需要这个宏
public:
    explicit Euclidean(QObject *parent = nullptr); // 构造函数

    // --- 实现 IDifferenceStrategy 接口 ---
    QString algorithmId() const override;
    QString userVisibleName() const override;
    double calculateDifference(const Curve& curveA, const Curve& curveB, const QVariantMap& params = QVariantMap()) override;
};

#endif // EUCLIDEAN_H