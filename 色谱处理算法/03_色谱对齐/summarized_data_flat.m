%% ========================================================================
% 批量提取所有子文件夹内的文件到一个新文件夹（无子文件夹）
% ========================================================================

clear; clc;

%% ================= 路径设置 =================
input_root  = 'All_599_aligned_by_ref';     % 原始文件夹（有子文件夹）
output_root = 'All_599_aligned_by_ref_flat';       % 新文件夹（无子文件夹）

if ~exist(output_root,'dir')
    mkdir(output_root);
end

%% ================= 扫描所有子文件夹 =================
ref_folders = dir(input_root);
is_sub = [ref_folders.isdir] & ~ismember({ref_folders.name},{'.','..'});
ref_folders = ref_folders(is_sub);

fprintf('共检测到 %d 个子文件夹\n', numel(ref_folders));

copied_files = {};

%% ================= 遍历子文件夹 =================
for i_ref = 1:numel(ref_folders)
    sub_name   = ref_folders(i_ref).name;
    sub_folder = fullfile(input_root, sub_name);

    % 找出所有 MAT 文件
    files = dir(fullfile(sub_folder, '*.mat'));

    for i_file = 1:numel(files)
        src_file  = fullfile(files(i_file).folder, files(i_file).name);
        dest_file = fullfile(output_root, files(i_file).name);

        % 避免重复复制
        if ~ismember(files(i_file).name, copied_files)
            copyfile(src_file, dest_file);
            copied_files{end+1} = files(i_file).name;
            fprintf('复制 %s → %s\n', files(i_file).name, output_root);
        end
    end
end

fprintf('\n✔ 所有文件已提取完成，总数 %d 个\n', numel(copied_files));
