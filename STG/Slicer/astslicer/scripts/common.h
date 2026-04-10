#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>          // 标准输入输出
#include <stdlib.h>         // 通用工具函数（内存、转换等）
#include <string.h>         // 字符串/内存操作（替代 memory.h）
#include <math.h>           // 数学运算
#include <ctype.h>          // 字符处理
#include <limits.h>         // 数据类型范围
#include <stddef.h>         // 标准定义（size_t 等）
#include <errno.h>          // 错误码
#include <time.h>           // 时间操作
#include <assert.h>         // 断言
#include <stdarg.h>         // 可变参数

// POSIX 系统调用头文件（Linux/Unix 环境）
#include <unistd.h>         // 基础系统调用（read/write/fork 等）
#include <fcntl.h>          // 文件控制（open/fcntl 等）
#include <sys/stat.h>       // 文件状态
#include <sys/types.h>      // 系统数据类型（pid_t/uid_t 等）
#include <sys/wait.h>       // 进程等待
#include <signal.h>         // 信号处理
#include <sys/time.h>       // 高精度时间
#include <sys/resource.h>   // 系统资源限制
#include <sys/utsname.h>    // 系统信息（主机名、内核版本等）

// 文件与目录操作
#include <dirent.h>         // 目录遍历
#include <sys/mman.h>       // 内存映射

// 网络编程
#include <sys/socket.h>     // 套接字基础
#include <netinet/in.h>     // Internet 协议（TCP/UDP 等）
#include <arpa/inet.h>      // IP 地址转换
#include <netdb.h>          // 域名解析
#include <sys/select.h>     // IO 多路复用（select）
#include <poll.h>           // IO 多路复用（poll）

// 多线程与同步
#include <pthread.h>        // POSIX 线程
#include <semaphore.h>      // 信号量

// // add by redis
// #include <libgen.h>
// #include <stdatomic.h>
// #include <dlfcn.h>
// #include <termios.h>
// // add by cpython
// #include <ffi.h>
// #include <locale.h>
// #include <wchar.h>
// // add by bzip
// #include <getopt.h>
// // add by libpng
// #include <setjmp.h>
// #include <zlib.h>

