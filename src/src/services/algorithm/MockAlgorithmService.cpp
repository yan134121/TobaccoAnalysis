// src/algorithm/MockAlgorithmService.cpp
#include "MockAlgorithmService.h"

// 如果所有实现都在头文件中，这里通常可以为空，或者只包含必要的 `#include`
// Q_OBJECT 宏的 moc 文件生成通常在头文件，但其实现可能需要一个 .cpp 编译单元。
// CMake 的 AUTOMOC 会处理这一点，所以通常不需要在这里写什么。