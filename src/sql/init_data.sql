USE tobacco_system;

-- 初始化用户
INSERT INTO user (username, password_hash, role)
VALUES
('admin', '21232f297a57a5a743894a0e4a801fc3', 'admin'), -- md5('admin')
('researcher1', 'e99a18c428cb38d5f260853678922e03', 'researcher'); -- md5('abc123')

-- 初始化叶组
INSERT INTO leaf_group (name, description)
VALUES
('叶组A', '测试叶组数据'),
('叶组B', '备用叶组数据');
