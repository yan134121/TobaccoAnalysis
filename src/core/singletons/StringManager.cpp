
#include "StringManager.h"

// 【关键】静态成员变量需要在类外部进行定义和初始化
// (虽然这里是函数内的静态变量，但遵循这个原则是好的)

StringManager* StringManager::instance()
{
    // 使用函数内静态变量实现线程安全的单例
    static StringManager instance;
    return &instance;
}

StringManager::StringManager(QObject *parent) : QObject(parent)
{
    // 在构造函数中加载所有字符串
    loadStrings();
}

QString StringManager::getString(const QString &key) const
{
    // value() 方法是线程安全的
    // 如果在 m_stringMap 中找不到 key，返回一个带标记的键本身，方便调试
    return m_stringMap.value(key, QString("[[%1]]").arg(key));
}

void StringManager::loadStrings()
{
    // *** 这是我们所有UI字符串的“数据库” ***
    // tr() 在这里是为了让 lupdate 工具能够扫描到这些字符串并生成 .ts 文件

    // 在 loadStrings() 中
    m_stringMap["nav.workspace.root"] = tr("打开的窗口");
    m_stringMap["nav.datasource.root"] = tr("原料数据源");
    m_stringMap["nav.processdata.root"] = tr("工序数据源");
        
    // --- 通用 ---
    m_stringMap["app.title"] = tr("烟草数据分析系统");
    m_stringMap["error.title"] = tr("错误");
    m_stringMap["warning.title"] = tr("警告");
    m_stringMap["info.title"] = tr("提示");
    // 数据库连接失败时的提示文案（不再提示程序退出）
    m_stringMap["db.connection_failed"] = tr("无法连接到数据库，请检查配置或联系管理员。\n当前将以未连接数据库模式运行，部分功能可能不可用。");

    // --- 主窗口 (MainWindow) ---
    m_stringMap["main.dock.navigator"] = tr("数据导航");
    m_stringMap["main.dock.properties"] = tr("属性/详情");
    m_stringMap["main.dock.analysis"] = tr("分析平台");
    m_stringMap["main.menu.file"] = tr("文件(&F)");
    m_stringMap["main.menu.view"] = tr("视图(&V)");
    m_stringMap["main.menu.settings"] = tr("设置(&S)");
    m_stringMap["main.menu.dataProcess"] = tr("数据处理(&D)");
    m_stringMap["main.menu.tools"] = tr("工具(&T)");
    m_stringMap["main.menu.help"] = tr("帮助(&H)");
    m_stringMap["main.toolbar.file"] = tr("文件");
    m_stringMap["main.toolbar.common"] = tr("常用工具");

    // --- 动作 (Actions) ---
    m_stringMap["action.import"] = tr("导入数据(&I)...");
    m_stringMap["action.export"] = tr("导出数据(&E)...");
    m_stringMap["action.exit"] = tr("退出(&X)");
    m_stringMap["action.zoom_in"] = tr("放大");
    m_stringMap["action.zoom_out"] = tr("缩小");
    m_stringMap["action.zoom_reset"] = tr("重置缩放");
    m_stringMap["action.about"] = tr("关于(&A)...");

    // --- 导航树 (DataNavigator) ---
    m_stringMap["nav.dataset"] = tr("数据集");
    m_stringMap["nav.context.expand_all"] = tr("全部展开");
    m_stringMap["nav.context.collapse_all"] = tr("全部折叠");
    m_stringMap["nav.context.refresh"] = tr("刷新");

    // --- 样本视图 (SampleViewWindow) ---
    m_stringMap["view.sample.title"] = tr("样本: %1");
    m_stringMap["view.sample.chart.tg_big"] = tr("大热重");
    m_stringMap["view.sample.chart.tg_small"] = tr("小热重");
    m_stringMap["view.sample.chart.chrom"] = tr("色谱");
    m_stringMap["view.sample.chart.x_label.serial"] = tr("数据序号");
    m_stringMap["view.sample.chart.x_label.temp"] = tr("温度 (°C)");
    m_stringMap["view.sample.chart.x_label.time"] = tr("保留时间 (min)");
    m_stringMap["view.sample.chart.y_label.dtg"] = tr("热重微分值 (DTG)");
    m_stringMap["view.sample.chart.y_label.response"] = tr("响应值");
    m_stringMap["view.sample.parallel_sample"] = tr("平行样 %1");

    //  算法设置
    m_stringMap["action.align"] = tr("对齐");
    m_stringMap["action.normalize"] = tr("归一化");
    m_stringMap["action.smooth"] = tr("平滑");
    m_stringMap["action.base_line"] = tr("底对齐");
    m_stringMap["action.peak_line"] = tr("峰对齐");
    m_stringMap["action.diff"] = tr("差异度");


    // 算法处理
    m_stringMap["action.tg_big_data_process"] = tr("大热重");
    m_stringMap["action.tg_small_data_process"] = tr("小热重");
    m_stringMap["action.chromatograph_data_process"] = tr("色谱");
    m_stringMap["action.process_tg_big_data_process"] = tr("工序大热重数据");
    
    // --- QMessageBox 按钮 ---
    // m_stringMap["msgbox.delete"] = tr("删除");
    // m_stringMap["msgbox.hide"]   = tr("隐藏");
    // m_stringMap["msgbox.cancel"] = tr("取消");
    // m_stringMap["msgbox.title.sciDavis"] = tr("叶组分析");
    // m_stringMap["msgbox.text.hide_or_delete"] = tr("你想隐藏或删除吗？") + "<p><b>'%1'</b> ?";

    m_stringMap["Delete"] = tr("删除");
    m_stringMap["Hide"]   = tr("隐藏");
    m_stringMap["Cancel"] = tr("取消");
    m_stringMap["SciDAVis"] = tr("叶组分析");
    m_stringMap["Do you want to hide or delete"] = tr("你想隐藏或删除吗？") + "<p><b>'%1'</b> ?";



}

// void StringManager::reloadStrings()
// {
//     loadStrings();
//     emit stringsUpdated();
// }

// void StringManager::setSamplesForType(const QString& dataType, const QSet<int>& sampleIds)
// {
//     m_samplesByType[dataType] = sampleIds;
//     emit samplesUpdated(dataType);
// }

// void StringManager::addSample(const QString& dataType, int sampleId)
// {
//     QSet<int>& set = m_samplesByType[dataType];
//     if (!set.contains(sampleId)) {
//         set.insert(sampleId);
//         emit samplesUpdated(dataType);
//     }
// }

// void StringManager::removeSample(const QString& dataType, int sampleId)
// {
//     QSet<int>& set = m_samplesByType[dataType];
//     if (set.contains(sampleId)) {
//         set.remove(sampleId);
//         emit samplesUpdated(dataType);
//     }
// }

// QSet<int> StringManager::samplesByType(const QString& dataType) const
// {
//     return m_samplesByType.value(dataType);
// }
