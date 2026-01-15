# CMake 最低版本要求
cmake_minimum_required(VERSION 3.16)

# --- 设置默认构建类型 ---
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

# 项目名称和语言
project(TobaccoAnalysis VERSION 1.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用 Qt 的一些 CMake 功能 (自动处理 MOC, UIC, RCC)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# --- 查找依赖 ---
# 确保包含了所有需要的 Qt 模块
find_package(Qt5 COMPONENTS Core Gui Widgets Sql Charts Xml Concurrent REQUIRED)
# Qt Charts 模块包含 PrintSupport，QCustomPlot 需要它
find_package(Qt5 COMPONENTS PrintSupport)
message(STATUS "Found Qt version ${Qt5_VERSION} at ${Qt5_DIR}")


message(STATUS "PROJECT_SOURCE_DIR: ${PROJECT_SOURCE_DIR}")
message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

# --- 收集所有源文件 ---
# 定义 src 目录的路径，方便引用
set(PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 使用 file(GLOB_RECURSE ...) 自动查找指定目录下的所有匹配文件。
# 这意味着，只要您在这些目录中添加了新的 .h, .cpp, .ui 文件，
# 下次重新运行 cmake 配置命令时，它们就会被自动包含进来。

# Core: 包含 core/ 及其所有子目录 (entities, models, singletons)
file(GLOB_RECURSE CORE_FILES 
    "${PROJECT_SOURCE_DIR}/core/*.h" 
    "${PROJECT_SOURCE_DIR}/core/*.cpp" 
    # "${PROJECT_SOURCE_DIR}/core/*/*.cpp" 这种方式包含不了子文件夹
    # "${PROJECT_SOURCE_DIR}/core/*/*.h"
    "${PROJECT_SOURCE_DIR}/core/entities/*.cpp"
    "${PROJECT_SOURCE_DIR}/core/entities/*.h"
    "${PROJECT_SOURCE_DIR}/core/models/.cpp"
    "${PROJECT_SOURCE_DIR}/core/models/.h"
    "${PROJECT_SOURCE_DIR}/core/singletons/*.cpp"  # <-- 关键
    "${PROJECT_SOURCE_DIR}/core/singletons/*.h"
    "${PROJECT_SOURCE_DIR}/core/sql/*.cpp"  
    "${PROJECT_SOURCE_DIR}/core/sql/*.h"
)

# Data Access: 包含 data_access/ 目录下的所有文件
file(GLOB_RECURSE DATA_ACCESS_FILES 
    "${PROJECT_SOURCE_DIR}/data_access/*.cpp" 
    "${PROJECT_SOURCE_DIR}/data_access/*.h"
    "${PROJECT_SOURCE_DIR}/data_access/dao/*.cpp" 
    "${PROJECT_SOURCE_DIR}/data_access/dao/*.h"
)

# Services: 包含 services/ 目录及其子目录下的所有文件
file(GLOB_RECURSE SERVICE_FILES 
    "${PROJECT_SOURCE_DIR}/services/*.cpp" 
    "${PROJECT_SOURCE_DIR}/services/*.h"
    "${PROJECT_SOURCE_DIR}/services/data_import/*.cpp"
    "${PROJECT_SOURCE_DIR}/services/data_import/*.h"
    "${PROJECT_SOURCE_DIR}/services/algorithm/*.cpp"
    "${PROJECT_SOURCE_DIR}/services/algorithm/*.h"
    "${PROJECT_SOURCE_DIR}/services/algorithm/processing/*.cpp"
    "${PROJECT_SOURCE_DIR}/services/algorithm/processing/*.h"
    "${PROJECT_SOURCE_DIR}/services/analysis/*.cpp"
    "${PROJECT_SOURCE_DIR}/services/analysis/*.h"
)


# # GUI: 包含 gui/ 及其所有子目录 (dialogs, views, widgets)
file(GLOB_RECURSE GUI_FILES 
    "${PROJECT_SOURCE_DIR}/gui/*.cpp" 
    "${PROJECT_SOURCE_DIR}/gui/*.h" 
    "${PROJECT_SOURCE_DIR}/gui/*/*.cpp" 
    "${PROJECT_SOURCE_DIR}/gui/*/*.h" 
    "${PROJECT_SOURCE_DIR}/gui/*/*.ui"
    "${PROJECT_SOURCE_DIR}/gui/*.ui"
    "${PROJECT_SOURCE_DIR}/gui/dialogs/*.cpp" 
    "${PROJECT_SOURCE_DIR}/gui/dialogs/*.h" 
    "${PROJECT_SOURCE_DIR}/gui/forms_data/*.cpp" 
    "${PROJECT_SOURCE_DIR}/gui/forms_data/*.h" 
    "${PROJECT_SOURCE_DIR}/gui/forms_data/*.ui" 
    "${PROJECT_SOURCE_DIR}/gui/ui_data/*.cpp" 
    "${PROJECT_SOURCE_DIR}/gui/ui_data/*.h"
    "${PROJECT_SOURCE_DIR}/gui/ui_data/*.ui"
    "${PROJECT_SOURCE_DIR}/gui/ui_data/charts/*.cpp" 
    "${PROJECT_SOURCE_DIR}/gui/ui_data/charts/*.h"
    "${PROJECT_SOURCE_DIR}/gui/ui_data/charts/*.ui"
    "${PROJECT_SOURCE_DIR}/gui/ui_data/dialogs/*.cpp" 
    "${PROJECT_SOURCE_DIR}/gui/ui_data/dialogs/*.h"
    "${PROJECT_SOURCE_DIR}/gui/ui_data/dialogs/*.ui"
    "${PROJECT_SOURCE_DIR}/gui/views/workbenches/*.cpp"
    "${PROJECT_SOURCE_DIR}/gui/views/workbenches/*.h"
    "${PROJECT_SOURCE_DIR}/gui/views/workbenches/*.ui"
    "${PROJECT_SOURCE_DIR}/gui/delegates/*.cpp"
    "${PROJECT_SOURCE_DIR}/gui/delegates/*.h"
)

# 显式添加对话框文件，确保它们被包含在编译中
set(DIALOG_FILES
    "${PROJECT_SOURCE_DIR}/gui/dialogs/SamplePropertiesDialog.cpp"
    "${PROJECT_SOURCE_DIR}/gui/dialogs/SamplePropertiesDialog.h"
    "${PROJECT_SOURCE_DIR}/gui/dialogs/SampleDataTableDialog.cpp"
    "${PROJECT_SOURCE_DIR}/gui/dialogs/SampleDataTableDialog.h"
    "${PROJECT_SOURCE_DIR}/gui/dialogs/SampleListDialog.cpp"
    "${PROJECT_SOURCE_DIR}/gui/dialogs/SampleListDialog.h"
    "${PROJECT_SOURCE_DIR}/gui/dialogs/samplelistdialog.ui"
)
# ExcelPreviewDialog 文件（新添加）
list(APPEND DIALOG_FILES
    "${PROJECT_SOURCE_DIR}/gui/dialogs/ExcelPreviewDialog.cpp"
    "${PROJECT_SOURCE_DIR}/gui/dialogs/ExcelPreviewDialog.h"
)




# utils: 包含 utils/ 及其所有子目录 
file(GLOB_RECURSE UTIL_FILES 
    "${PROJECT_SOURCE_DIR}/utils/*.cpp" 
    "${PROJECT_SOURCE_DIR}/utils/*.h" 
    "${PROJECT_SOURCE_DIR}/utils/*/*.cpp" 
    "${PROJECT_SOURCE_DIR}/utils/*/*.h" 
    "${PROJECT_SOURCE_DIR}/utils/util_data/*.cpp" 
    "${PROJECT_SOURCE_DIR}/utils/util_data/*.h" 
)


# Resources: 包含 resources/ 目录下的 .qrc 文件
file(GLOB_RECURSE RESOURCE_FILES 
    "${PROJECT_SOURCE_DIR}/resources/*.qrc"
)

# 添加资源文件
set(APP_ICON_RC "${CMAKE_CURRENT_SOURCE_DIR}/app_icon.rc")

# (为未来预留) Services 和 Utils
# file(GLOB_RECURSE SERVICE_FILES "${PROJECT_SOURCE_DIR}/services/*.cpp" "${PROJECT_SOURCE_DIR}/services/*.h")
# file(GLOB_RECURSE UTIL_FILES "${PROJECT_SOURCE_DIR}/utils/*.cpp" "${PROJECT_SOURCE_DIR}/utils/*.h")

# 添加 QCustomPlot 的源文件
set(QCUSTOMPLOT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/qcustomplot/qcustomplot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/qcustomplot/qcustomplot.h
)


# WIN32 告诉 CMake 这是一个 GUI 应用程序，会自动避免在启动时弹出命令行窗口。
# --- 定义可执行文件 ---
add_executable(${PROJECT_NAME} WIN32
    # 程序入口
    ${PROJECT_SOURCE_DIR}/main.cpp
    # ${PROJECT_SOURCE_DIR}/core/singletons/StringManager.cpp  # <-- 关键
    
    # 添加所有收集到的文件列表
    ${CORE_FILES}
    ${DATA_ACCESS_FILES}
    ${GUI_FILES}
    ${DIALOG_FILES}
    ${RESOURCE_FILES}
    ${QCUSTOMPLOT_SOURCES}
    ${SERVICE_FILES}
    ${UTIL_FILES}
    ${APP_ICON_RC}
)


add_subdirectory(third_party/QXlsx)
target_link_libraries(${PROJECT_NAME} PRIVATE QXlsx::QXlsx)

# --- 链接库 ---
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Sql
    Qt5::PrintSupport # 【关键】链接 PrintSupport 库
    Qt5::Charts
    Qt5::Xml
    Qt5::Concurrent
)

# 添加 src 作为头文件查找路径
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/core
    ${PROJECT_SOURCE_DIR}/core/entities
    ${PROJECT_SOURCE_DIR}/core/models
    ${PROJECT_SOURCE_DIR}/core/singletons
    ${PROJECT_SOURCE_DIR}/core/algorithm
    ${PROJECT_SOURCE_DIR}/core/sql
    ${PROJECT_SOURCE_DIR}/data_access
    ${PROJECT_SOURCE_DIR}/data_access/dao
    ${PROJECT_SOURCE_DIR}/services
    ${PROJECT_SOURCE_DIR}/services/data_import
    ${PROJECT_SOURCE_DIR}/services/algorithm
    ${PROJECT_SOURCE_DIR}/services/algorithm/processing
    ${PROJECT_SOURCE_DIR}/gui
    ${PROJECT_SOURCE_DIR}/gui/views
    ${PROJECT_SOURCE_DIR}/gui/dialogs
    ${PROJECT_SOURCE_DIR}/gui/widgets
    ${PROJECT_SOURCE_DIR}/gui/forms_data
    ${PROJECT_SOURCE_DIR}/gui/ui_data
    ${PROJECT_SOURCE_DIR}/gui/ui_data/charts
    ${PROJECT_SOURCE_DIR}/gui/ui_data/dialogs
    ${PROJECT_SOURCE_DIR}/utils
    # ${PROJECT_SOURCE_DIR}/utils/util_data/
    ${PROJECT_SOURCE_DIR}/utils/util_data
    

    # third_party
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/qcustomplot
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/QXlsx

)






# --- 输出目录设置 ---
# 为 Debug 和 Release 版本设置不同的输出目录，便于管理
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/release)


if(WIN32)
    # 自动复制libmysql.dll到执行文件夹
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "D:/Program Files/MySQL/MySQL Server 8.0/lib/libmysql.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )

    # # --- 复制 config/config.json ---
    # add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    #     # COMMAND ${CMAKE_COMMAND} -E make_directory
    #     #     "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config"
    #     COMMAND ${CMAKE_COMMAND} -E copy_if_different
    #         "${CMAKE_CURRENT_SOURCE_DIR}/config/config.json"
    #         "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config.json"
    # )

    # --- 复制 sql 文件夹 ---
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        # COMMAND ${CMAKE_COMMAND} -E echo "Copying SQL folder..."
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/sql/"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/sql/"
    )

endif()

# 针对 Debug 模式生成 PDB 和符号
# if(WIN32)
#     foreach(flag_var
#             CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG
#             CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_RELWITHDEBINFO)
#         string(APPEND ${flag_var} " /Zi")  # 生成 PDB 调试信息
#     endforeach()

#     # 链接时生成完整 PDB
#     set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")
#     set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /DEBUG")
# endif()
if(MSVC)
    # MSVC 特有调试选项
    foreach(flag_var
            CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG
            CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        string(APPEND ${flag_var} " /Zi")
    endforeach()

    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /DEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO} /DEBUG")
endif()




# Windows 专用库
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE dbghelp)  # <-- 关键
endif()



# 定义配置文件源目录的路径，sql_config.json
set(CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/config)

# 检查源目录是否存在
if(EXISTS ${CONFIG_DIR})

    # 构建后自动复制 config 文件夹到可执行目录
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        ${CONFIG_DIR}                         # 源目录
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/config  # 目标目录
        COMMENT "Copying config directory to build folder..."
        VERBATIM
    )

endif()


# --- 自动部署外部可执行文件 ---

# # 定义外部工具源目录的路径,复制第三方exe文件到可执行文件夹
# set(EXTERNAL_BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# # 检查源目录是否存在
# if(EXISTS ${EXTERNAL_BIN_DIR})

#     # 为 Debug 版本添加构建后命令
#     add_custom_command(
#         TARGET ${PROJECT_NAME}       # 指定这个命令在哪个目标成功构建后执行
#         POST_BUILD                  # 指定在构建之后执行
#         COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different # CMake 内置的跨平台复制目录命令
#         ${EXTERNAL_BIN_DIR}         # 源目录
#         $<TARGET_FILE_DIR:${PROJECT_NAME}> # 目标目录 (这是 CMake 的一个特殊语法，代表目标的输出目录)
#         COMMENT "Copying external executables to build directory..."
#         VERBATIM
#     )

#     # 注意：上面的命令对所有配置都有效 (Debug, Release, etc.)
#     # 如果您想为不同配置复制不同文件，需要更复杂的逻辑，但目前这样已足够。

# endif()



# # --- 添加测试程序 ---
# # 定义测试程序的可执行文件
# add_executable(test_chromatograph_integration
#     test_chromatograph_integration.cpp
#     # 包含必要的源文件
#     ${CORE_FILES}
#     ${DATA_ACCESS_FILES}
#     ${SERVICE_FILES}
#     ${GUI_FILES}
#     ${QCUSTOMPLOT_SOURCES}
#     ${UTIL_FILES}
# )

# # 为测试程序链接库
# target_link_libraries(test_chromatograph_integration PRIVATE
#     Qt5::Core
#     Qt5::Gui
#     Qt5::Widgets
#     Qt5::Sql
#     Qt5::PrintSupport
#     Qt5::Charts
#     QXlsx::QXlsx
# )

# # 为测试程序添加头文件查找路径
# target_include_directories(test_chromatograph_integration PRIVATE
#     ${PROJECT_SOURCE_DIR}
#     ${PROJECT_SOURCE_DIR}/core
#     ${PROJECT_SOURCE_DIR}/core/entities
#     ${PROJECT_SOURCE_DIR}/core/models
#     ${PROJECT_SOURCE_DIR}/data_access
#     ${PROJECT_SOURCE_DIR}/gui
#     ${PROJECT_SOURCE_DIR}/gui/views
#     ${PROJECT_SOURCE_DIR}/gui/dialogs
#     ${PROJECT_SOURCE_DIR}/gui/widgets
#     ${PROJECT_SOURCE_DIR}/utils
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/third_party/qcustomplot
# )


# 只在 Debug 模式下开启 ASan
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Enabling AddressSanitizer")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
endif()
