-- =========================
-- 数据库：tobacco_system
-- =========================
CREATE DATABASE IF NOT EXISTS tobacco_data CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE tobacco_data;

-- =========================
-- 用户表
-- =========================
CREATE TABLE IF NOT EXISTS user (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    role ENUM('admin', 'researcher', 'viewer') DEFAULT 'viewer',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 1. 烟草型号表
CREATE TABLE IF NOT EXISTS tobacco_model (
  id INT AUTO_INCREMENT PRIMARY KEY,
  model_code VARCHAR(50) NOT NULL UNIQUE COMMENT '型号代号',
  model_name VARCHAR(100) COMMENT '型号名称',
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 2. 批次表
-- CREATE TABLE IF NOT EXISTS tobacco_batch (
--   id INT AUTO_INCREMENT PRIMARY KEY,
--   model_id INT NOT NULL COMMENT '烟草型号ID，关联tobacco_model',
--   batch_code VARCHAR(100) NOT NULL COMMENT '批次编码（例如2025A001）',
--   produce_date DATE COMMENT '生产/采集日期',
--   description TEXT COMMENT '批次说明',
--   created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
--   FOREIGN KEY (model_id) REFERENCES tobacco_model(id) ON DELETE CASCADE,
--   UNIQUE KEY uq_batch_per_model (model_id, batch_code),
--   INDEX idx_model (model_id)
-- ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
CREATE TABLE IF NOT EXISTS tobacco_batch (
  id INT AUTO_INCREMENT PRIMARY KEY,
  model_id INT NOT NULL COMMENT '烟草型号ID，关联tobacco_model',
  batch_code VARCHAR(100) NOT NULL COMMENT '批次编码（例如2025A001）',
  produce_date DATE COMMENT '生产/采集日期',
  description TEXT COMMENT '批次说明',
  batch_type ENUM('NORMAL', 'PROCESS') NOT NULL DEFAULT 'NORMAL' COMMENT '批次类型：NORMAL-常规数据，PROCESS-工艺数据',
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (model_id) REFERENCES tobacco_model(id) ON DELETE CASCADE,
  UNIQUE KEY uq_batch_per_model_type (model_id, batch_code, batch_type),
  INDEX idx_model (model_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


-- -- 3. 样品表
CREATE TABLE IF NOT EXISTS single_tobacco_sample (
  id INT AUTO_INCREMENT PRIMARY KEY,
  batch_id INT NOT NULL COMMENT '批次ID，关联tobacco_batch',
  project_name VARCHAR(50) NOT NULL COMMENT '香烟型号',
  short_code VARCHAR(50) NOT NULL COMMENT '样本编码',
  parallel_no INT NOT NULL COMMENT '平行样编号',
  sample_name VARCHAR(255) COMMENT '样本名称（界面显示）',
  note TEXT COMMENT '备注，如盘信息',
  year INT COMMENT '年份',
  origin VARCHAR(100) COMMENT '产地',
  part VARCHAR(50) COMMENT '部位',
  grade VARCHAR(50) COMMENT '等级',
  type ENUM('单打','混打') COMMENT '打料方式',
  collect_date DATE COMMENT '采集日期',
  detect_date DATE COMMENT '检测日期',
  data_json JSON COMMENT '扩展字段',
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (batch_id) REFERENCES tobacco_batch(id) ON DELETE CASCADE,
  UNIQUE KEY uq_sample (batch_id, short_code, parallel_no)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 4. 大热重数据
CREATE TABLE IF NOT EXISTS tg_big_data (
  id INT AUTO_INCREMENT PRIMARY KEY,
  sample_id INT NOT NULL COMMENT '样品ID',
  serial_no INT NOT NULL COMMENT '原始数据序号',
  temperature DOUBLE COMMENT '温度',
  weight DOUBLE COMMENT '重量 (克)',
  tg_value DOUBLE COMMENT '热重值',
  dtg_value DOUBLE COMMENT '热重值微分',
  source_filename VARCHAR(255) COMMENT '原始文件名',
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (sample_id) REFERENCES single_tobacco_sample(id) ON DELETE CASCADE,
  INDEX idx_bigdata_sample (sample_id, serial_no)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 5. 小热重数据
CREATE TABLE IF NOT EXISTS tg_small_data (
  id INT AUTO_INCREMENT PRIMARY KEY,
  sample_id INT NOT NULL COMMENT '样品ID',
  serial_no INT NOT NULL COMMENT '原始数据序号',
  temperature DOUBLE COMMENT '温度',
  weight DOUBLE COMMENT '重量 (克)',
  tg_value DOUBLE COMMENT '热重值',
  dtg_value DOUBLE COMMENT '热重值微分',
  source_filename VARCHAR(255) COMMENT '原始文件名',
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (sample_id) REFERENCES single_tobacco_sample(id) ON DELETE CASCADE,
  INDEX idx_smalldata_sample (sample_id, serial_no)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 5.1 小热重（原始数据）
CREATE TABLE IF NOT EXISTS tg_small_raw_data (
  id INT AUTO_INCREMENT PRIMARY KEY,
  sample_id INT NOT NULL COMMENT '样品ID',
  serial_no INT NOT NULL COMMENT '原始数据序号',
  temperature DOUBLE COMMENT '温度',
  weight DOUBLE COMMENT '重量 (克)',
  tg_value DOUBLE COMMENT '热重值',
  dtg_value DOUBLE COMMENT '热重值微分',
  source_filename VARCHAR(255) COMMENT '原始文件名',
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (sample_id) REFERENCES single_tobacco_sample(id) ON DELETE CASCADE,
  INDEX idx_smallrawdata_sample (sample_id, serial_no)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 6. 色谱数据
CREATE TABLE IF NOT EXISTS chromatography_data (
  id INT AUTO_INCREMENT PRIMARY KEY,
  sample_id INT NOT NULL COMMENT '样品ID',
  retention_time DOUBLE NOT NULL COMMENT '保留时间 (x轴)',
  response_value DOUBLE NOT NULL COMMENT '响应值 (y轴)',
  source_filename VARCHAR(255) COMMENT '原始文件名',
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (sample_id) REFERENCES single_tobacco_sample(id) ON DELETE CASCADE,
  INDEX idx_chrom_sample (sample_id, retention_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 7. 工艺大热重数据
CREATE TABLE IF NOT EXISTS process_tg_big_data (
  id INT AUTO_INCREMENT PRIMARY KEY,
  sample_id INT NOT NULL COMMENT '样品ID',
  serial_no INT NOT NULL COMMENT '原始数据序号',
  temperature DOUBLE COMMENT '温度',
  weight DOUBLE COMMENT '重量 (克)',
  tg_value DOUBLE COMMENT '热重值',
  dtg_value DOUBLE COMMENT '热重值微分',
  source_filename VARCHAR(255) COMMENT '原始文件名',
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (sample_id) REFERENCES single_tobacco_sample(id) ON DELETE CASCADE,
  INDEX idx_bigdata_sample (sample_id, serial_no)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;


-- =========================
-- 8. 最优平行样表
-- =========================
CREATE TABLE IF NOT EXISTS representative_samples (
  id INT AUTO_INCREMENT PRIMARY KEY,
  batch_id INT NOT NULL,
  short_code VARCHAR(50) NOT NULL,
  data_type VARCHAR(50) NOT NULL COMMENT 'e.g., "大热重", "小热重", "色谱"',

  -- 【核心】被选为代表的那个平行样的 sample_id
  representative_sample_id INT NOT NULL, 
  
  selection_method VARCHAR(50) COMMENT '"auto" or "manual"',
  selection_reason TEXT COMMENT '选择的理由，或自动选择的得分',
  selected_by_user_id INT,
  selected_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,

  FOREIGN KEY (batch_id) REFERENCES tobacco_batch(id) ON DELETE CASCADE,
  FOREIGN KEY (representative_sample_id) REFERENCES single_tobacco_sample(id) ON DELETE CASCADE,

  -- 【关键】确保对于一个 short_code 的一种数据类型，只能有一个代表
  UNIQUE KEY uq_representative (batch_id, short_code, data_type)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;




-- =========================
-- 算法处理结果表
-- =========================
CREATE TABLE IF NOT EXISTS algo_results (
    id INT AUTO_INCREMENT PRIMARY KEY,
    
    raw_type ENUM('big', 'small', 'chrom') NOT NULL COMMENT '原始数据来源类型: 大热重、小热重、色谱',
    raw_id INT NOT NULL COMMENT '对应原始数据表的 ID (tg_big_data.id / tg_small_data.id / chromatography_data.id)',
    
    algo_name VARCHAR(100) NOT NULL COMMENT '算法名称',
    run_no INT NOT NULL DEFAULT 1 COMMENT '同一算法重复运行编号',
    param_json JSON COMMENT '算法运行参数',
    
    x_value DOUBLE NOT NULL COMMENT '算法处理后的横坐标值',
    y_value DOUBLE NOT NULL COMMENT '算法处理后的纵坐标值',
    
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '记录创建时间',
    
    -- 联合唯一约束，避免同一算法同次运行重复插入同一个原始点
    UNIQUE KEY uq_algo_point (raw_type, raw_id, algo_name, run_no, x_value),
    
    -- 常用查询索引
    INDEX idx_raw (raw_type, raw_id),
    INDEX idx_algo (algo_name, run_no)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;



-- =========================
-- 字典选项表 (用于存储下拉框选项)
-- =========================
CREATE TABLE IF NOT EXISTS dictionary_option (
    id INT AUTO_INCREMENT PRIMARY KEY,
    category VARCHAR(50) NOT NULL,          -- 选项类别 (e.g., "Origin", "Part", "Grade", "BlendType")
    value VARCHAR(255) NOT NULL,            -- 选项值 (e.g., "云南", "上部", "A", "单打")
    description TEXT,                       -- 选项描述 (可选)
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY unique_category_value (category, value) -- 确保每个类别下的值唯一
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
