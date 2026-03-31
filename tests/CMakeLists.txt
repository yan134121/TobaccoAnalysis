# 色谱 MATLAB 流程自洽测试（可选目标）
# Curve.h 依赖 QColor/QPen（Gui）与 qcustomplot.h（Widgets + PrintSupport）
find_package(Qt5 COMPONENTS Gui Widgets PrintSupport REQUIRED)

set(_TA_SRC "${CMAKE_SOURCE_DIR}/src")

add_executable(chromatogram_matlab_parity_test
    "${CMAKE_CURRENT_SOURCE_DIR}/chromatogram_matlab_parity_test.cpp"
    "${_TA_SRC}/core/entities/Curve.cpp"
    "${_TA_SRC}/utils/logger.cpp"
    "${_TA_SRC}/services/algorithm/PeakSegCOWAlignment.cpp"
    "${_TA_SRC}/services/algorithm/ChromatogramSegmentFinetune.cpp"
    "${_TA_SRC}/services/algorithm/ChromatogramDifferenceMetrics.cpp"
)

target_include_directories(chromatogram_matlab_parity_test PRIVATE
    "${_TA_SRC}"
    "${_TA_SRC}/core"
    "${_TA_SRC}/core/entities"
    "${_TA_SRC}/utils"
    "${_TA_SRC}/services"
    "${_TA_SRC}/services/algorithm"
    "${_TA_SRC}/services/algorithm/processing"
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/third_party/qcustomplot"
)

target_link_libraries(chromatogram_matlab_parity_test PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::PrintSupport
)
