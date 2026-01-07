-- =====================================================
-- 实现长期解决方案：project_name-batch_code-short_code-parallel_no唯一性
-- 
-- 问题描述：当前系统无法确保project_name-batch_code-short_code-parallel_no
--          组合的唯一性，导致88-89-88-1类型的数据出现冲突
-- 
-- 解决方案：
-- 1. 确保tobacco_batch表的约束正确（model_id, batch_code）
-- 2. 添加应用层唯一性检查的数据库索引支持
-- 3. 清理现有冲突数据
-- 
-- 执行日期：2024-12-XX
-- 版本：v2.2
-- =====================================================

-- 开始事务
START TRANSACTION;

-- =====================================================
-- 第一部分：备份和检查现有数据
-- =====================================================

-- 1. 创建备份表（生产环境建议执行）
CREATE TABLE IF NOT EXISTS single_tobacco_sample_backup_20241201 AS 
SELECT * FROM single_tobacco_sample;

CREATE TABLE IF NOT EXISTS tobacco_batch_backup_20241201 AS 
SELECT * FROM tobacco_batch;

-- 2. 检查当前冲突数据
SELECT 
    '冲突数据检查' as check_type,
    s.project_name,
    b.batch_code,
    s.short_code,
    s.parallel_no,
    COUNT(*) as duplicate_count
FROM single_tobacco_sample s
JOIN tobacco_batch b ON s.batch_id = b.id
GROUP BY s.project_name, b.batch_code, s.short_code, s.parallel_no
HAVING COUNT(*) > 1
ORDER BY duplicate_count DESC;

-- =====================================================
-- 第二部分：修复tobacco_batch表约束
-- =====================================================

-- 3. 检查tobacco_batch表的当前约束
SELECT 
    'tobacco_batch约束检查' as check_type,
    CONSTRAINT_NAME, 
    COLUMN_NAME, 
    CONSTRAINT_TYPE 
FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE 
WHERE TABLE_NAME = 'tobacco_batch' 
    AND TABLE_SCHEMA = DATABASE()
    AND COLUMN_NAME = 'batch_code';

-- 4. 如果存在全局batch_code约束，则删除它
-- 注意：这个操作可能会失败，如果约束不存在，可以忽略错误
SET @sql = 'ALTER TABLE tobacco_batch DROP INDEX batch_code';
SET @sql_exists = (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS 
                   WHERE TABLE_NAME = 'tobacco_batch' 
                     AND TABLE_SCHEMA = DATABASE()
                     AND INDEX_NAME = 'batch_code');

-- 只有当约束存在时才执行删除
SET @sql_final = IF(@sql_exists > 0, @sql, 'SELECT "batch_code index does not exist" as message');
PREPARE stmt FROM @sql_final;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- 5. 添加正确的复合唯一约束（如果不存在）
SET @constraint_exists = (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS 
                         WHERE TABLE_NAME = 'tobacco_batch' 
                           AND TABLE_SCHEMA = DATABASE()
                           AND INDEX_NAME = 'uq_batch_per_model');

SET @add_constraint_sql = IF(@constraint_exists = 0, 
    'ALTER TABLE tobacco_batch ADD UNIQUE KEY uq_batch_per_model (model_id, batch_code)',
    'SELECT "uq_batch_per_model constraint already exists" as message');

PREPARE stmt FROM @add_constraint_sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- =====================================================
-- 第三部分：添加性能优化索引
-- =====================================================

-- 6. 为应用层唯一性检查添加复合索引
-- 这个索引将大大提高唯一性检查查询的性能
SET @index_exists = (SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS 
                    WHERE TABLE_NAME = 'single_tobacco_sample' 
                      AND TABLE_SCHEMA = DATABASE()
                      AND INDEX_NAME = 'idx_uniqueness_check');

SET @add_index_sql = IF(@index_exists = 0,
    'CREATE INDEX idx_uniqueness_check ON single_tobacco_sample (project_name, short_code, parallel_no, batch_id)',
    'SELECT "idx_uniqueness_check index already exists" as message');

PREPARE stmt FROM @add_index_sql;
EXECUTE stmt;
DEALLOCATE PREPARE stmt;

-- =====================================================
-- 第四部分：数据清理（可选）
-- =====================================================

-- 7. 标识需要清理的重复数据
-- 注意：这里只是标识，不直接删除，需要人工确认
CREATE TEMPORARY TABLE temp_duplicates AS
SELECT 
    s.id,
    s.project_name,
    b.batch_code,
    s.short_code,
    s.parallel_no,
    s.created_at,
    ROW_NUMBER() OVER (
        PARTITION BY s.project_name, b.batch_code, s.short_code, s.parallel_no 
        ORDER BY s.created_at ASC
    ) as row_num
FROM single_tobacco_sample s
JOIN tobacco_batch b ON s.batch_id = b.id;

-- 8. 显示重复数据（保留最早的记录，标记其他为重复）
SELECT 
    '重复数据清理建议' as action_type,
    id,
    project_name,
    batch_code,
    short_code,
    parallel_no,
    created_at,
    CASE 
        WHEN row_num = 1 THEN '保留'
        ELSE '建议删除'
    END as action_recommendation
FROM temp_duplicates
WHERE id IN (
    SELECT id FROM temp_duplicates 
    WHERE (project_name, batch_code, short_code, parallel_no) IN (
        SELECT project_name, batch_code, short_code, parallel_no
        FROM temp_duplicates
        GROUP BY project_name, batch_code, short_code, parallel_no
        HAVING COUNT(*) > 1
    )
)
ORDER BY project_name, batch_code, short_code, parallel_no, created_at;

-- =====================================================
-- 第五部分：验证和测试
-- =====================================================

-- 9. 验证约束是否正确设置
SELECT 
    '约束验证' as check_type,
    INDEX_NAME,
    COLUMN_NAME,
    SEQ_IN_INDEX,
    NON_UNIQUE
FROM INFORMATION_SCHEMA.STATISTICS 
WHERE TABLE_NAME = 'tobacco_batch' 
    AND TABLE_SCHEMA = DATABASE()
    AND INDEX_NAME = 'uq_batch_per_model'
ORDER BY SEQ_IN_INDEX;

-- 10. 验证索引是否正确创建
SELECT 
    '索引验证' as check_type,
    INDEX_NAME,
    COLUMN_NAME,
    SEQ_IN_INDEX
FROM INFORMATION_SCHEMA.STATISTICS 
WHERE TABLE_NAME = 'single_tobacco_sample' 
    AND TABLE_SCHEMA = DATABASE()
    AND INDEX_NAME = 'idx_uniqueness_check'
ORDER BY SEQ_IN_INDEX;

-- 11. 最终数据完整性检查
SELECT 
    '数据完整性检查' as check_type,
    COUNT(*) as total_samples,
    COUNT(DISTINCT CONCAT(s.project_name, '-', b.batch_code, '-', s.short_code, '-', s.parallel_no)) as unique_combinations,
    COUNT(*) - COUNT(DISTINCT CONCAT(s.project_name, '-', b.batch_code, '-', s.short_code, '-', s.parallel_no)) as remaining_duplicates
FROM single_tobacco_sample s
JOIN tobacco_batch b ON s.batch_id = b.id;

-- =====================================================
-- 提交事务
-- =====================================================

-- 如果所有操作都成功，提交事务
COMMIT;

-- =====================================================
-- 使用说明
-- =====================================================

/*
执行此脚本后：

1. 数据库约束已更新：
   - tobacco_batch表现在使用(model_id, batch_code)复合唯一约束
   - 添加了性能优化索引

2. 应用程序代码已更新：
   - SingleTobaccoSampleData类支持batchCode字段
   - SingleTobaccoSampleDAO支持用户指定的batch_code
   - 添加了应用层唯一性检查

3. 数据清理：
   - 脚本会显示重复数据的清理建议
   - 需要根据业务需求手动清理重复数据

4. 测试建议：
   - 测试插入88-89-88-1和88-89-89-1类型的数据
   - 验证不同项目可以有相同的batch_code
   - 验证相同项目内batch_code-short_code-parallel_no组合的唯一性

5. 回滚方案（如果需要）：
   - 使用备份表恢复数据
   - 删除新添加的约束和索引
*/