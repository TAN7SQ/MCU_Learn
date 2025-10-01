/**
 * @file elab_def.h
 * @author ZC (387646983@qq.com)
 * @brief 
 * @version 0.1
 * @date 2025-08-17
 * 
 * 
 */

 
 #ifndef ELAB_DEF_H 
 #define ELAB_DEF_H 
 
 /* ==================== [Includes] ========================================== */
 
 #include "elab_std.h"
 
 #ifdef __cplusplus
 extern "C"{
 #endif
 
 /* ==================== [Defines] ========================================== */
 /**Complier */
#if defined(__CC_ARM) || defined(__CLANG_ARM) /* ARM Compiler */

    #include <stdarg.h>
    #define ELAB_SECTION(x)             __attribute__((section(x)))
    #define ELAB_USED                   __attribute__((used))
    #define ELAB_ALIGN(n)               __attribute__((aligned(n)))
    #define ELAB_WEAK                   __attribute__((weak))
    #define elab_inline                 static __inline
    #define elab_likely(x)              (x)
    #define elab_unlikely(x)            (x)
#elif defined (__IAR_SYSTEMS_ICC__)           /* for IAR Compiler */

    #include <stdarg.h>
    #define ELAB_SECTION(x)             @ x
    #define ELAB_USED                   __root
    #define ELAB_PRAGMA(x)              _Pragma(#x)
    #define ELAB_ALIGN(n)               ELAB_PRAGMA(data_alignment=n)
    #define ELAB_WEAK                   __weak
    #define elab_inline                 static inline
    #define elab_likely(x)              (x)
    #define elab_unlikely(x)            (x)
#elif defined (__GNUC__)                      /* GNU GCC Compiler */
    #include <stdarg.h>
    #define ELAB_SECTION(x)             __attribute__((section(x)))
    #define ELAB_USED                   __attribute__((used))
    #define ELAB_ALIGN(n)               __attribute__((aligned(n)))
    #define ELAB_WEAK                   __attribute__((weak))
    #define elab_inline                 static inline
    #define elab_likely(x)              __builtin_expect(!!(x), 1)
    #define elab_unlikely(x)            __builtin_expect(!!(x), 0)
#elif defined(__TASKING__)                  /* Tasking Compiler for AURIX */

    #include <stdarg.h>
    #define ELAB_USED                   __attribute__((protect))  //infineon must use protect to keep seciton
    #define ELAB_ALIGN(n)               __attribute__((aligned(n)))
    #define ELAB_WEAK                   __attribute__((weak))
    #define elab_inline                 static inline
    #define elab_likely(x)              (x)
    #define elab_unlikely(x)            (x)

#else
    #error The current compiler is NOT supported!
#endif

#if defined(__x86_64__) || defined(__aarch64__)
typedef int64_t                         elab_pointer_t;
#elif defined(__i386__) || defined(__arm__)
typedef int32_t                         elab_pointer_t;
#elif defined(__TASKING__)
typedef int32_t                         elab_pointer_t;
#else
    #error The currnet CPU is NOT supported!
#endif


 /* ==================== [Typedefs] ========================================== */
 /* public typedef ----------------------------------------------------------- */
typedef enum elab_err
{
    ELAB_OK                             = 0,
    ELAB_ERROR                          = -1,
    ELAB_ERR_EMPTY                      = -2,
    ELAB_ERR_FULL                       = -3,
    ELAB_ERR_TIMEOUT                    = -4,
    ELAB_ERR_BUSY                       = -5,
    ELAB_ERR_NO_MEMORY                  = -6,
    ELAB_ERR_IO                         = -7,
    ELAB_ERR_INVALID                    = -8,
    ELAB_ERR_MEM_OVERLAY                = -9,
    ELAB_ERR_MALLOC                     = -10,
    ELAB_ERR_NOT_ENOUGH                 = -11,
    ELAB_ERR_NO_SYSTEM                  = -12,
    ELAB_ERR_BUS                        = -13,
} elab_err_t;
 /* ==================== [Global Prototypes] ================================== */
 
 /* ==================== [Macros] ============================================ */
 // 指定宽度向上对齐
#define ELAB_SIZE_ALIGN(size, align) (((size) + (align - 1)) & ~(align - 1))

// 指定宽度向下对齐
#define ELAB_SIZE_ALIGN_DOWN(size, align) ((size) & ~(align - 1))


 #ifdef __cplusplus
 }
 #endif
 
 #endif	/* ELAB_DEF_H */
