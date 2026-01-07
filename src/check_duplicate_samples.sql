-- 检查 single_tobacco_sample 表中的重复记录
-- 用于诊断 uq_sample 约束冲突问题

-- 1. 查看所有样本记录的关键字段
SELECT 
    id,
    batch_id,
    project_name,
    short_code,
    parallel_no,
    sample_name,
    CONCAT(batch_id, '-', short_code, '-', parallel_no) as constraint_key
FROM single_tobacco_sample 
ORDER BY batch_id, short_code, parallel_no;

-- 2. 查找重复的 (batch_id, short_code, parallel_no) 组合
SELECT 
    batch_id,
    short_code,
    parallel_no,
    COUNT(*) as duplicate_count,
    GROUP_CONCAT(id) as duplicate_ids,
    GROUP_CONCAT(project_name) as project_names
FROM single_tobacco_sample 
GROUP BY batch_id, short_code, parallel_no
HAVING COUNT(*) > 1;

-- 3. 查看 tobacco_batch 表的情况
SELECT 
    tb.id as batch_id,
    tb.batch_code,
    tb.model_id,
    tm.model_code,
    COUNT(sts.id) as sample_count
FROM tobacco_batch tb
LEFT JOIN tobacco_model tm ON tb.model_id = tm.id
LEFT JOIN single_tobacco_sample sts ON tb.id = sts.batch_id
GROUP BY tb.id, tb.batch_code, tb.model_id, tm.model_code
ORDER BY tb.id;

-- 4. 查看具体的问题记录（batch_id=1 的情况）
SELECT 
    sts.id,
    sts.batch_id,
    sts.project_name,
    sts.short_code,
    sts.parallel_no,
    sts.sample_name,
    tb.batch_code,
    tm.model_code
FROM single_tobacco_sample sts
LEFT JOIN tobacco_batch tb ON sts.batch_id = tb.id
LEFT JOIN tobacco_model tm ON tb.model_id = tm.id
WHERE sts.batch_id = 1
ORDER BY sts.short_code, sts.parallel_no;