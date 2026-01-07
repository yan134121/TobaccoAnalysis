#include "Logger.h"

// #include "StackTraceUtils.h"
// #include <QDebug>
#include "Logger.h"

// #ifdef Q_OS_WIN
//     #include <windows.h>
// #endif


// 只有堆栈地址没有函数名
// void PrintStackTrace()
// {
// #ifdef Q_OS_WIN
//     void *stack[100];
//     USHORT frames = CaptureStackBackTrace(0, 100, stack, NULL);

//     // qDebug() << "---- CALL STACK BEGIN ----";
//     DEBUG_LOG << "---- CALL STACK BEGIN ----";
//     for (USHORT i = 0; i < frames; i++) {
//         DEBUG_LOG << "  #" << i << stack[i];
//     }
//     DEBUG_LOG << "---- CALL STACK END ----";
// #else
//     // qDebug() << "Stack trace only supported on Windows.";
//     DEBUG_LOG << "Stack trace only supported on Windows.";
// #endif
// }


// qDebug也可能是DEBUG_LOGd的原因导致现在打印不了，使用DEBUG_LOG有识别不了char
// #include "StackTraceUtils.h"
// #include <QDebug>

// #ifdef Q_OS_WIN
// #include <windows.h>
// #include <dbghelp.h>
// #pragma comment(lib, "dbghelp.lib")
// #endif

// void PrintStackTrace()
// {
// #ifdef Q_OS_WIN
//     HANDLE process = GetCurrentProcess();
//     SymInitialize(process, NULL, TRUE);

//     void* stack[100];
//     USHORT frames = CaptureStackBackTrace(0, 100, stack, NULL);

//     // qDebug() << "---- CALL STACK BEGIN ----";
//     DEBUG_LOG << "---- CALL STACK BEGIN ----";

//     for (USHORT i = 0; i < frames; i++) {
//         DWORD64 address = (DWORD64)(stack[i]);

//         char buffer[sizeof(SYMBOL_INFO) + 256];
//         PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
//         symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
//         symbol->MaxNameLen = 255;

//         DWORD64 displacement = 0;

//         if (SymFromAddr(process, address, &displacement, symbol)) {
//             // qDebug() << "#" << i << symbol->Name << "+0x" << QString::number(displacement, 16);
//             DEBUG_LOG << "#" << i << symbol->Name << "+0x" << QString::number(displacement, 16);
//         } else {
//             // qDebug() << "#" << i << "0x" << QString::number(address, 16);
//             DEBUG_LOG << "#" << i << "0x" << QString::number(address, 16);
//         }
//     }

//     // qDebug() << "---- CALL STACK END ----";
//     DEBUG_LOG << "---- CALL STACK END ----";
// #else
//     // qDebug() << "Stacktrace only available on Windows.";
//     DEBUG_LOG << "Stacktrace only available on Windows.";
// #endif
// }



// 目前只能显示栈地址，没有函数名，先不使用了
#include "StackTraceUtils.h"
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

void PrintStackTrace()
{
// #ifdef Q_OS_WIN
//     HANDLE process = GetCurrentProcess();
//     SymInitialize(process, NULL, TRUE);

//     void* stack[100];
//     USHORT frames = CaptureStackBackTrace(0, 100, stack, NULL);

//     DEBUG_LOG << "---- CALL STACK BEGIN ----";

//     for (USHORT i = 0; i < frames; i++) {
//         DWORD64 address = (DWORD64)(stack[i]);

//         char buffer[sizeof(SYMBOL_INFO) + 256];
//         PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
//         symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
//         symbol->MaxNameLen = 255;

//         DWORD64 displacement = 0;

//         if (SymFromAddr(process, address, &displacement, symbol)) {

//             // 关键修改：把 char[] 转为 QString
//             QString name = QString::fromLatin1(symbol->Name);

//             DEBUG_LOG << "#" << i 
//                       << name 
//                       << "+0x" 
//                       << QString::number(displacement, 16);
//         } else {
//             DEBUG_LOG << "#" << i 
//                       << "0x" 
//                       << QString::number(address, 16);
//         }
//     }

//     DEBUG_LOG << "---- CALL STACK END ----";

// #else
//     DEBUG_LOG << "Stacktrace only available on Windows.";
// #endif
}

