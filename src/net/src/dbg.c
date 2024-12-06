#include "dbg.h"

#include <sys/unistd.h>
#include "sys_plat.h"
#include <stdarg.h>

const char* get_style(int s_level){
    const char* style;
    switch (s_level) {
        case DBG_LEVEL_ERROR:
            style = DBG_STYLE_ERROR;
        break;
        case DBG_LEVEL_WARN:
            style = DBG_STYLE_WARN;
        break;
        case DBG_LEVEL_INFO:
            style = DBG_STYLE_INFO;
        break;
        default:
            style = DBG_STYLE_RESET;
        break;
    }
    return style;
}

static const char *color[] = {
    [DBG_LEVEL_NONE] = "none",
    [DBG_LEVEL_ERROR] = DBG_STYLE_ERROR"error: ",
    [DBG_LEVEL_WARN] = DBG_STYLE_WARN"warning: ",
    [DBG_LEVEL_INFO] = DBG_STYLE_INFO"info: ",
};

void dbg_print(int debugger_module_level, int s_level, const char* file, const char* func, int line, const char* fmt, ...){
    if (debugger_module_level < s_level) {
        return;
    }

    //file 是字符串的起始地址。
    //加上 plat_strlen(file) 的长度，就移动指针到字符串末尾，即 '\0' 所在的位置。
    //通过将 file 起始地址加上字符串长度，指针 end 指向字符串的末尾（'\0' 的位置）。
    const char* end = file + plat_strlen(file);
    while (end >= file) {
        if (*end == '\\' || *end == '/') {
            break;
        }
        end--;
    }
    end++;


    //const char* style_default = DBG_STYLE_RESET;
    //const char* style = get_style(s_level);
    plat_printf("%s", color[s_level]);
    plat_printf("(%s-%s-%d):",end, func, line);

    char str_buf[256];
    va_list args;
    va_start(args, fmt);
    plat_vsprintf(str_buf, fmt, args);
    plat_printf("%s\n", str_buf);
    plat_printf(""DBG_STYLE_RESET);
    va_end(args);
}


