// // src/entity/SingleMaterialTobacco.cpp
// #include "SingleMaterialTobacco.h" // <-- 只包含头文件

// SingleMaterialTobacco::SingleMaterialTobacco()
//     : m_id(-1), m_year(0) // 默认值
// {
//     // 可以在这里设置其他成员变量的默认值
//     m_code = QString();
//     m_origin = QString();
//     m_part = QString();
//     m_grade = QString();
//     m_type = QString();
//     m_collectDate = QDate();
//     m_detectDate = QDate();
//     m_dataJson = QJsonDocument();
//     m_createdAt = QDateTime();
// }

// SingleMaterialTobacco::SingleMaterialTobacco(int id, const QString& code, int year, const QString& origin,
//                                              const QString& part, const QString& grade, const QString& type,
//                                              const QDate& collectDate, const QDate& detectDate,
//                                              const QJsonDocument& dataJson, const QDateTime& createdAt)
//     : m_id(id),
//       m_code(code),
//       m_year(year),
//       m_origin(origin),
//       m_part(part),
//       m_grade(grade),
//       m_type(type),
//       m_collectDate(collectDate),
//       m_detectDate(detectDate),
//       m_dataJson(dataJson),
//       m_createdAt(createdAt)
// {
// }

// int SingleMaterialTobacco::getId() const { return m_id; }
// QString SingleMaterialTobacco::getCode() const { return m_code; }
// int SingleMaterialTobacco::getYear() const { return m_year; }
// QString SingleMaterialTobacco::getOrigin() const { return m_origin; }
// QString SingleMaterialTobacco::getPart() const { return m_part; }
// QString SingleMaterialTobacco::getGrade() const { return m_grade; }
// QString SingleMaterialTobacco::getType() const { return m_type; }
// QDate SingleMaterialTobacco::getCollectDate() const { return m_collectDate; }
// QDate SingleMaterialTobacco::getDetectDate() const { return m_detectDate; }
// QJsonDocument SingleMaterialTobacco::getDataJson() const { return m_dataJson; }
// QDateTime SingleMaterialTobacco::getCreatedAt() const { return m_createdAt; }

// void SingleMaterialTobacco::setId(int newId) { m_id = newId; }
// void SingleMaterialTobacco::setCode(const QString& newCode) { m_code = newCode; }
// void SingleMaterialTobacco::setYear(int newYear) { m_year = newYear; }
// void SingleMaterialTobacco::setOrigin(const QString& newOrigin) { m_origin = newOrigin; }
// void SingleMaterialTobacco::setPart(const QString& newPart) { m_part = newPart; }
// void SingleMaterialTobacco::setGrade(const QString& newGrade) { m_grade = newGrade; }
// void SingleMaterialTobacco::setType(const QString& newType) { m_type = newType; }
// void SingleMaterialTobacco::setCollectDate(const QDate& newCollectDate) { m_collectDate = newCollectDate; }
// void SingleMaterialTobacco::setDetectDate(const QDate& newDetectDate) { m_detectDate = newDetectDate; }
// void SingleMaterialTobacco::setDataJson(const QJsonDocument& newDataJson) { m_dataJson = newDataJson; }
// void SingleMaterialTobacco::setCreatedAt(const QDateTime& newCreatedAt) { m_createdAt = newCreatedAt; }


// 建议文件名: src/entity/SingleTobaccoSampleData.cpp
#include "SingleTobaccoSampleData.h" // 修改: 包含对应的头文件

// 修改: 默认构造函数更新
SingleTobaccoSampleData::SingleTobaccoSampleData()
    : m_id(-1),
      m_batchId(0),
      m_parallelNo(0),
      m_year(0)
{
    // 初始化其他成员为默认空值
    m_projectName = QString();
    m_shortCode = QString();
    m_sampleName = QString();
    m_note = QString();
    m_origin = QString();
    m_part = QString();
    m_grade = QString();
    m_type = QString();
    m_collectDate = QDate();
    m_detectDate = QDate();
    m_dataJson = QJsonDocument();
    m_createdAt = QDateTime();
}

// 修改: 带参数的构造函数完全更新
SingleTobaccoSampleData::SingleTobaccoSampleData(int id, int batchId, const QString& projectName, const QString& batchCode,
                                          const QString& shortCode, int parallelNo, const QString& sampleName, const QString& note,
                                          int year, const QString& origin, const QString& part, const QString& grade,
                                          const QString& type, const QDate& collectDate, const QDate& detectDate,
                                          const QJsonDocument& dataJson, const QDateTime& createdAt)
    : m_id(id),
      m_batchId(batchId),
      m_projectName(projectName),
      m_batchCode(batchCode),
      m_shortCode(shortCode),
      m_parallelNo(parallelNo),
      m_sampleName(sampleName),
      m_note(note),
      m_year(year),
      m_origin(origin),
      m_part(part),
      m_grade(grade),
      m_type(type),
      m_collectDate(collectDate),
      m_detectDate(detectDate),
      m_dataJson(dataJson),
      m_createdAt(createdAt)
{
}

// --- Getters ---
int SingleTobaccoSampleData::getId() const { return m_id; }
// 新增 Getters 实现
int SingleTobaccoSampleData::getBatchId() const { return m_batchId; }
QString SingleTobaccoSampleData::getProjectName() const { return m_projectName; }
QString SingleTobaccoSampleData::getBatchCode() const { return m_batchCode; }
QString SingleTobaccoSampleData::getShortCode() const { return m_shortCode; }
int SingleTobaccoSampleData::getParallelNo() const { return m_parallelNo; }
QString SingleTobaccoSampleData::getSampleName() const { return m_sampleName; }
QString SingleTobaccoSampleData::getNote() const { return m_note; }
// 保留的 Getters 实现
int SingleTobaccoSampleData::getYear() const { return m_year; }
QString SingleTobaccoSampleData::getOrigin() const { return m_origin; }
QString SingleTobaccoSampleData::getPart() const { return m_part; }
QString SingleTobaccoSampleData::getGrade() const { return m_grade; }
QString SingleTobaccoSampleData::getType() const { return m_type; }
QDate SingleTobaccoSampleData::getCollectDate() const { return m_collectDate; }
QDate SingleTobaccoSampleData::getDetectDate() const { return m_detectDate; }
QJsonDocument SingleTobaccoSampleData::getDataJson() const { return m_dataJson; }
QDateTime SingleTobaccoSampleData::getCreatedAt() const { return m_createdAt; }

// --- Setters ---
void SingleTobaccoSampleData::setId(int newId) { m_id = newId; }
// 新增 Setters 实现
void SingleTobaccoSampleData::setBatchId(int newBatchId) { m_batchId = newBatchId; }
void SingleTobaccoSampleData::setProjectName(const QString& newProjectName) { m_projectName = newProjectName; }
void SingleTobaccoSampleData::setBatchCode(const QString& newBatchCode) { m_batchCode = newBatchCode; }
void SingleTobaccoSampleData::setShortCode(const QString& newShortCode) { m_shortCode = newShortCode; }
void SingleTobaccoSampleData::setParallelNo(int newParallelNo) { m_parallelNo = newParallelNo; }
void SingleTobaccoSampleData::setSampleName(const QString& newSampleName) { m_sampleName = newSampleName; }
void SingleTobaccoSampleData::setNote(const QString& newNote) { m_note = newNote; }
// 保留的 Setters 实现
void SingleTobaccoSampleData::setYear(int newYear) { m_year = newYear; }
void SingleTobaccoSampleData::setOrigin(const QString& newOrigin) { m_origin = newOrigin; }
void SingleTobaccoSampleData::setPart(const QString& newPart) { m_part = newPart; }
void SingleTobaccoSampleData::setGrade(const QString& newGrade) { m_grade = newGrade; }
void SingleTobaccoSampleData::setType(const QString& newType) { m_type = newType; }
void SingleTobaccoSampleData::setCollectDate(const QDate& newCollectDate) { m_collectDate = newCollectDate; }
void SingleTobaccoSampleData::setDetectDate(const QDate& newDetectDate) { m_detectDate = newDetectDate; }
void SingleTobaccoSampleData::setDataJson(const QJsonDocument& newDataJson) { m_dataJson = newDataJson; }
void SingleTobaccoSampleData::setCreatedAt(const QDateTime& newCreatedAt) { m_createdAt = newCreatedAt; }
