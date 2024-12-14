#ifndef DBG_H
#define DBG_H

//#define EN_DEBUG
#include "cfg.h"
#include <stddef.h> // for offsetof

#define DBG_STYLE_ERROR "\033[1;31m" //red
#define DBG_STYLE_WARN "\033[1;33m" //yellow
#define DBG_STYLE_INFO "\033[1;32m" //green
#define DBG_STYLE_RESET "\033[1;0m" //reset

#define DBG_LEVEL_NONE 0
#define DBG_LEVEL_ERROR 1
#define DBG_LEVEL_WARN 2
#define DBG_LEVEL_INFO 3

void dbg_print(int debugger_module_level,int s_level,const char* file, const char* func, int line, const char* fmt, ...);

#define dbg_info(debugger_module_level,fmt,...) dbg_print(debugger_module_level,DBG_LEVEL_INFO,__FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define dbg_warning(debugger_module_level,fmt,...) dbg_print(debugger_module_level,DBG_LEVEL_WARN,__FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)
#define dbg_error(debugger_module_level,fmt,...) dbg_print(debugger_module_level,DBG_LEVEL_ERROR,__FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define dbg_assert(expr, msg) \
    do { \
        if (!(expr)) { \
            dbg_error(1, "Assertion failed: (%s), %s", #expr, msg); \
        } \
    } while (0)

/*	•	ptr：指向结构体中某个成员的指针（已有的成员指针）。
•
type：该成员所在的结构体类型名。
• member：成员在结构体中的名称。*/
#define container_of(ptr, type, member) \
((type *)((char *)(ptr) - offsetof(type, member)))

#define DEBUG_DISP_ENABLED(module)  (module>=DBG_LEVEL_INFO)
#endif // DBG_H