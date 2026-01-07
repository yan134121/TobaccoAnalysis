#include "DictionaryOptionService.h"
#include <QDebug>
#include <QSqlError>

#include "data_access/DatabaseConnector.h"
#include "Logger.h"

DictionaryOptionService::DictionaryOptionService(DictionaryOptionDAO* dao, QObject *parent)
    : QObject(parent), m_dao(dao)
{
    // if (!m_dao) { FATAL_LOG << "DictionaryOptionService: DictionaryOptionDAO not injected!"; }
    if (!m_dao) { 
        FATAL_LOG << "DictionaryOptionService: DictionaryOptionDAO not injected!"; 
    } else {
        DEBUG_LOG << "DictionaryOptionService: DAO injected successfully!";
    }
    DEBUG_LOG << "m_dao pointer: " << m_dao;
}

QStringList DictionaryOptionService::getOptionsByCategory(const QString& category)
{
    // DEBUG_LOG << "getOptionsByCategory called with category:" << category;

    // if (m_dao) {
    //     DEBUG_LOG << "DAO pointer is valid:" << m_dao;
    // } else {
    //     WARNING_LOG << "DAO pointer is null!";
    //     return QStringList();
    // }

    return m_dao->getValuesByCategory(category);
}


// bool DictionaryOptionService::addOption(const QString& category, const QString& value, const QString& description, QString& errorMessage)
// bool DictionaryOptionService::addOption(const QString& category, const QString& value, QString& errorMessage, const QString& description)
// {
//     if (value.trimmed().isEmpty()) {
//         errorMessage = "选项值不能为空。"; return false;
//     }
//     if (m_dao->exists(category, value.trimmed())) {
//         errorMessage = "该选项已存在。"; return false;
//     }

//     DictionaryOption option;
//     option.setCategory(category);
//     option.setValue(value.trimmed());
//     option.setDescription(description);

//     if (m_dao->insert(option)) {
//         DEBUG_LOG << "Added new option:" << category << ":" << value;
//         return true;
//     } else {
//         errorMessage = "添加选项失败: " + DatabaseConnector::getInstance().getDatabase().lastError().text();
//         WARNING_LOG << errorMessage; return false;
//     }
// }


bool DictionaryOptionService::addOption(const QString& category, const QString& value, QString& errorMessage, const QString& description)
{
    // 1. 验证输入值是否为空
    if (value.trimmed().isEmpty()) {
        errorMessage = "选项值不能为空。";
        return false;
    }

    // 2. 检查选项是否已存在
    if (m_dao->exists(category, value.trimmed())) {
        // 选项已存在，视为成功（幂等操作），无需重复添加
        // 不需要设置 errorMessage，因为这不是一个错误情况
        DEBUG_LOG << "Option '" << value.trimmed() << "' already exists in category '" << category << "'. Skipping addition.";
        errorMessage.clear(); // 确保错误信息是空的
        return true;
    }

    // 3. 如果不存在，则创建新的字典选项并插入数据库
    DictionaryOption option;
    option.setCategory(category);
    option.setValue(value.trimmed()); // 确保存储 trimmed 的值
    option.setDescription(description);

    if (m_dao->insert(option)) {
        DEBUG_LOG << "Added new option:" << category << ":" << value.trimmed();
        errorMessage.clear(); // 成功，清空错误信息
        return true;
    } else {
        errorMessage = "添加选项失败: " + DatabaseConnector::getInstance().getDatabase().lastError().text(); // <-- DAO 错误信息
        WARNING_LOG << errorMessage;
        return false;
    }
}


bool DictionaryOptionService::removeOption(int id, QString& errorMessage)
{
    if (m_dao->remove(id)) {
        DEBUG_LOG << "Removed option with ID:" << id; return true;
    } else {
        errorMessage = "删除选项失败: " + DatabaseConnector::getInstance().getDatabase().lastError().text();
        WARNING_LOG << errorMessage; return false;
    }
}
