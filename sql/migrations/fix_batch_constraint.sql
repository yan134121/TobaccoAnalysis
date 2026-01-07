-- =====================================================
-- 修复批次约束问题的数据库迁移脚本
-- 
-- 问题描述：tobacco_batch表的batch_code字段当前是全局唯一，
--          导致不同型号不能有相同的批次编码，引发"Duplicate entry"错误
-- 
-- 解决方案：将batch_code约束从全局唯一改为在同一model_id下唯一
-- 
-- 执行日期：2024-12-XX
-- 版本：v2.1
-- =====================================================

-- 开始事务
START TRANSACTION;

-- 1. 备份现有数据（可选，建议在生产环境执行）
-- CREATE TABLE tobacco_batch_backup_20241201 AS SELECT * FROM tobacco_batch;

-- 2. 检查当前约束状态
SELECT 
    CONSTRAINT_NAME, 
    COLUMN_NAME, 
    CONSTRAINT_TYPE 
FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE 
WHERE TABLE_NAME = 'tobacco_batch' 
    AND TABLE_SCHEMA = DATABASE()
    AND COLUMN_NAME = 'batch_code';

-- 3. 删除现有的全局唯一约束
-- 注意：约束名可能是 'batch_code' 或 'batch_code_UNIQUE'，需要根据实际情况调整
ALTER TABLE tobacco_batch DROP INDEX batch_code;

-- 4. 添加新的复合唯一约束
ALTER TABLE tobacco_batch ADD UNIQUE KEY uq_batch_per_model (model_id, batch_code);

-- 5. 验证新约束是否正确添加
SELECT 
    INDEX_NAME,
    COLUMN_NAME,
    SEQ_IN_INDEX,
    NON_UNIQUE
FROM INFORMATION_SCHEMA.STATISTICS 
WHERE TABLE_NAME = 'tobacco_batch' 
    AND TABLE_SCHEMA = DATABASE()
    AND INDEX_NAME = 'uq_batch_per_model'
ORDER BY SEQ_IN_INDEX;

-- 6. 测试数据完整性（可选）
-- 以下查询应该返回空结果，表示没有违反新约束的数据
SELECT 
    model_id, 
    batch_code, 
    COUNT(*) as duplicate_count
FROM tobacco_batch 
GROUP BY model_id, batch_code 
HAVING COUNT(*) > 1;

-- 提交事务
COMMIT;

-- =====================================================
-- 验证脚本（可选执行）
-- =====================================================

-- 测试1：验证不同型号可以有相同批次编码
-- 注意：以下测试语句仅用于验证，实际执行时请根据具体情况调整

/*
-- 假设存在model_id为1和2的记录
INSERT INTO tobacco_batch (model_id, batch_code, description) 
VALUES (1, 'TEST_BATCH_001', '型号1的测试批次');

INSERT INTO tobacco_batch (model_id, batch_code, description) 
VALUES (2, 'TEST_BATCH_001', '型号2的测试批次');
-- 上述两条插入都应该成功

-- 测试2：验证同一型号下批次编码仍然唯一
INSERT INTO tobacco_batch (model_id, batch_code, description) 
VALUES (1, 'TEST_BATCH_001', '型号1的重复批次');
-- 上述插入应该失败并报唯一约束错误

-- 清理测试数据
DELETE FROM tobacco_batch WHERE batch_code = 'TEST_BATCH_001';
*/

-- =====================================================
-- 回滚脚本（如果需要回滚）
-- =====================================================

/*
-- 如果修改后出现问题，可以执行以下回滚操作：

START TRANSACTION;

-- 1. 删除新的复合约束
ALTER TABLE tobacco_batch DROP INDEX uq_batch_per_model;

-- 2. 恢复原有的全局唯一约束
ALTER TABLE tobacco_batch ADD UNIQUE KEY batch_code (batch_code);

-- 3. 如果数据有问题，从备份恢复（需要先创建备份）
-- DROP TABLE tobacco_batch;
-- CREATE TABLE tobacco_batch AS SELECT * FROM tobacco_batch_backup_20241201;
-- 然后重新添加索引和外键约束

COMMIT;
*/

-- =====================================================
-- 执行说明
-- =====================================================

/*
执行步骤：
1. 在执行前务必备份整个数据库
2. 在测试环境先执行此脚本验证
3. 确认无误后在生产环境执行
4. 执行后运行验证查询确保约束正确
5. 测试应用程序功能确保正常工作

预期结果：
- tobacco_batch表的batch_code字段不再有全局唯一约束
- 新增了(model_id, batch_code)的复合唯一约束
- 不同型号可以有相同的批次编码
- 同一型号下批次编码仍然保持唯一
- 现有应用程序代码无需修改即可正常工作
*/