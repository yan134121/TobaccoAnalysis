// 单表操作：增删改查、按条件查询、标识符获取
#ifndef SINGLETOBACCOSAMPLEDAO_H // 修改: 头文件卫士更新
#define SINGLETOBACCOSAMPLEDAO_H

// 修改: 包含的实体类头文件更新
#include "SingleTobaccoSampleData.h"
#include <QList>
#include <QMap>
#include <QVariant>
#include <QSqlQuery>

#include "common.h"

// 修改: 类名更新以匹配新的实体
class SingleTobaccoSampleDAO {
public:
    // 构造函数
    SingleTobaccoSampleDAO() = default;
    explicit SingleTobaccoSampleDAO(QSqlDatabase& db) : m_db(db) {}
    
    /**
     * @brief 插入一条样品记录
     * @param sample 如果插入成功，会更新 sample 对象的 id 字段为数据库生成的自增ID
     * @return 成功返回新记录的ID, 失败返回 -1
     */
    // 修改: 实体类类型更新，返回类型改为int
    int insert(SingleTobaccoSampleData& sample);

    /**
     * @brief 根据ID更新一条样品记录
     * @param sample 待更新的样品对象 (其ID必须有效)
     * @return 成功返回 true, 失败返回 false
     */
    // 修改: 实体类类型更新
    bool update(const SingleTobaccoSampleData& sample);

    /**
     * @brief 根据ID删除一条样品记录
     * @param id 要删除的记录ID
     * @return 成功返回 true, 失败返回 false
     */
    bool remove(int id);

    /**
     * @brief 根据ID查询一条样品记录
     * @param id 要查询的记录ID
     * @return 如果未找到，返回一个默认构造的 SingleTobaccoSampleData 对象 (id 为 -1)
     */
    // 修改: 实体类类型更新
    SingleTobaccoSampleData getById(int id);

    /**
     * @brief 查询所有样品记录
     * @return 包含所有样品对象的列表
     */
    // 修改: 实体类类型更新
    QList<SingleTobaccoSampleData> queryAll();

    /**
     * @brief 根据条件查询样品记录，支持模糊查询和多条件查询
     * @param conditions 查询条件 Map. 示例:
     *   {{"project_name", "香烟型号A"}, {"year", 2024}}
     *   对于模糊查询，值中可以包含通配符 %, 如 {{"short_code", "S01%"}}
     * @return 符合条件的样品对象列表
     */
    // 修改: 实体类类型更新
    QList<SingleTobaccoSampleData> query(const QMap<QString, QVariant>& conditions);
    
    /**
     * @brief 根据短码查询样品
     * @param shortCode 样品短码
     * @return 符合条件的样品对象列表
     */
    QList<SingleTobaccoSampleData> queryByShortCode(const QString& shortCode);

    QList<SingleTobaccoSampleData> queryByShortCodeAndParallelNo(
    const QString& shortCode, int parallelNo);

    QList<SingleTobaccoSampleData> queryByProjectNameAndBatchCodeAndShortCodeAndParallelNo(
    const QString& projectName, const QString& batchCode, const QString& shortCode, int parallelNo);
    SampleIdentifier getSampleIdentifierById(int sampleId);

    
    QList<SampleIdentifier> getSampleIdentifiersByProjectAndBatch(const QString& projectName, const QString& batchCode, const BatchType batchType);
    

private:
    /**
     * @brief 辅助函数，将 QSqlQuery 的当前行数据填充到 SingleTobaccoSampleData 对象中
     * @param query 执行了查询并定位到有效行的 QSqlQuery 对象
     * @return 填充好数据的 SingleTobaccoSampleData 对象
     */
    // 修改: 辅助函数名和返回类型更新
    SingleTobaccoSampleData createSampleFromQuery(QSqlQuery& query);
    QSqlDatabase m_db; // 数据库连接

    
};

#endif // SINGLETOBACCOSAMPLEDAO_H
