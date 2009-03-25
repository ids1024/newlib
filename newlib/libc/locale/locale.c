/*
FUNCTION
<<setlocale>>, <<localeconv>>---select or query locale

INDEX
	setlocale
INDEX
	localeconv
INDEX
	_setlocale_r
INDEX
	_localeconv_r

ANSI_SYNOPSIS
	#include <locale.h>
	char *setlocale(int <[category]>, const char *<[locale]>);
	lconv *localeconv(void);

	char *_setlocale_r(void *<[reent]>,
                        int <[category]>, const char *<[locale]>);
	lconv *_localeconv_r(void *<[reent]>);

TRAD_SYNOPSIS
	#include <locale.h>
	char *setlocale(<[category]>, <[locale]>)
	int <[category]>;
	char *<[locale]>;

	lconv *localeconv();

	char *_setlocale_r(<[reent]>, <[category]>, <[locale]>)
	char *<[reent]>;
	int <[category]>;
	char *<[locale]>;

	lconv *_localeconv_r(<[reent]>);
	char *<[reent]>;

DESCRIPTION
<<setlocale>> is the facility defined by ANSI C to condition the
execution environment for international collating and formatting
information; <<localeconv>> reports on the settings of the current
locale.

This is a minimal implementation, supporting only the required <<"POSIX">>
and <<"C">> values for <[locale]>; strings representing other locales are not
honored unless _MB_CAPABLE is defined in which case POSIX locale strings
are allowed, plus five extensions supported for backward compatibility with
older implementations using newlib: <<"C-UTF-8">>, <<"C-JIS">>,
<<"C-EUCJP">>/<<"C-eucJP">>, <<"C-SJIS">>, <<"C-ISO-8859-x">> with
1 <= x <= 15, or <<"C-CPxxx">> with xxx in [437, 720, 737, 775, 850, 852,
855, 857, 858, 862, 866, 874, 1125, 1250, 1251, 1252, 1253, 1254, 1255, 1256,
1257, 1258].  Even when using POSIX locale strings, the only charsets allowed
are <<"UTF-8">>, <<"JIS">>, <<"EUCJP">>/<<"eucJP">>, <<"SJIS">>,
<<"ISO-8859-x">> with 1 <= x <= 15, or <<"CPxxx">> with xxx in [437, 720,
737, 775, 850, 852, 855, 857, 858, 862, 866, 874, 1125, 1250, 1251, 1252,
1253, 1254, 1255, 1256, 1257, 1258]. 
(<<"">> is also accepted; if given, the settings are read from the
corresponding LC_* environment variables and $LANG according to POSIX rules.

Under Cygwin, this implementation additionally supports the charsets
<<"GB2312">>, <<"eucKR">>, and <<"Big5">>.

If you use <<NULL>> as the <[locale]> argument, <<setlocale>> returns
a pointer to the string representing the current locale (always
<<"C">> in this implementation).  The acceptable values for
<[category]> are defined in `<<locale.h>>' as macros beginning with
<<"LC_">>, but this implementation does not check the values you pass
in the <[category]> argument.

<<localeconv>> returns a pointer to a structure (also defined in
`<<locale.h>>') describing the locale-specific conventions currently
in effect.  

<<_localeconv_r>> and <<_setlocale_r>> are reentrant versions of
<<localeconv>> and <<setlocale>> respectively.  The extra argument
<[reent]> is a pointer to a reentrancy structure.

RETURNS
A successful call to <<setlocale>> returns a pointer to a string
associated with the specified category for the new locale.  The string
returned by <<setlocale>> is such that a subsequent call using that
string will restore that category (or all categories in case of LC_ALL),
to that state.  The application shall not modify the string returned
which may be overwritten by a subsequent call to <<setlocale>>.
On error, <<setlocale>> returns <<NULL>>.

<<localeconv>> returns a pointer to a structure of type <<lconv>>,
which describes the formatting and collating conventions in effect (in
this implementation, always those of the C locale).

PORTABILITY
ANSI C requires <<setlocale>>, but the only locale required across all
implementations is the C locale.

NOTES
There is no ISO-8859-12 codepage.  It's also refused by this implementation.

No supporting OS subroutines are required.
*/

/* Parts of this code are originally taken from FreeBSD. */
/*
 * Copyright (c) 1996 - 2002 FreeBSD Project
 * Copyright (c) 1991, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <newlib.h>
#include <errno.h>
#include <locale.h>
#include <string.h>
#include <limits.h>
#include <reent.h>
#include <stdlib.h>
#include <wchar.h>
#include "../stdlib/local.h"
#ifdef __CYGWIN__
#include <windows.h>
#endif

#define _LC_LAST      7
#define ENCODING_LEN 31

#ifdef __CYGWIN__
int __declspec(dllexport) __mb_cur_max = 1;
#else
int __mb_cur_max = 1;
#endif

int __nlocale_changed = 0;
int __mlocale_changed = 0;
char *_PathLocale = NULL;

static _CONST struct lconv lconv = 
{
  ".", "", "", "", "", "", "", "", "", "",
  CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX,
  CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX,
};

#ifdef _MB_CAPABLE
/*
 * Category names for getenv()
 */
static char *categories[_LC_LAST] = {
  "LC_ALL",
  "LC_COLLATE",
  "LC_CTYPE",
  "LC_MONETARY",
  "LC_NUMERIC",
  "LC_TIME",
  "LC_MESSAGES",
};

/*
 * Current locales for each category
 */
static char current_categories[_LC_LAST][ENCODING_LEN + 1] = {
    "C",
    "C",
    "C",
    "C",
    "C",
    "C",
    "C",
};

/*
 * The locales we are going to try and load
 */
static char new_categories[_LC_LAST][ENCODING_LEN + 1];
static char saved_categories[_LC_LAST][ENCODING_LEN + 1];

static char current_locale_string[_LC_LAST * (ENCODING_LEN + 1/*"/"*/ + 1)];
static char *currentlocale(void);
static char *loadlocale(struct _reent *, int);
static const char *__get_locale_env(struct _reent *, int);

#endif

static char lc_ctype_charset[ENCODING_LEN + 1] = "ASCII";
static char lc_message_charset[ENCODING_LEN + 1] = "ASCII";

char *
_DEFUN(_setlocale_r, (p, category, locale),
       struct _reent *p _AND
       int category _AND
       _CONST char *locale)
{
#ifndef _MB_CAPABLE
  if (locale)
    { 
      if (strcmp (locale, "POSIX") && strcmp (locale, "C")
	  && strcmp (locale, ""))
        return NULL;
    }
  return "C";
#else
  int i, j, len, saverr;
  const char *env, *r;

  if (category < LC_ALL || category >= _LC_LAST)
    {
      p->_errno = EINVAL;
      return NULL;
    }

  if (locale == NULL)
    return category != LC_ALL ? current_categories[category] : currentlocale();

  /*
   * Default to the current locale for everything.
   */
  for (i = 1; i < _LC_LAST; ++i)
    strcpy (new_categories[i], current_categories[i]);

  /*
   * Now go fill up new_categories from the locale argument
   */
  if (!*locale)
    {
      if (category == LC_ALL)
	{
	  for (i = 1; i < _LC_LAST; ++i)
	    {
	      env = __get_locale_env (p, i);
	      if (strlen (env) > ENCODING_LEN)
		{
		  p->_errno = EINVAL;
		  return NULL;
		}
	      strcpy (new_categories[i], env);
	    }
	}
      else
	{
	  env = __get_locale_env (p, category);
	  if (strlen (env) > ENCODING_LEN)
	    {
	      p->_errno = EINVAL;
	      return NULL;
	    }
	  strcpy (new_categories[category], env);
	}
    }
  else if (category != LC_ALL)
    {
      if (strlen (locale) > ENCODING_LEN)
	{
	  p->_errno = EINVAL;
	  return NULL;
	}
      strcpy (new_categories[category], locale);
    }
  else
    {
      if ((r = strchr (locale, '/')) == NULL)
	{
	  if (strlen (locale) > ENCODING_LEN)
	    {
	      p->_errno = EINVAL;
	      return NULL;
	    }
	  for (i = 1; i < _LC_LAST; ++i)
	    strcpy (new_categories[i], locale);
	}
      else
	{
	  for (i = 1; r[1] == '/'; ++r)
	    ;
	  if (!r[1])
	    {
	      p->_errno = EINVAL;
	      return NULL;  /* Hmm, just slashes... */
	    }
	  do
	    {
	      if (i == _LC_LAST)
		break;  /* Too many slashes... */
	      if ((len = r - locale) > ENCODING_LEN)
		{
		  p->_errno = EINVAL;
		  return NULL;
		}
	      strlcpy (new_categories[i], locale, len + 1);
	      i++;
	      while (*r == '/')
		r++;
	      locale = r;
	      while (*r && *r != '/')
		r++;
	    }
	  while (*locale);
	  while (i < _LC_LAST)
	    {
	      strcpy (new_categories[i], new_categories[i-1]);
	      i++;
	    }
	}
    }

  if (category != LC_ALL)
    return loadlocale (p, category);

  for (i = 1; i < _LC_LAST; ++i)
    {
      strcpy (saved_categories[i], current_categories[i]);
      if (loadlocale (p, i) == NULL)
	{
	  saverr = p->_errno;
	  for (j = 1; j < i; j++)
	    {
	      strcpy (new_categories[j], saved_categories[j]);
	      if (loadlocale (p, j) == NULL)
		{
		  strcpy (new_categories[j], "C");
		  loadlocale (p, j);
		}
	    }
	  p->_errno = saverr;
	  return NULL;
	}
    }
  return currentlocale ();
#endif
}

#ifdef _MB_CAPABLE
static char *
currentlocale()
{
        int i;

        (void)strcpy(current_locale_string, current_categories[1]);

        for (i = 2; i < _LC_LAST; ++i)
                if (strcmp(current_categories[1], current_categories[i])) {
                        for (i = 2; i < _LC_LAST; ++i) {
                                (void)strcat(current_locale_string, "/");
                                (void)strcat(current_locale_string,
                                             current_categories[i]);
                        }
                        break;
                }
        return (current_locale_string);
}
#endif

#ifdef _MB_CAPABLE
#ifdef __CYGWIN__
extern void *__set_charset_from_codepage (unsigned int, char *charset);
extern void __set_ctype (const char *charset);
#endif /* __CYGWIN__ */

static char *
loadlocale(struct _reent *p, int category)
{
  /* At this point a full-featured system would just load the locale
     specific data from the locale files.
     What we do here for now is to check the incoming string for correctness.
     The string must be in one of the allowed locale strings, either
     one in POSIX-style, or one in the old newlib style to maintain
     backward compatibility.  If the local string is correct, the charset
     is extracted and stored in lc_ctype_charset or lc_message_charset
     dependent on the cateogry. */
  char *locale = new_categories[category];
  char charset[ENCODING_LEN + 1];
  unsigned long val;
  char *end;
  int mbc_max;
  
  /* "POSIX" is translated to "C", as on Linux. */
  if (!strcmp (locale, "POSIX"))
    strcpy (locale, "C");
  if (!strcmp (locale, "C"))				/* Default "C" locale */
    strcpy (charset, "ASCII");
  else if (locale[0] == 'C' && locale[1] == '-')	/* Old newlib style */
	strcpy (charset, locale + 2);
  else							/* POSIX style */
    {
      char *c = locale;

      /* Don't use ctype macros here, they might be localized. */
      /* Language */
      if (c[0] < 'a' || c[0] > 'z'
	  || c[1] < 'a' || c[1] > 'z')
	return NULL;
      c += 2;
      if (c[0] == '_')
        {
	  /* Territory */
	  ++c;
	  if (c[0] < 'A' || c[0] > 'Z'
	      || c[1] < 'A' || c[1] > 'Z')
	    return NULL;
	  c += 2;
	}
      if (c[0] == '.')
	{
	  /* Charset */
	  strcpy (charset, c + 1);
	  if ((c = strchr (charset, '@')))
	    /* Strip off modifier */
	    *c = '\0';
	}
      else if (c[0] == '\0' || c[0] == '@')
	/* End of string or just a modifier */
#ifdef __CYGWIN__
	__set_charset_from_codepage (GetACP (), charset);
#else
	strcpy (charset, "ISO-8859-1");
#endif
      else
	/* Invalid string */
      	return NULL;
    }
  /* We only support this subset of charsets. */
  switch (charset[0])
    {
    case 'U':
      if (strcmp (charset, "UTF-8"))
	return NULL;
      mbc_max = 6;
#ifdef _MB_CAPABLE
      __wctomb = __utf8_wctomb;
      __mbtowc = __utf8_mbtowc;
#endif
    break;
    case 'J':
      if (strcmp (charset, "JIS"))
	return NULL;
      mbc_max = 8;
#ifdef _MB_CAPABLE
      __wctomb = __jis_wctomb;
      __mbtowc = __jis_mbtowc;
#endif
    break;
    case 'E':
    case 'e':
      if (!strcmp (charset, "EUCJP") || !strcmp (charset, "eucJP"))
	{
	  strcpy (charset, "EUCJP");
	  mbc_max = 2;
#ifdef _MB_CAPABLE
	  __wctomb = __eucjp_wctomb;
	  __mbtowc = __eucjp_mbtowc;
#endif
	}
#ifdef __CYGWIN__
      else if (!strcmp (charset, "EUCKR") || !strcmp (charset, "eucKR"))
	{
	  strcpy (charset, "EUCKR");
	  mbc_max = 2;
#ifdef _MB_CAPABLE
	  __wctomb = __kr_wctomb;
	  __mbtowc = __kr_mbtowc;
#endif
	}
#endif
      else
	return NULL;
    break;
    case 'S':
      if (strcmp (charset, "SJIS"))
	return NULL;
      mbc_max = 2;
#ifdef _MB_CAPABLE
      __wctomb = __sjis_wctomb;
      __mbtowc = __sjis_mbtowc;
#endif
    break;
    case 'I':
      /* Must be exactly one of ISO-8859-1, [...] ISO-8859-16, except for
         ISO-8859-12. */
      if (strncmp (charset, "ISO-8859-", 9))
	return NULL;
      val = _strtol_r (p, charset + 9, &end, 10);
      if (val < 1 || val > 16 || val == 12 || *end)
	return NULL;
      mbc_max = 1;
#ifdef _MB_CAPABLE
#ifdef _MB_EXTENDED_CHARSETS_ISO
      __wctomb = __iso_wctomb;
      __mbtowc = __iso_mbtowc;
#else /* !_MB_EXTENDED_CHARSETS_ISO */
      __wctomb = __ascii_wctomb;
      __mbtowc = __ascii_mbtowc;
#endif /* _MB_EXTENDED_CHARSETS_ISO */
#endif
    break;
    case 'C':
      if (charset[1] != 'P')
	return NULL;
      val = _strtol_r (p, charset + 2, &end, 10);
      if (*end)
	return NULL;
      switch (val)
	{
	case 437:
	case 720:
	case 737:
	case 775:
	case 850:
	case 852:
	case 855:
	case 857:
	case 858:
	case 862:
	case 866:
	case 874:
	case 1125:
	case 1250:
	case 1251:
	case 1252:
	case 1253:
	case 1254:
	case 1255:
	case 1256:
	case 1257:
	case 1258:
	  mbc_max = 1;
#ifdef _MB_CAPABLE
#ifdef _MB_EXTENDED_CHARSETS_WINDOWS
	  __wctomb = __cp_wctomb;
	  __mbtowc = __cp_mbtowc;
#else /* !_MB_EXTENDED_CHARSETS_WINDOWS */
	  __wctomb = __ascii_wctomb;
	  __mbtowc = __ascii_mbtowc;
#endif /* _MB_EXTENDED_CHARSETS_WINDOWS */
#endif
	  break;
	default:
	  return NULL;
	}
    break;
    case 'A':
      if (strcmp (charset, "ASCII"))
	return NULL;
      mbc_max = 1;
#ifdef _MB_CAPABLE
      __wctomb = __ascii_wctomb;
      __mbtowc = __ascii_mbtowc;
#endif
      break;
#ifdef __CYGWIN__
    case 'G':
      if (strcmp (charset, "GB2312"))
      	return NULL;
      mbc_max = 2;
#ifdef _MB_CAPABLE
      __wctomb = __gbk_wctomb;
      __mbtowc = __gbk_mbtowc;
#endif
      break;
    case 'B':
      if (strcmp (charset, "BIG5") && strcmp (charset, "Big5"))
      	return NULL;
      strcpy (charset, "BIG5");
      mbc_max = 2;
#ifdef _MB_CAPABLE
      __wctomb = __big5_wctomb;
      __mbtowc = __big5_mbtowc;
#endif
      break;
#endif /* __CYGWIN__ */
    default:
      return NULL;
    }
  if (category == LC_CTYPE)
    {
      strcpy (lc_ctype_charset, charset);
      __mb_cur_max = mbc_max;
#ifdef __CYGWIN__
      __set_ctype (charset);
#endif
    }
  else if (category == LC_MESSAGES)
    strcpy (lc_message_charset, charset);
  return strcpy(current_categories[category], new_categories[category]);
}

static const char *
__get_locale_env(struct _reent *p, int category)
{
  const char *env;

  /* 1. check LC_ALL. */
  env = _getenv_r (p, categories[0]);

  /* 2. check LC_* */
  if (env == NULL || !*env)
    env = _getenv_r (p, categories[category]);

  /* 3. check LANG */
  if (env == NULL || !*env)
    env = _getenv_r (p, "LANG");

  /* 4. if none is set, fall to "C" */
  if (env == NULL || !*env)
    env = "C";

  return env;
}
#endif

char *
_DEFUN_VOID(__locale_charset)
{
  return lc_ctype_charset;
}

char *
_DEFUN_VOID(__locale_msgcharset)
{
  return lc_message_charset;
}

struct lconv *
_DEFUN(_localeconv_r, (data), 
      struct _reent *data)
{
  return (struct lconv *) &lconv;
}

#ifndef _REENT_ONLY

char *
_DEFUN(setlocale, (category, locale),
       int category _AND
       _CONST char *locale)
{
  return _setlocale_r (_REENT, category, locale);
}


struct lconv *
_DEFUN_VOID(localeconv)
{
  return _localeconv_r (_REENT);
}

#endif
