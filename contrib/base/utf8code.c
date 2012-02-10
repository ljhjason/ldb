
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include "utf8code.h"

/**
 * 如果为windows则为chs,若为linux则为zh_CN
 * */
#ifdef WIN32
#define LOCAL_PAGE "chs"
#else
#define LOCAL_PAGE "zh_CN"
#endif
#define SET_LOCAL 	setlocale(LC_ALL, LOCAL_PAGE)

/**
 * 从ansi变为宽字符
 * 若sz为0，则计算转换后所需的字符数。此时字符为wchar_t类型，切记
 * 若sz大于0，则进行转换并返回转换后的字符数量
 * */
size_t AnsiToUnicode (const char *ansi, wchar_t *utf16, size_t sz)
{
	SET_LOCAL;
	if (sz == 0)
		return mbstowcs(NULL, ansi, 0);
	return mbstowcs(utf16, ansi, sz);
}

/**
 * 从宽字符变为ansi
 * 若sz为0，则计算转换后所需的字符数。此时字符为char类型，切记
 * 若sz大于0，则进行转换并返回转换后的字符数量
 * */
size_t UnicodeToAnsi (const wchar_t *utf16, char *utf8, size_t sz)
{
	SET_LOCAL;
	if (sz == 0)
		return wcstombs(NULL, utf16, 0);
	SET_LOCAL;
	return wcstombs(utf8, utf16, sz);
}

/**
 * 分析utf8字符串，获取utf8非英文的数量和英文的数量
 * */
void Utf8CharInfo (const char *utf8, struct charnuminfo *info)
{
	if (!utf8 || !info)
		return;

	info->utf8num = 0;
	info->englishnum = 0;
	while (*utf8)
	{
		signed char c = *utf8;
		if ((c & 0xc0) != 0x80)
		{
			if (c > 0)
				++info->englishnum;
			else
				++info->utf8num;
		}
		++utf8;
	}
}

/**
 * 从utf8变为宽字符。
 * 若sz为0，则计算转换后的字符数量，此时字符为wchar_t类型，切记。
 * 若sz大于0，则进行转换并返回转换后的字符数量
 * */
size_t Utf8ToUnicode (const char *utf8, wchar_t *utf16, size_t sz)
{
	wchar_t *ptr;
	while (((unsigned char)*utf8 & 0xc0) == 0x80)
	{
		++utf8;
	}
	if (sz == 0) 
	{
		size_t n = 0;
		while (*utf8)
		{
			signed char c = *utf8;
			if ((c & 0xc0) != 0x80)
			{
				++n;
			}
			++utf8;
		}
		return n;
	}
	if (*utf8 == '\0')
	{
		utf16[0] = 0;
		return 0;
	}

	ptr = utf16 - 1;
	
	while (*utf8)
	{
		signed char c = *utf8;
		if ((c & 0xc0) == 0x80)
		{
			*ptr = *ptr<<6 | (c & 0x3f);
		}
		else
		{
			++ptr;
			if (ptr - utf16 >= sz)
			{
				*(ptr-1) = 0;
				return sz - 1;
			}
			*ptr = c&(0x0f | (~(c>>1) &0x1f) | ~(c>>7));
		}
		++utf8;
	}
	
	if (ptr - utf16 < sz)
	{
		++ptr;
	}
	*ptr = 0;
	return ptr - utf16;
}

/**
 * 从宽字符变为utf8
 * 若sz为0，则计算转换后的字符数量，此时字符为char类型，切记。
 * 若sz大于0，则进行转换并返回转换后的字符数量
 * */
size_t UnicodeToUtf8(const wchar_t * utf16, char * utf8, size_t sz)
{
	unsigned char *last;
	unsigned char *ptr = (unsigned char *)utf8;
	if (sz < 3)
	{
		utf8 = (char *)utf16;
		ptr = (unsigned char *)utf16;
		while (*utf16)
		{
			wchar_t c = *utf16;
			if (c>0x7F)
			{
				if (c>0x7ff)
				{
					++ptr;
				}
				++ptr;
			}
			++ptr;
			++utf16;
		}
		return (char *)ptr - utf8;
	}

	sz -= 2;
	
	while (*utf16)
	{
		wchar_t c = *utf16;
		last = ptr;
		if (c <= 0x7F)
		{
			*ptr = (unsigned char)c;
		}
		else
		{
			if (c <= 0x7ff)
			{
				*ptr = (unsigned char)((c>>6) | 0xc0);
			}
			else
			{
				*ptr = (unsigned char)((c>>12) | 0xe0);
				++ptr;
				*ptr = (unsigned char)(((c>>6) & 0x3f) | 0x80);
			}
			++ptr;
			*ptr = (unsigned char)((c&0x3f) | 0x80);
		}
		++ptr;
		if ((char *)ptr - utf8 >= sz + 2)
		{
			ptr = last;
			break;
		}
		++utf16;
	}
	*ptr = 0;
	return (char *)ptr - utf8;
}

/**
 * 把字符串从utf8变为ansi，返回新的字符串，切记用完后调用free释放返回的字符串
 * */
char *Utf8ToAnsi (const char *utf8)
{
	int sz;
	wchar_t *buf;
	char *out;
	if (!utf8 || utf8[0] == '\0')
		return NULL;

	/**
	 * 先把字符串从utf8变为Unicode，然后再变为ansi
	 * */
	sz = Utf8ToUnicode(utf8, NULL, 0);
	buf = (wchar_t *)malloc((sz + 2) * sizeof(wchar_t));
	if (!buf)
		return NULL;
	buf[sz] = 0;
	buf[sz + 1] = 0;
	sz = Utf8ToUnicode(utf8, buf, sz + 1);
	sz = UnicodeToAnsi(buf, NULL, 0);
	out = (char *)malloc(sz + 2);
	if (!out)
	{
		free(buf);
		return NULL;
	}

	out[sz] = 0;
	out[sz + 1] = 0;
	UnicodeToAnsi(buf, out, sz + 1);
	free(buf);
	return out;
}

/**
 * 把字符串从ansi变为utf8，返回新的字符串，切记用完后调用free释放返回的字符串
 * */
char *AnsiToUtf8 (const char *ansi)
{
	int sz;
	wchar_t *buf;
	char *out;
	if (!ansi || ansi[0] == '\0')
		return NULL;

	/**
	 * 先把字符串从ansi变为Unicode，然后再变为utf8
	 * */
	sz = AnsiToUnicode(ansi, NULL, 0);
	buf = (wchar_t *)malloc((sz + 2) * sizeof(wchar_t));
	if (!buf)
		return NULL;
	buf[sz] = 0;
	buf[sz + 1] = 0;
	sz = AnsiToUnicode(ansi, buf, sz + 1);
	sz = UnicodeToUtf8(buf, NULL, 0);
	out = (char *)malloc(sz + 2);
	if (!out)
	{
		free(buf);
		return NULL;
	}

	out[sz] = 0;
	out[sz + 1] = 0;
	UnicodeToUtf8(buf, out, sz + 1);
	free(buf);
	return out;
}



