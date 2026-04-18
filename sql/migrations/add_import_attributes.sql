-- 为 5 张数据点表添加 import_attributes JSON 列
-- 存储每次导入时用户填写的属性信息（编码、年份、产地、部位、等级、叶梗分离方式、检测日期、分厂）

ALTER TABLE tg_big_data
    ADD COLUMN import_attributes JSON NULL COMMENT '导入属性（编码/年份/产地/部位/等级/叶梗分离方式/检测日期/分厂）';

ALTER TABLE tg_small_data
    ADD COLUMN import_attributes JSON NULL COMMENT '导入属性（编码/年份/产地/部位/等级/叶梗分离方式/检测日期/分厂）';

ALTER TABLE tg_small_raw_data
    ADD COLUMN import_attributes JSON NULL COMMENT '导入属性（编码/年份/产地/部位/等级/叶梗分离方式/检测日期/分厂）';

ALTER TABLE process_tg_big_data
    ADD COLUMN import_attributes JSON NULL COMMENT '导入属性（编码/年份/产地/部位/等级/叶梗分离方式/检测日期/分厂）';

ALTER TABLE chromatography_data
    ADD COLUMN import_attributes JSON NULL COMMENT '导入属性（编码/年份/产地/部位/等级/叶梗分离方式/检测日期/分厂）';
