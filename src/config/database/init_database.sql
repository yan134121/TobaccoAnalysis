-- 建表 SQL 脚本
-- -- 建立数据库（如果不存在）
-- CREATE DATABASE IF NOT EXISTS tobacco_leaf_db DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
-- USE tobacco_leaf_db;


-- 单料烟色谱表
CREATE TABLE IF NOT EXISTS raw_leaf_chromatogram (
    id INT AUTO_INCREMENT PRIMARY KEY,
    -- sample_type ENUM('TGA', 'GC', 'Other'),  -- 类型标识
    time VARCHAR(50),
    data INT
);

-- -- 单料烟原始数据表
-- CREATE TABLE IF NOT EXISTS raw_leaf_sample (
--     id INT AUTO_INCREMENT PRIMARY KEY,
--     -- sample_type ENUM('TGA', 'GC', 'Other'),  -- 类型标识
--     code VARCHAR(50),
--     year INT,
--     origin VARCHAR(100),
--     part VARCHAR(50),
--     grade VARCHAR(50),
--     is_mixed BOOLEAN,
--     sample_date DATE,
--     tga_raw_data JSON,
--     gc_raw_data JSON,
--     create_time DATETIME DEFAULT CURRENT_TIMESTAMP
-- );

-- -- 单料烟处理后数据
-- CREATE TABLE IF NOT EXISTS processed_leaf_sample (
--     id INT AUTO_INCREMENT PRIMARY KEY,
--     raw_sample_id INT,
--     data_type ENUM('TGA-L', 'TGA-S', 'GC'),
--     norm BOOLEAN,
--     smooth BOOLEAN,
--     derivative BOOLEAN,
--     average BOOLEAN,
--     processed_data JSON,
--     process_params JSON,
--     create_time DATETIME DEFAULT CURRENT_TIMESTAMP,
--     FOREIGN KEY (raw_sample_id) REFERENCES raw_leaf_sample(id)
-- );

-- -- 拟合叶组信息
-- CREATE TABLE IF NOT EXISTS fitted_leaf_group_info (
--     id INT AUTO_INCREMENT PRIMARY KEY,
--     name VARCHAR(100),
--     fitting_date DATE,
--     composition JSON,
--     remarks TEXT,
--     create_time DATETIME DEFAULT CURRENT_TIMESTAMP
-- );

-- -- 用户信息
-- CREATE TABLE IF NOT EXISTS users (
--     id INT AUTO_INCREMENT PRIMARY KEY,
--     username VARCHAR(50) NOT NULL,
--     password_hash VARCHAR(128) NOT NULL,
--     role ENUM('管理员', '普通用户') DEFAULT '普通用户',
--     create_time DATETIME DEFAULT CURRENT_TIMESTAMP
-- );
