#ifndef _H_UTF8CODE_UNICODE_H_
#define _H_UTF8CODE_UNICODE_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C"{
#endif

struct charnuminfo
{
	int utf8num;
	int englishnum;
};

/**
 * 把字符串从utf8变为ansi，返回新的字符串，切记用完后调用free释放返回的字符串
 * */
char *Utf8ToAnsi (const char *utf8);

/**
 * 把字符串从ansi变为utf8，返回新的字符串，切记用完后调用free释放返回的字符串
 * */
char *AnsiToUtf8 (const char *ansi);





/**
 * 分析utf8字符串，获取utf8非英文的数量和英文的数量
 * */
void Utf8CharInfo (const char *utf8, struct charnuminfo *info);

/**
 * 从ansi变为宽字符
 * 若sz为0，则计算转换后所需的字符数。此时字符为wchar_t类型，切记
 * 若sz大于0，则进行转换并返回转换后的字符数量
 * */
size_t AnsiToUnicode (const char *ansi, wchar_t *utf16, size_t sz);

/**
 * 从宽字符变为ansi
 * 若sz为0，则计算转换后所需的字符数。此时字符为char类型，切记
 * 若sz大于0，则进行转换并返回转换后的字符数量
 * */
size_t UnicodeToAnsi (const wchar_t* utf16, char *utf8, size_t sz);


/**
 * 从utf8变为宽字符。
 * 若sz为0，则计算转换后的字符数量，此时字符为wchar_t类型，切记。
 * 若sz大于0，则进行转换并返回转换后的字符数量
 * */
size_t Utf8ToUnicode (const char *utf8, wchar_t *utf16, size_t sz);

/**
 * 从宽字符变为utf8
 * 若sz为0，则计算转换后的字符数量，此时字符为char类型，切记。
 * 若sz大于0，则进行转换并返回转换后的字符数量
 * */
size_t UnicodeToUtf8 (const wchar_t * utf16, char * utf8, size_t sz);

#ifdef __cplusplus
}
#endif
#endif


