// Public Domain. See "unlicense" statement at the end of this file.

#ifndef easy_util
#define easy_util

#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////////////////////////////////
// Options

#ifdef EASYUTIL_ONLY_ANNOTATIONS
    #define EASYUTIL_NO_MSVC_COMPAT
    #define EASYUTIL_NO_ALIGNED_MALLOC
    #define EASYUTIL_NO_MINMAXCLAMP
#endif

#ifdef EASYUTIL_ONLY_MSVC_COMPAT
    #define EASYUTIL_NO_ANNOTATIONS
    #define EASYUTIL_NO_ALIGNED_MALLOC
    #define EASYUTIL_NO_MINMAXCLAMP
#endif

#ifdef EASYUTIL_ONLY_ALIGNED_MALLOC
    #define EASYUTIL_NO_ANNOTATIONS
    #define EASYUTIL_NO_MSVC_COMPAT
    #define EASYUTIL_NO_MINMAXCLAMP
#endif


// Disable MSVC compatibility if we're compiling with it.
#if !defined(EASYUTIL_NO_MSVC_COMPAT) && defined(_MSC_VER)
    #define EASYUTIL_NO_MSVC_COMPAT
#endif

#if defined(_MSC_VER)
#define EASYUTIL_INLINE static __inline
#else
#define EASYUTIL_INLINE static inline
#endif


#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#ifndef EASYUTIL_NO_MSVC_COMPAT
#include <errno.h>
#endif


#define STRINGIFY(x)    #x
#define TOSTRING(x)     STRINGIFY(x)


/////////////////////////////////////////////////////////
// Annotations

#ifndef EASYUTIL_NO_ANNOTATIONS
    #ifndef IN
    #define IN
    #endif

    #ifndef OUT
    #define OUT
    #endif

    #ifndef UNUSED
    #define UNUSED(x) ((void)x)
    #endif

    #define PRIVATE
    #define PUBLIC
#endif


/////////////////////////////////////////////////////////
// min/max/clamp

#ifndef EASYUTIL_NO_MINMAXCLAMP
    #ifndef easy_min
    #define easy_min(x, y) (((x) < (y)) ? (x) : (y))
    #endif

    #ifndef easy_max
    #define easy_max(x, y) (((x) > (y)) ? (x) : (y))
    #endif

    #ifndef easy_clamp
    #define easy_clamp(x, low, high) (easy_max(low, easy_min(x, high)))
    #endif
#endif


/////////////////////////////////////////////////////////
// MSVC Compatibility

#ifndef EASYUTIL_NO_MSVC_COMPAT
/// A basic implementation of MSVC's strcpy_s().
EASYUTIL_INLINE int strcpy_s(char* dst, size_t dstSizeInBytes, const char* src)
{
    if (dst == 0) {
        return EINVAL;
    }
    if (dstSizeInBytes == 0) {
        return ERANGE;
    }
    if (src == 0) {
        dst[0] = '\0';
        return EINVAL;
    }
    
    char* iDst = dst;
    const char* iSrc = src;
    size_t remainingSizeInBytes = dstSizeInBytes;
    while (remainingSizeInBytes > 0 && iSrc[0] != '\0')
    {
        iDst[0] = iSrc[0];

        iDst += 1;
        iSrc += 1;
        remainingSizeInBytes -= 1;
    }

    if (remainingSizeInBytes > 0) {
        iDst[0] = '\0';
    } else {
        dst[0] = '\0';
        return ERANGE;
    }

    return 0;
}

// A wrapper for _stricmp/strcasecmp
EASYUTIL_INLINE int _stricmp(const char* string1, const char* string2)
{
    return strcasecmp(string1, string2);
}
#endif


/////////////////////////////////////////////////////////
// String Helpers

/// Removes every occurance of the given character from the given string.
void easyutil_strrmchar(char* str, char c);

/// Finds the first non-whitespace character in the given string.
const char* easyutil_first_non_whitespace(const char* str);

/// Finds the first occurance of a whitespace character in the given string.
const char* easyutil_first_whitespace(const char* str);


/////////////////////////////////////////////////////////
// Aligned Allocations

#ifndef EASYUTIL_NO_ALIGNED_MALLOC
EASYUTIL_INLINE void* aligned_malloc(size_t alignment, size_t size)
{
#if defined(_WIN32) || defined(_WIN64)
    return _aligned_malloc(size, alignment);
#else
    void* pResult;
    if (posix_memalign(&pResult, alignment, size) == 0) {
        return pResult;
    }
    
    return 0;
#endif
}

EASYUTIL_INLINE void aligned_free(void* ptr)
{
#if defined(_WIN32) || defined(_WIN64)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}
#endif  // !EASYUTIL_NO_ALIGNED_MALLOC



/////////////////////////////////////////////////////////
// Key/Value Pair Parsing

typedef unsigned int (* key_value_read_proc) (void* pUserData, void* pDataOut, unsigned int bytesToRead);
typedef void         (* key_value_pair_proc) (void* pUserData, const char* key, const char* value);
typedef void         (* key_value_error_proc)(void* pUserData, const char* message, unsigned int line);

/// Parses a series of simple Key/Value pairs.
///
/// @remarks
///     This function is suitable for parsing simple key/value config files.
///     @par
///     This function will never allocate memory on the heap. Because of this there is a minor restriction in the length of an individual
///     key/value pair which is 4KB.
///     @par
///     Formatting rules are as follows:
///      - The basic syntax for a key/value pair is [key][whitespace][value]. Example: MyProperty 1234
///      - All key/value pairs must be declared on a single line, and a single line cannot contain more than a single key/value pair.
///      - Comments begin with the '#' character and continue until the end of the line.
///      - A key cannot contain spaces but are permitted in values.
///      - The value will have any leading and trailing whitespace trimmed.
///      - A value can be wrapped in double-quote characters in which case the last double-quote character acts as the end point.
///     @par
///     If an error occurs, that line will be skipped and processing will continue.
void easyutil_parse_key_value_pairs(key_value_read_proc onRead, key_value_pair_proc onPair, key_value_error_proc onError, void* pUserData);



/////////////////////////////////////////////////////////
// Basic Tokenizer

/// Retrieves the first token in the given string.
///
/// @remarks
///     This function is suitable for doing a simple whitespace tokenization of a null-terminated string.
///     @par
///     The return value is a pointer to one character past the last character of the next token. You can use the return value to execute
///     this function in a loop to parse an entire string.
///     @par
///     <tokenOut> can be null. If the buffer is too small to contain the entire token it will be set to an empty string. The original
///     input string combined with the return value can be used to reliably find the token.
///     @par
///     This will handle double-quoted strings, so a string such as "My \"Complex String\"" contains two tokens: "My" and "\"Complex String\"".
///     @par
///     This function has no dependencies.
const char* easyutil_next_token(const char* tokens, char* tokenOut, unsigned int tokenOutSize);



/////////////////////////////////////////////////////////
// Known Folders

/// Retrieves the path of the user's config directory.
///
/// @remarks
///     On Windows this will typically be %APPDATA% and on Linux it will usually be ~/.config
bool easyutil_get_config_folder_path(char* pathOut, unsigned int pathOutSize);

/// Retrieves the path of the user's log directory.
///
/// @remarks
///     On Windows this will typically be %APPDATA% and on Linux it will usually be var/log
bool easyutil_get_log_folder_path(char* pathOut, unsigned int pathOutSize);



/////////////////////////////////////////////////////////
// DPI Awareness

#if defined(_WIN32) || defined(_WIN64)
/// Win32 Only: Makes the application DPI aware.
void win32_make_dpi_aware();
#endif



/////////////////////////////////////////////////////////
// Date / Time

/// Retrieves a time_t as of the time the function was called.
time_t easyutil_now();

/// Formats a data/time string.
void easyutil_datetime_short(time_t t, char* strOut, unsigned int strOutSize);



/////////////////////////////////////////////////////////
// Command Line
//
// The command line functions below are just simple iteration functions. This command line system is good for
// simple command lines, but probably not the best for programs requiring complex command line work.
//
// For argv style command lines, parse_cmdline() will run without any heap allocations. With a Win32 style
// command line there will be one malloc() per call fo parse_cmdline(). This is the only function that will do
// a malloc().
//
// Below is an example:
//
// easyutil_cmdline cmdline;
// if (easyutil_init_cmdline(&cmdline, argc, argv)) {
//     easyutil_parse_cmdline(&cmdline, my_cmdline_handler, pMyUserData);
// }
//
// void my_cmdline_handler(const char* key, const char* value, void* pUserData)
// {
//     // Do something...
// }
//
//
// When parsing the command line, the first iteration will be the program path and the key will be "[path]".
//
// For segments such as "-abcd", the callback will be called for "a", "b", "c", "d" individually, with the
// value set to NULL.
//
// For segments such as "--server", the callback will be called for "server", with the value set to NULL.
//
// For segments such as "-f file.txt", the callback will be called with the key set to "f" and the value set
// to "file.txt".
//
// For segments such as "-f file1.txt file2.txt", the callback will be called twice, once for file1.txt and
// again for file2.txt, with with the key set to "f" in both cases.
//
// For segments where there is no leading key, the values will be posted as annonymous (key set to NULL). An example
// is "my_program.exe file1.txt file2.txt", in which case the first iteration will be the program path, the second iteration
// will be "file1.txt", with the key set to NULL. The third iteration will be "file2.txt" with the key set to NULL.
//
// For segments such as "-abcd file.txt", "a", "b", "c", "d" will be sent with NULL values, and "file.txt" will be
// posted with a NULL key.

typedef struct
{
    // argv style.
    int argc;
    char** argv;

    // Win32 style
    const char* win32;

} easyutil_cmdline;

typedef bool easyutil_cmdline_parse_proc(const char* key, const char* value, void* pUserData);


/// Initializes a command line object.
bool easyutil_init_cmdline(easyutil_cmdline* pCmdLine, int argc, char** argv);

/// Initializes a command line object using a Win32 style command line.
bool easyutil_init_cmdline_win32(easyutil_cmdline* pCmdLine, const char* args);

/// Parses the given command line.
void easyutil_parse_cmdline(easyutil_cmdline* pCmdLine, easyutil_cmdline_parse_proc callback, void* pUserData);





/////////////////////////////////////////////////////////
// Threading

/// Puts the calling thread to sleep for approximately the given number of milliseconds.
///
/// @remarks
///     This is not 100% accurate and should be considered an approximation.
void easyutil_sleep(unsigned int milliseconds);


/// Thread.
typedef void* easyutil_thread;
typedef int (* easyutil_thread_entry_proc)(void* pData);

/// Creates and begins executing a new thread.
///
/// @remarks
///     This will not return until the thread has entered into it's entry point.
///     @par
///     Creating a thread should be considered an expensive operation. For high performance, you should create threads
///     and load time and cache them.
easyutil_thread easyutil_create_thread(easyutil_thread_entry_proc entryProc, void* pData);

/// Deletes the given thread.
///
/// @remarks
///     This does not actually exit the thread, but rather deletes the memory that was allocated for the thread
///     object returned by easyutil_create_thread().
///     @par
///     It is usually best to wait for the thread to terminate naturally with easyutil_wait_thread() before calling
///     this function, however it is still safe to do something like the following.
///     @code
///     easyutil_delete_thread(easyutil_create_thread(my_thread_proc, pData))
///     @endcode
void easyutil_delete_thread(easyutil_thread thread);

/// Waits for the given thread to terminate.
void easyutil_wait_thread(easyutil_thread thread);

/// Helper function for waiting for a thread and then deleting the handle after it has terminated.
void easyutil_wait_and_delete_thread(easyutil_thread thread);



/// Mutex
typedef void* easyutil_mutex;

/// Creates a mutex object.
///
/// @remarks
///     If an error occurs, 0 is returned. Otherwise a handle the size of a pointer is returned.
easyutil_mutex easyutil_create_mutex();

/// Deletes a mutex object.
void easyutil_delete_mutex(easyutil_mutex mutex);

/// Locks the given mutex.
void easyutil_lock_mutex(easyutil_mutex mutex);

/// Unlocks the given mutex.
void easyutil_unlock_mutex(easyutil_mutex mutex);



/// Semaphore
typedef void* easyutil_semaphore;

/// Creates a semaphore object.
///
/// @remarks
///     If an error occurs, 0 is returned. Otherwise a handle the size of a pointer is returned.
easyutil_semaphore easyutil_create_semaphore(int initialValue);

/// Deletes the given semaphore.
void easyutil_delete_semaphore(easyutil_semaphore semaphore);

/// Waits on the given semaphore object and decrements it's counter by one upon returning.
///
/// @remarks
///     This will block so long as the counter is > 0.
bool easyutil_wait_semaphore(easyutil_semaphore semaphore);

/// Releases the given semaphore and increments it's counter by one up returning.
bool easyutil_release_semaphore(easyutil_semaphore semaphore);




/////////////////////////////////////////////////////////
// C++ Specific

#ifdef __cplusplus

// Use this to prevent objects of the given class or struct from being copied. This is also useful for eliminating some
// compiler warnings.
//
// Note for structs - this sets the access mode to private, so place this at the end of the declaration.
#define NO_COPY(classname) \
    private: \
        classname(const classname &); \
        classname & operator=(const classname &);

#endif





#ifdef __cplusplus
}
#endif

#endif

/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/
