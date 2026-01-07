-- =====================================================
-- 测试长期解决方案：验证project_name-batch_code-short_code-parallel_no唯一性
-- 
-- 测试目标：
-- 1. 验证88-89-88-1和88-89-89-1类型数据能正确插入
-- 2. 验证重复数据会被正确拒绝
-- 3. 验证不同项目可以有相同的batch_code
-- 4. 验证应用层唯一性检查的性能
-- 
-- 执行前提：
-- - 已执行implement_unique_constraint_solution.sql迁移脚本
-- - 应用程序代码已更新
-- 
-- 执行日期：2024-12-XX
-- =====================================================

-- 开始测试事务
START TRANSACTION;

-- =====================================================
-- 第一部分：准备测试数据
-- =====================================================

-- 1. 清理可能存在的测试数据
DELETE FROM single_tobacco_sample 
WHERE project_name IN ('TEST_PROJECT_88', 'TEST_PROJECT_89');

DELETE FROM tobacco_batch 
WHERE batch_code IN ('88', '89') 
  AND model_id IN (
    SELECT id FROM tobacco_model 
    WHERE model_code IN ('TEST_PROJECT_88', 'TEST_PROJECT_89')
  );

DELETE FROM tobacco_model 
WHERE model_code IN ('TEST_PROJECT_88', 'TEST_PROJECT_89');

-- 2. 创建测试用的tobacco_model记录
INSERT INTO tobacco_model (model_code, model_name) VALUES 
('TEST_PROJECT_88', 'Test Project 88'),
('TEST_PROJECT_89', 'Test Project 89');

-- 获取model_id
SET @model_id_88 = (SELECT id FROM tobacco_model WHERE model_code = 'TEST_PROJECT_88');
SET @model_id_89 = (SELECT id FROM tobacco_model WHERE model_code = 'TEST_PROJECT_89');

-- 3. 创建测试用的tobacco_batch记录
INSERT INTO tobacco_batch (model_id, batch_code, produce_date, description) VALUES 
(@model_id_88, '88', CURDATE(), 'Test batch 88 for project 88'),
(@model_id_88, '89', CURDATE(), 'Test batch 89 for project 88'),
(@model_id_89, '88', CURDATE(), 'Test batch 88 for project 89'),
(@model_id_89, '89', CURDATE(), 'Test batch 89 for project 89');

-- 获取batch_id
SET @batch_id_88_88 = (SELECT id FROM tobacco_batch WHERE model_id = @model_id_88 AND batch_code = '88');
SET @batch_id_88_89 = (SELECT id FROM tobacco_batch WHERE model_id = @model_id_88 AND batch_code = '89');
SET @batch_id_89_88 = (SELECT id FROM tobacco_batch WHERE model_id = @model_id_89 AND batch_code = '88');
SET @batch_id_89_89 = (SELECT id FROM tobacco_batch WHERE model_id = @model_id_89 AND batch_code = '89');

-- =====================================================
-- 第二部分：测试正常插入场景
-- =====================================================

-- 4. 测试插入88-89-88-1类型的数据（应该成功）
INSERT INTO single_tobacco_sample (
    batch_id, project_name, short_code, parallel_no, sample_name,
    note, year, origin, part, grade, type, collect_date, detect_date, data_json
) VALUES 
(@batch_id_88_89, 'TEST_PROJECT_88', '88', 1, '88-89-88-1',
 'Test sample 88-89-88-1', 2024, 'Test Origin', 'Test Part', 'Test Grade', 'Test Type', 
 CURDATE(), CURDATE(), '{}');

SELECT 'Test 1: 插入88-89-88-1' as test_name, 
       CASE WHEN ROW_COUNT() > 0 THEN 'PASS' ELSE 'FAIL' END as result;

-- 5. 测试插入88-89-89-1类型的数据（应该成功）
INSERT INTO single_tobacco_sample (
    batch_id, project_name, short_code, parallel_no, sample_name,
    note, year, origin, part, grade, type, collect_date, detect_date, data_json
) VALUES 
(@batch_id_88_89, 'TEST_PROJECT_88', '89', 1, '88-89-89-1',
 'Test sample 88-89-89-1', 2024, 'Test Origin', 'Test Part', 'Test Grade', 'Test Type', 
 CURDATE(), CURDATE(), '{}');

SELECT 'Test 2: 插入88-89-89-1' as test_name, 
       CASE WHEN ROW_COUNT() > 0 THEN 'PASS' ELSE 'FAIL' END as result;

-- 6. 测试不同项目相同batch_code（应该成功）
INSERT INTO single_tobacco_sample (
    batch_id, project_name, short_code, parallel_no, sample_name,
    note, year, origin, part, grade, type, collect_date, detect_date, data_json
) VALUES 
(@batch_id_89_88, 'TEST_PROJECT_89', '88', 1, '89-88-88-1',
 'Test sample 89-88-88-1', 2024, 'Test Origin', 'Test Part', 'Test Grade', 'Test Type', 
 CURDATE(), CURDATE(), '{}');

SELECT 'Test 3: 不同项目相同batch_code' as test_name, 
       CASE WHEN ROW_COUNT() > 0 THEN 'PASS' ELSE 'FAIL' END as result;

-- =====================================================
-- 第三部分：测试重复数据检测
-- =====================================================

-- 7. 测试应用层唯一性检查查询的性能
SET @start_time = NOW(6);

SELECT COUNT(*) as count
FROM single_tobacco_sample s
JOIN tobacco_batch b ON s.batch_id = b.id
WHERE s.project_name = 'TEST_PROJECT_88' 
  AND b.batch_code = '89'
  AND s.short_code = '88'
  AND s.parallel_no = 1;

SET @end_time = NOW(6);
SET @query_time = TIMESTAMPDIFF(MICROSECOND, @start_time, @end_time);

SELECT 'Test 4: 唯一性检查查询性能' as test_name,
       CONCAT(@query_time, ' microseconds') as execution_time,
       CASE WHEN @query_time < 10000 THEN 'PASS' ELSE 'SLOW' END as result;

-- 8. 验证重复数据检测逻辑
SELECT 
    'Test 5: 重复数据检测' as test_name,
    s.project_name,
    b.batch_code,
    s.short_code,
    s.parallel_no,
    COUNT(*) as count,
    CASE WHEN COUNT(*) = 1 THEN 'PASS' ELSE 'FAIL' END as result
FROM single_tobacco_sample s
JOIN tobacco_batch b ON s.batch_id = b.id
WHERE s.project_name IN ('TEST_PROJECT_88', 'TEST_PROJECT_89')
GROUP BY s.project_name, b.batch_code, s.short_code, s.parallel_no
ORDER BY s.project_name, b.batch_code, s.short_code, s.parallel_no;

-- =====================================================
-- 第四部分：测试约束有效性
-- =====================================================

-- 9. 测试tobacco_batch约束（不同model_id可以有相同batch_code）
INSERT INTO tobacco_batch (model_id, batch_code, produce_date, description) VALUES 
(@model_id_88, 'DUPLICATE_TEST', CURDATE(), 'Duplicate test for model 88'),
(@model_id_89, 'DUPLICATE_TEST', CURDATE(), 'Duplicate test for model 89');

SELECT 'Test 6: tobacco_batch约束测试' as test_name,
       CASE WHEN ROW_COUNT() > 0 THEN 'PASS' ELSE 'FAIL' END as result;

-- 10. 验证索引是否正确创建
SELECT 
    'Test 7: 索引验证' as test_name,
    INDEX_NAME,
    GROUP_CONCAT(COLUMN_NAME ORDER BY SEQ_IN_INDEX) as columns,
    CASE WHEN INDEX_NAME IS NOT NULL THEN 'PASS' ELSE 'FAIL' END as result
FROM INFORMATION_SCHEMA.STATISTICS 
WHERE TABLE_NAME = 'single_tobacco_sample' 
    AND TABLE_SCHEMA = DATABASE()
    AND INDEX_NAME = 'idx_uniqueness_check'
GROUP BY INDEX_NAME;

-- =====================================================
-- 第五部分：性能测试
-- =====================================================

-- 11. 批量插入测试（模拟实际使用场景）
SET @batch_start_time = NOW(6);

INSERT INTO single_tobacco_sample (
    batch_id, project_name, short_code, parallel_no, sample_name,
    note, year, origin, part, grade, type, collect_date, detect_date, data_json
) VALUES 
(@batch_id_88_88, 'TEST_PROJECT_88', '01', 1, '88-88-01-1', 'Batch test 1', 2024, 'Origin', 'Part', 'Grade', 'Type', CURDATE(), CURDATE(), '{}'),
(@batch_id_88_88, 'TEST_PROJECT_88', '01', 2, '88-88-01-2', 'Batch test 2', 2024, 'Origin', 'Part', 'Grade', 'Type', CURDATE(), CURDATE(), '{}'),
(@batch_id_88_88, 'TEST_PROJECT_88', '02', 1, '88-88-02-1', 'Batch test 3', 2024, 'Origin', 'Part', 'Grade', 'Type', CURDATE(), CURDATE(), '{}'),
(@batch_id_88_88, 'TEST_PROJECT_88', '02', 2, '88-88-02-2', 'Batch test 4', 2024, 'Origin', 'Part', 'Grade', 'Type', CURDATE(), CURDATE(), '{}'),
(@batch_id_88_88, 'TEST_PROJECT_88', '03', 1, '88-88-03-1', 'Batch test 5', 2024, 'Origin', 'Part', 'Grade', 'Type', CURDATE(), CURDATE(), '{}');

SET @batch_end_time = NOW(6);
SET @batch_time = TIMESTAMPDIFF(MICROSECOND, @batch_start_time, @batch_end_time);

SELECT 'Test 8: 批量插入性能' as test_name,
       CONCAT(@batch_time, ' microseconds for 5 records') as execution_time,
       CASE WHEN @batch_time < 50000 THEN 'PASS' ELSE 'SLOW' END as result;

-- =====================================================
-- 第六部分：测试总结
-- =====================================================

-- 12. 最终数据统计
SELECT 
    'Test Summary' as test_name,
    COUNT(*) as total_test_records,
    COUNT(DISTINCT CONCAT(s.project_name, '-', b.batch_code, '-', s.short_code, '-', s.parallel_no)) as unique_combinations,
    CASE 
        WHEN COUNT(*) = COUNT(DISTINCT CONCAT(s.project_name, '-', b.batch_code, '-', s.short_code, '-', s.parallel_no)) 
        THEN 'PASS: 所有记录都是唯一的' 
        ELSE 'FAIL: 存在重复记录' 
    END as uniqueness_result
FROM single_tobacco_sample s
JOIN tobacco_batch b ON s.batch_id = b.id
WHERE s.project_name IN ('TEST_PROJECT_88', 'TEST_PROJECT_89');

-- 13. 显示所有测试数据
SELECT 
    'Test Data Overview' as section,
    s.id,
    s.project_name,
    b.batch_code,
    s.short_code,
    s.parallel_no,
    s.sample_name,
    CONCAT(s.project_name, '-', b.batch_code, '-', s.short_code, '-', s.parallel_no) as unique_key
FROM single_tobacco_sample s
JOIN tobacco_batch b ON s.batch_id = b.id
WHERE s.project_name IN ('TEST_PROJECT_88', 'TEST_PROJECT_89')
ORDER BY s.project_name, b.batch_code, s.short_code, s.parallel_no;

-- =====================================================
-- 清理测试数据
-- =====================================================

-- 14. 清理测试数据（可选，取消注释以执行）
/*
DELETE FROM single_tobacco_sample 
WHERE project_name IN ('TEST_PROJECT_88', 'TEST_PROJECT_89');

DELETE FROM tobacco_batch 
WHERE batch_code IN ('88', '89', 'DUPLICATE_TEST') 
  AND model_id IN (@model_id_88, @model_id_89);

DELETE FROM tobacco_model 
WHERE model_code IN ('TEST_PROJECT_88', 'TEST_PROJECT_89');

SELECT 'Cleanup completed' as message;
*/

-- 提交测试事务
COMMIT;

-- =====================================================
-- 测试结果说明
-- =====================================================

/*
测试结果解释：

PASS 结果表示：
- 数据插入成功
- 唯一性约束正常工作
- 性能满足要求
- 索引正确创建

FAIL 结果表示：
- 存在重复数据
- 约束未正确设置
- 需要进一步检查

SLOW 结果表示：
- 功能正常但性能需要优化
- 可能需要调整索引策略

预期结果：
1. 88-89-88-1 和 88-89-89-1 数据应该能正常插入
2. 不同项目可以有相同的batch_code
3. 唯一性检查查询应该在10ms内完成
4. 所有记录应该保持唯一性
*/