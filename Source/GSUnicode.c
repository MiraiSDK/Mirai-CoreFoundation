/* GSUnicode.h
   
   Copyright (C) 2013 Free Software Foundation, Inc.
   
   Written by: Stefan Bidigaray
   Date: July, 2013
   
   This file is part of GNUstep CoreBase library.
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; see the file COPYING.LIB.
   If not, see <http://www.gnu.org/licenses/> or write to the 
   Free Software Foundation, 51 Franklin Street, Fifth Floor, 
   Boston, MA 02110-1301, USA.
*/

#include "config.h"

#include "CoreFoundation/CFByteOrder.h"
#include "CoreFoundation/CFDictionary.h"
#include "CoreFoundation/CFLocale.h"
#include "CoreFoundation/CFRuntime.h"

#include "GSPrivate.h"
#include "GSUnicode.h"
#include "GSMemory.h"

#include <unicode/ucnv.h>

#define BUFFER_SIZE 512

CFIndex
GSUnicodeFromUTF8 (const UInt8 * s, CFIndex slen, UniChar lossChar, UniChar * d,
                   CFIndex dlen, CFIndex * usedLen)
{
  const UInt8 *sstart;
  const UInt8 *slimit;
  UniChar *dstart;
  UniChar *dlimit;

  sstart = s;
  slimit = sstart + slen;
  dstart = d;
  dlimit = dstart + dlen;
  while (s < slimit && (dlen == 0 || d < dlimit))
    {
      if (*s < 0x80)
        {
          if (dlen != 0)
            *d = *s;
          d++;
          s++;
        }
      else
        {
          UTF32Char u;
          CFIndex count;

          count = GS_UTF8_BYTE_COUNT (*s);
          if (count == 2 && GS_UTF8_IS_TRAIL (s[1]))
            {
              u = (s[0] & 0x1F) << 6;
              u |= s[1] & 0x3F;
            }
          else if (count == 3 && GS_UTF8_IS_TRAIL (s[1])
                   && GS_UTF8_IS_TRAIL (s[2]))
            {
              u = (s[0] & 0x0F) << 12;
              u |= (s[1] & 0x3F) << 6;
              u |= s[2] & 0x3F;
            }
          else if (count == 4 && GS_UTF8_IS_TRAIL (s[1])
                   && GS_UTF8_IS_TRAIL (s[2]) && GS_UTF8_IS_TRAIL (s[3]))
            {
              u = (s[0] & 0x07) << 18;
              u |= (s[1] & 0x3F) << 12;
              u |= (s[2] & 0x3F) << 6;
              u |= s[3] & 0x3F;
            }
          else
            {
              break;
            }
          s += count;
          if (u < 0x10000)
            {
              if (GS_UTF_IS_SURROGATE (u))
                break;
              if (dlen != 0)
                *d = u;
              d++;
            }
          else if (u < 0x10FFFF)
            {
              if (dlen != 0)
                {
                  d[0] = (u >> 10) + 0xD7C0;
                  d[1] = (u & 0x3FF) | 0xDC00;
                }
              d += 2;
            }
          else
            {
              break;
            }
        }
    }

  if (usedLen)
    *usedLen = ((UInt8 *) d) - ((UInt8 *) dstart);

  return s - sstart;
}

static UInt8 utf8[] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0 };

CFIndex
GSUnicodeToUTF8 (const UniChar * s, CFIndex slen, UniChar lossChar, UInt8 * d,
                 CFIndex dlen, CFIndex * usedLen)
{
  const UniChar *sstart;
  const UniChar *slimit;
  UInt8 *dstart;
  UInt8 *dlimit;

  sstart = s;
  slimit = sstart + slen;
  dstart = d;
  dlimit = dstart + dlen;
  while (s < slimit && (dlen == 0 || d < dlimit))
    {
      UTF32Char u;

      u = *s++;
      if (u < 0x80)
        {
          if (dlen != 0)
            *d = (UInt8) u;
          d++;
        }
      else
        {
          CFIndex count;

          if (GS_UTF_IS_SURROGATE (u) && GS_UTF_IS_LEAD_SURROGATE (u)
              && slimit - s > 0)
            {
              UTF16Char u16;

              u16 = *s++;
              if (GS_UTF_IS_TRAIL_SURROGATE (u16))
                u = GS_UTF16_GET_CHAR (u, u16);
              else if (lossChar)
                u = lossChar;
              else
                break;
            }
          else if (lossChar)
            {
              u = lossChar;
            }
          else
            {
              break;
            }
          count = GS_UTF8_LENGTH (u);
          if (count > 4)
            break;
          if (dlen != 0)
            {
              if (count < dlimit - d)
                break;
              switch (count)
                {
                case 4:
                  d[3] = (u & 0x3F) | 0x80;
                  u = u >> 6;
                case 3:
                  d[2] = (u & 0x3F) | 0x80;
                  u = u >> 6;
                case 2:
                  d[1] = (u & 0x3F) | 0x80;
                  u = u >> 6;
                case 1:
                  d[0] = (u & 0x3F) | utf8[count];
                }
            }
          d += count;
        }
    }

  if (usedLen)
    *usedLen = ((UInt8 *) d) - ((UInt8 *) dstart);

  return s - sstart;
}

CFIndex
GSUnicodeFromUTF32 (const UTF32Char * s, CFIndex slen, UniChar lossChar,
                    UniChar * d, CFIndex dlen, CFIndex * usedLen)
{
  const UTF32Char *sstart;
  const UTF32Char *slimit;
  UniChar *dstart;
  UniChar *dlimit;

  sstart = s;
  slimit = sstart + slen;
  dstart = d;
  dlimit = dstart + dlen;
  while (s < slimit && (dlen == 0 || d < dlimit))
    {
      UTF32Char u;

      u = *s++;
      if (u < 0x10000)
        {
          if (GS_UTF_IS_SURROGATE (u))
            break;
          if (dlen != 0)
            *d = u;
          d++;
        }
      else
        {
          if (u >= 0x10FFFF)
            break;
          if (dlen != 0)
            {
              d[0] = (u >> 10) + 0xD7C0;
              d[1] = (u & 0x3FF) | 0xDC00;
            }
          d += 2;
        }
    }

  if (usedLen)
    *usedLen = ((UInt8 *) d) - ((UInt8 *) dstart);

  return s - sstart;
}

CFIndex
GSUnicodeToUTF32 (const UniChar * s, CFIndex slen, UniChar lossChar,
                  UTF32Char * d, CFIndex dlen, CFIndex * usedLen)
{
  const UniChar *sstart;
  const UniChar *slimit;
  UTF32Char *dstart;
  UTF32Char *dlimit;

  sstart = s;
  slimit = sstart + slen;
  dstart = d;
  dlimit = dstart + dlen;
  while (s < slimit && (dlen == 0 || d < dlimit))
    {
      UTF32Char u;

      u = *s++;
      if (GS_UTF_IS_SURROGATE (u))
        {
          UTF16Char u16;

          if (slimit - s > 0 || !GS_UTF_IS_LEAD_SURROGATE (u))
            break;
          u16 = *s++;
          if (!GS_UTF_IS_TRAIL_SURROGATE (u))
            break;
          u = GS_UTF16_GET_CHAR (u, u16);
        }
      if (dlen != 0)
        *d = u;
      d++;
    }

  if (usedLen)
    *usedLen = ((UInt8 *) d) - ((UInt8 *) dstart);

  return s - sstart;
}

CFIndex
GSUnicodeFromNonLossyASCII (const char *s, CFIndex slen, UniChar lossChar,
                            UniChar * d, CFIndex dlen, CFIndex * usedLen)
{
  const char *sstart;
  const char *slimit;
  UniChar *dstart;
  UniChar *dlimit;

  sstart = s;
  slimit = sstart + slen;
  if (d == NULL)
    dlen = 0;
  dstart = d;
  dlimit = dstart + dlen;
  while (s < slimit && (dlen == 0 || d < dlimit))
    {
      UniChar c;

      c = *s;
      if (c < 0x80 && c != '\\')
        {
          if (dlen != 0)
            *d = *s;
          d++;
          s++;
        }
      else if (c == '\\')
        {
          CFIndex convCount;

          c = s[1];
          if (c == '\\')
            {
              convCount = 2;
            }
          else if (CHAR_IS_OCTAL (c) && CHAR_IS_OCTAL (s[2])
                   && CHAR_IS_OCTAL (s[3]))
            {
              c = (s[1] - '0') << 6;
              c |= (s[2] - '0') << 3;
              c |= s[3] - '0';
              convCount = 4;
            }
          else if (c == 'u' && CHAR_IS_HEX (s[2]) && CHAR_IS_HEX (s[3])
                   && CHAR_IS_HEX (s[4]) && CHAR_IS_HEX (s[5]))
            {
              /* The first part of this equation gives us a value between
               * 0 and 9, the second part adds 9 to that if the char is
               * alphabetic.
               */
              c = ((s[5] & 0xF) + (s[5] >> 6) * 9) & 0x000F;
              c |= ((s[4] & 0xF) + (s[4] >> 6) * 9) << 4;
              c |= ((s[3] & 0xF) + (s[3] >> 6) * 9) << 8;
              c |= ((s[2] & 0xF) + (s[2] >> 6) * 9) << 12;
              convCount = 6;
            }
          else
            {
              break;
            }
          if (dlen != 0)
            *d = c;
          d++;
          s += convCount;
        }
      else
        {
          break;
        }
    }

  if (usedLen)
    *usedLen = d - dstart;

  return s - sstart;
}

static const char *base16 = "0123456789ABCDEF";

CFIndex
GSUnicodeToNonLossyASCII (const UniChar * s, CFIndex slen, UniChar lossChar,
                          char *d, CFIndex dlen, CFIndex * usedLen)
{
  const UniChar *sstart;
  const UniChar *slimit;
  char *dstart;
  char *dlimit;

  sstart = s;
  slimit = sstart + slen;
  if (d == NULL)
    dlen = 0;
  dstart = d;
  dlimit = dstart + dlen;
  while (s < slimit && (dlen == 0 || d < dlimit))
    {
      UniChar c;

      c = *s;
      if (c < 0x80 && c != '\\')
        {
          if (dlen != 0)
            *d = c;
          d++;
          s++;
        }
      else
        {
          char conv[6];
          CFIndex convCount;

          conv[0] = '\\';
          if (c == '\\')
            {
              conv[1] = '\\';
              convCount = 2;
            }
          else if (c <= 0x00FF)
            {
              conv[3] = (c & 0x7) + '0';
              conv[2] = ((c >> 3) & 0x7) + '0';
              conv[1] = ((c >> 6) & 0x7) + '0';
              convCount = 4;
            }
          else
            {
              conv[5] = base16[(c & 0xF)];
              conv[4] = base16[((c >> 4) & 0xF)];
              conv[3] = base16[((c >> 8) & 0xF)];
              conv[2] = base16[((c >> 12) & 0xF)];
              conv[1] = 'u';
              convCount = 6;
            }
          if (dlen != 0 && convCount < dlimit - d)
            {
              CFIndex i;
              for (i = 0; i < convCount; ++i)
                d[i] = conv[i];
            }
          d += convCount;
        }
    }

  if (usedLen)
    *usedLen = d - dstart;

  return s - sstart;
}

CFIndex
GSUnicodeFromISOLatin1 (const UInt8 * s, CFIndex slen, UniChar lossChar,
                        UniChar * d, CFIndex dlen, CFIndex * usedLen)
{
  const UInt8 *sstart;
  const UInt8 *slimit;
  UniChar *dstart;
  UniChar *dlimit;

  sstart = s;
  slimit = sstart + slen;
  if (d == NULL)
    dlen = 0;
  dstart = d;
  dlimit = dstart + dlen;
  while (s < slimit && (dlen == 0 || d < dlimit))
    {
      UInt8 c;

      c = *s++;
      if (dlen != 0)
        *d = c;
      d++;
    }

  if (usedLen)
    *usedLen = d - dstart;

  return s - sstart;
}

CFIndex
GSUnicodeToISOLatin1 (const UniChar * s, CFIndex slen, UniChar lossChar,
                      UInt8 * d, CFIndex dlen, CFIndex * usedLen)
{
  const UniChar *sstart;
  const UniChar *slimit;
  UInt8 *dstart;
  UInt8 *dlimit;

  sstart = s;
  slimit = sstart + slen;
  if (d == NULL)
    dlen = 0;
  dstart = d;
  dlimit = dstart + dlen;
  while (s < slimit && (dlen == 0 || d < dlimit))
    {
      UniChar u;

      u = *s++;
      if (u > 0xFF)
        {
          if (lossChar)
            u = lossChar;
          else
            break;
        }
      if (dlen != 0)
        *d = u;
      d++;
    }

  if (usedLen)
    *usedLen = d - dstart;

  return s - sstart;
}

CFIndex
GSFromUnicode (const UniChar * s, CFIndex slen,
               CFStringEncoding enc, UniChar lossChar,
               Boolean isExtRep, UInt8 * d, CFIndex dlen, CFIndex * usedDstLen)
{
  CFIndex converted;

  converted = 0;
  if (d == NULL)
    dlen = 0;

  if (enc == kCFStringEncodingUTF8)
    {
      if (isExtRep && dlen > 3)
        {
          *d++ = 0xEF;
          *d++ = 0xBB;
          *d++ = 0xBF;
          dlen -= 3;
        }
      converted = GSUnicodeToUTF8 (s, slen, lossChar, d, dlen, usedDstLen);
    }
  else if (enc == kCFStringEncodingUTF16
           || enc == kCFStringEncodingUTF16BE
           || enc == kCFStringEncodingUTF16LE)
    {
      UniChar *dst;
      CFIndex copyLength;

      dst = (UniChar *) d;
      if (isExtRep && enc == kCFStringEncodingUTF16 && dlen >= 2)
        {
          *dst = GS_UTF16_BOM;
          dst++;
          dlen -= 2;
        }

      copyLength =
        (dlen <= slen * sizeof (UniChar)) ? dlen : (slen * sizeof (UniChar));
      memcpy (dst, s, copyLength);
      if (enc == GS_UTF16_ENCODING_TO_SWAP)
        {
          UniChar *end;

          end = dst + converted;
          while (dst < end)
            {
              *dst = CFSwapInt16 (*dst);
              ++dst;
            }
        }
      if (usedDstLen)
        *usedDstLen = slen + (isExtRep ? 2 : 0);
      converted = copyLength / sizeof (UniChar);
    }
  else if (enc == kCFStringEncodingUTF32
           || enc == kCFStringEncodingUTF32BE
           || enc == kCFStringEncodingUTF32LE)
    {
      UTF32Char *dst;

      dst = (UTF32Char *) d;
      if (isExtRep && enc == kCFStringEncodingUTF32 && dlen >= 4)
        {
          *dst++ = GS_UTF32_BOM;
          dlen -= 4;
        }
      /* round to the nearest multiple of 4 */
      dlen &= ~0x3;
      converted =
        GSUnicodeToUTF32 (s, slen / sizeof (UTF32Char), lossChar,
                          (UTF32Char *) d, dlen, usedDstLen);
      if (enc == GS_UTF32_ENCODING_TO_SWAP && dlen != 0)
        {
          UTF32Char *cur;
          UTF32Char *end;

          cur = (UTF32Char *) d;
          end = (UTF32Char *) (d + dlen);
          while (cur < end)
            {
              *cur = CFSwapInt32 (*cur);
              cur++;
            }
        }
    }
  else if (enc == kCFStringEncodingASCII)
    {
      UInt8 *dstart;
      const UInt8 *dlimit;
      const UniChar *sstart;
      const UniChar *slimit;

      sstart = s;
      slimit = s + slen;
      dstart = (UInt8 *) d;
      dlimit = dstart + dlen;
      while (s < slimit && (dlen == 0 || d < dlimit))
        {
          if (*s < 0x80)
            {
              *d++ = (UInt8) * s++;
            }
          else
            {
              if (!lossChar)
                break;
              *d++ = (UInt8) lossChar;
              s++;
            }
        }
      if (usedDstLen)
        *usedDstLen = (d - dstart);

      converted = (CFIndex) (s - sstart);
    }
  else if (enc == kCFStringEncodingNonLossyASCII)
    {
      converted = GSUnicodeToNonLossyASCII (s, slen, lossChar, (char *) d, dlen,
                                            usedDstLen);
    }
  else if (enc == kCFStringEncodingISOLatin1)
    {
      converted = GSUnicodeToISOLatin1 (s, slen, lossChar, d, dlen, usedDstLen);
    }
#if 0
  else
    {
      UConverter *ucnv;

      ucnv = GSStringOpenConverter (enc, lossChar);
      if (ucnv)
        {
          char *target;
          const char *targetLimit;
          const UniChar *source;
          const UniChar *sourceLimit;
          UErrorCode err = U_ZERO_ERROR;

          target = (char *) d;
          targetLimit = target + dlen;
          source = s;
          sourceLimit = source + slen;

          ucnv_fromUnicode (ucnv, &target, targetLimit, &source, sourceLimit,
                            NULL, true, &err);
          converted = (CFIndex) (target - (char *) d);
          if (usedDstLen)
            {
              *usedDstLen = converted;
              if (err == U_BUFFER_OVERFLOW_ERROR)
                {
                  char ibuffer[256];    /* Arbitrary buffer size */

                  targetLimit = ibuffer + 255;
                  do
                    {
                      target = ibuffer;
                      err = U_ZERO_ERROR;
                      ucnv_fromUnicode (ucnv, &target, targetLimit, &source,
                                        sourceLimit, NULL, true, &err);
                      *usedDstLen += (CFIndex) (target - ibuffer);
                    }
                  while (err == U_BUFFER_OVERFLOW_ERROR);
                }
            }

          GSStringCloseConverter (ucnv);
        }
    }
#endif

  return converted;
}

CFIndex
GSToUnicode (const UInt8 * s, CFIndex slen, CFStringEncoding enc,
             UniChar lossChar, Boolean isExtRep, UniChar * d, CFIndex dlen,
             CFIndex * usedDstLen)
{
  CFIndex converted;

  converted = 0;
  if (d == NULL)
    dlen = 0;

  if (enc == kCFStringEncodingUTF8)
    {
      if (isExtRep && slen > 3
          && (s[0] == 0xEF && s[1] == 0xBB && s[2] == 0xBF))
        s += 3;

      converted = GSUnicodeFromUTF8 (s, slen, lossChar, d, dlen, usedDstLen);
    }
  else if (enc == kCFStringEncodingUTF16
           || enc == kCFStringEncodingUTF16BE
           || enc == kCFStringEncodingUTF16LE)
    {
      const UniChar *src;
      CFIndex copyLength;
      Boolean swap;

      src = (const UniChar *) s;
      swap = false;

      if (enc == kCFStringEncodingUTF16)
        {
          UniChar bom;

          bom = (*src == 0xFEFF || *src == 0xFFFE) ? *src++ : 0;
#if WORDS_BIGENDIAN
          if (bom == 0xFFFE)
#else
          if (bom == 0xFEFF)
#endif
            swap = true;
        }
      else if (enc == GS_UTF16_ENCODING_TO_SWAP)
        {
          swap = true;
        }

      if (swap && slen != 0)
        {
          UniChar *cur;
          UniChar *end;

          cur = (UniChar *) s;
          end = (UniChar *) (s + slen);
          while (cur < end)
            {
              *cur = CFSwapInt16 (*cur);
              ++cur;
            }
        }
      copyLength =
        (dlen * sizeof (UniChar) <= slen) ? (dlen * sizeof (UniChar)) : slen;
      memcpy (d, s, copyLength);
      if (usedDstLen)
        *usedDstLen = slen;
      converted = copyLength / sizeof (UniChar);
    }
  else if (enc == kCFStringEncodingUTF32
           || enc == kCFStringEncodingUTF32BE
           || enc == kCFStringEncodingUTF32LE)
    {
      const UTF32Char *src;
      Boolean swap;

      src = (const UTF32Char *) s;
      swap = false;
      if (enc == kCFStringEncodingUTF32)
        {
          UTF32Char bom;

          bom = (*src == 0x0000FEFF || *src == 0xFFFE0000) ? *src++ : 0;
#if WORDS_BIGENDIAN
          if (bom == 0xFFFE0000)
#else
          if (bom == 0x0000FEFF)
#endif
            swap = true;
        }
      else if (enc == GS_UTF32_ENCODING_TO_SWAP)
        {
          swap = true;
        }
      /* round to the nearest multiple of 4 */
      slen &= ~0x3;
      if (swap && slen != 0)
        {
          UTF32Char *cur;
          UTF32Char *end;

          cur = (UTF32Char *) s;
          end = (UTF32Char *) (s + slen);
          while (cur < end)
            {
              *cur = CFSwapInt32 (*cur);
              cur++;
            }
        }
      converted =
        GSUnicodeFromUTF32 ((const UTF32Char *) s, slen / sizeof (UTF32Char),
                            lossChar, d, dlen, usedDstLen);
    }
  else if (enc == kCFStringEncodingASCII)
    {
      UniChar *dstart;
      const UniChar *dlimit;

      if (dlen > slen)
        dlen = slen;
      dstart = d;
      dlimit = d + dlen;
      while (d < dlimit)
        *d++ = *s++;
      if (usedDstLen)
        *usedDstLen = slen;
      converted = d - dstart;
    }
  else if (enc == kCFStringEncodingNonLossyASCII)
    {
      converted =
        GSUnicodeFromNonLossyASCII ((const char *) s, slen, lossChar, d, dlen,
                                    usedDstLen);
    }
  else if (enc == kCFStringEncodingISOLatin1)
    {
      converted = GSUnicodeFromISOLatin1 (s, slen, lossChar, d, dlen,
                                          usedDstLen);
    }
#if 0
  else
    {
      UConverter *ucnv;

      ucnv = GSStringOpenConverter (enc, 0);
      converted = 0;
      if (ucnv)
        {
          UniChar *target;
          const UniChar *targetLimit;
          const char *source;
          const char *sourceLimit;
          UErrorCode err = U_ZERO_ERROR;

          target = d;
          targetLimit = target + dlen;
          source = (char *) s;
          sourceLimit = source + slen;

          ucnv_toUnicode (ucnv, &target, targetLimit, &source, sourceLimit,
                          NULL, true, &err);
          converted = (CFIndex) (target - d);
          if (usedDstLen)
            {
              *usedDstLen = converted;
              if (err == U_BUFFER_OVERFLOW_ERROR)
                {
                  UniChar ibuffer[256]; /* Arbitrary buffer size */

                  targetLimit = ibuffer + 255;
                  do
                    {
                      target = ibuffer;
                      err = U_ZERO_ERROR;
                      ucnv_toUnicode (ucnv, &target, targetLimit, &source,
                                      sourceLimit, NULL, true, &err);
                      *usedDstLen +=
                        (CFIndex) ((char *) target - (char *) ibuffer);
                    }
                  while (err == U_BUFFER_OVERFLOW_ERROR);
                }
            }

          GSStringCloseConverter (ucnv);
        }
    }
#endif

  return converted;
}

typedef union format_argument
{
  int intValue;
  long int lintValue;
  long long int llintValue;
  double dblValue;
  long double ldblValue;
  void *ptrValue;
} format_argument_t;

#define FMT_MOD_INT 0
enum
{
  FMT_UNKNOWN = 0,
  FMT_SPACE,                    /* ' ' */
  FMT_HASH,                     /* '#' */
  FMT_QUOTE,                    /* '\'' */
  FMT_PLUS,                     /* '+' */
  FMT_MINUS,                    /* '-' */
  FMT_ZERO,                     /* '0' */
  FMT_NUMBER,                   /* '1' ... '9' */
  FMT_POSITION,                 /* '$' */
  FMT_WIDTH_AST,                /* '*' */
  FMT_PRECISION,                /* '.' */
  FMT_MOD_CHAR,                 /* 'hh' NOTE: Note in fmt_table[] */
  FMT_MOD_SHORT,                /* 'h' */
  FMT_MOD_LONG,                 /* 'l' or 'L' */
  FMT_MOD_LONGLONG,             /* 'll' NOTE: Not in fmt_table[] */
  FMT_MOD_SIZE,                 /* 'z' */
  FMT_MOD_PTRDIFF,              /* 't' */
  FMT_MOD_INTMAX,               /* 'j' */
  FMT_QUADWORD,                 /* 'q' */
  FMT_PERCENT,                  /* '%' */
  FMT_OBJECT,                   /* '@' */
  FMT_POINTER,                  /* 'p' */
  FMT_INTEGER,                  /* 'd', 'D' or 'i' */
  FMT_OCTAL,                    /* 'o' or 'O' */
  FMT_HEX,                      /* 'x' or 'X' */
  FMT_UINTEGER,                 /* 'u' */
  FMT_DOUBLE,                   /* 'e', 'E', 'f', 'F', 'g' or 'G' */
  FMT_DOUBLEHEX,                /* 'a' or 'A' */
  FMT_GETCOUNT,                 /* 'n' */
  FMT_CHARACTER,                /* 'c' or 'C' */
  FMT_STRING                    /* 's' or 'S' */
};

static const UInt8 fmt_table[] = {
  /* 0x20 */
  FMT_SPACE,     FMT_UNKNOWN,    FMT_UNKNOWN,    FMT_HASH,
  FMT_POSITION,  FMT_PERCENT,    FMT_UNKNOWN,    FMT_QUOTE,
  FMT_UNKNOWN,   FMT_UNKNOWN,    FMT_WIDTH_AST,  FMT_PLUS,
  FMT_UNKNOWN,   FMT_MINUS,      FMT_PRECISION,  FMT_UNKNOWN,
  /* 0x30 */
  FMT_ZERO,      FMT_NUMBER,     FMT_NUMBER,     FMT_NUMBER,
  FMT_NUMBER,    FMT_NUMBER,     FMT_NUMBER,     FMT_NUMBER,
  FMT_NUMBER,    FMT_NUMBER,     FMT_UNKNOWN,    FMT_UNKNOWN,
  FMT_UNKNOWN,   FMT_UNKNOWN,    FMT_UNKNOWN,    FMT_UNKNOWN,
  /* 0x40 */
  FMT_OBJECT,    FMT_DOUBLEHEX,  FMT_UNKNOWN,    FMT_CHARACTER,
  FMT_INTEGER,   FMT_DOUBLE,     FMT_DOUBLE,     FMT_DOUBLE,
  FMT_UNKNOWN,   FMT_UNKNOWN,    FMT_UNKNOWN,    FMT_UNKNOWN,
  FMT_MOD_LONG,  FMT_UNKNOWN,    FMT_UNKNOWN,    FMT_OCTAL,
  /* 0x50 */
  FMT_UNKNOWN,   FMT_UNKNOWN,    FMT_UNKNOWN,    FMT_STRING,
  FMT_UNKNOWN,   FMT_UINTEGER,   FMT_UNKNOWN,    FMT_UNKNOWN,
  FMT_HEX,       FMT_UNKNOWN,    FMT_UNKNOWN,    FMT_UNKNOWN,
  FMT_UNKNOWN,   FMT_UNKNOWN,    FMT_UNKNOWN,    FMT_UNKNOWN,
  /* 0x60 */
  FMT_UNKNOWN,   FMT_DOUBLEHEX,  FMT_UNKNOWN,    FMT_CHARACTER,
  FMT_INTEGER,   FMT_DOUBLE,     FMT_DOUBLE,     FMT_DOUBLE,
  FMT_MOD_SHORT, FMT_INTEGER,    FMT_MOD_INTMAX, FMT_UNKNOWN,
  FMT_MOD_LONG,  FMT_UNKNOWN,    FMT_GETCOUNT,   FMT_OCTAL,
  /* 0x70 */
  FMT_POINTER,   FMT_QUADWORD,   FMT_UNKNOWN,    FMT_STRING,
  FMT_MOD_PTRDIFF, FMT_UINTEGER, FMT_UNKNOWN,    FMT_UNKNOWN,
  FMT_HEX,       FMT_UNKNOWN,    FMT_MOD_SIZE,   FMT_UNKNOWN,
  FMT_UNKNOWN,   FMT_UNKNOWN,    FMT_UNKNOWN,    FMT_UNKNOWN
};

/* Step 0: At the beginning of the spec */
#define STEP_0_JUMP(_fmt) do \
{ \
  type = (fmt < fmtlimit) ? *fmt++ : 0; \
  switch ((type >= 0x20 && type <= 0x7A) ? fmt_table[type - 0x20] : 0) \
    { \
      case FMT_UNKNOWN:     goto handle_error; \
      case FMT_SPACE:       goto flag_space_prefix; \
      case FMT_HASH:        goto flag_alternate; \
      case FMT_QUOTE:       goto flag_grouping; \
      case FMT_PLUS:        goto flag_show_sign; \
      case FMT_MINUS:       goto flag_left_align; \
      case FMT_ZERO:        goto flag_pad_zeros; \
      case FMT_NUMBER:      goto width_number; \
      case FMT_POSITION:    goto get_position; \
      case FMT_WIDTH_AST:   goto width_asterisk; \
      case FMT_PRECISION:   goto precision; \
      case FMT_MOD_SHORT:   goto mod_short; \
      case FMT_MOD_LONG:    goto mod_long; \
      case FMT_MOD_SIZE:    goto mod_size_t; \
      case FMT_MOD_PTRDIFF: goto mod_ptrdiff_t; \
      case FMT_MOD_INTMAX:  goto mod_intmax_t; \
      case FMT_PERCENT:     goto fmt_percent; \
      case FMT_OBJECT:      goto fmt_object; \
      case FMT_POINTER:     goto fmt_pointer; \
      case FMT_INTEGER:     goto fmt_integer; \
      case FMT_OCTAL:       goto fmt_octal; \
      case FMT_HEX:         goto fmt_hex; \
      case FMT_UINTEGER:    goto fmt_uinteger; \
      case FMT_GETCOUNT:    goto fmt_getcount; \
      case FMT_DOUBLE:      goto fmt_double; \
      case FMT_DOUBLEHEX:   goto fmt_doublehex; \
      case FMT_CHARACTER:   goto fmt_character; \
      case FMT_STRING:      goto fmt_string; \
    } \
} while (0)

/* Step 1: After reading flags and/or width */
#define STEP_1_JUMP(_fmt) do \
{ \
  UniChar _c = *fmt++; \
  switch (((_fmt) <= fmtlimit && (_c >= 0x20 && _c <= 0x7A)) ? \
          fmt_table[_c - 0x20] : 0) \
    { \
      case FMT_UNKNOWN:     goto handle_error; \
      case FMT_SPACE:       goto handle_error; \
      case FMT_HASH:        goto handle_error; \
      case FMT_QUOTE:       goto handle_error; \
      case FMT_PLUS:        goto handle_error; \
      case FMT_MINUS:       goto handle_error; \
      case FMT_ZERO:        goto handle_error; \
      case FMT_NUMBER:      goto handle_error; \
      case FMT_POSITION:    goto get_position; \
      case FMT_WIDTH_AST:   goto handle_error; \
      case FMT_PRECISION:   goto precision; \
      case FMT_MOD_SHORT:   goto mod_short; \
      case FMT_MOD_LONG:    goto mod_long; \
      case FMT_MOD_SIZE:    goto mod_size_t; \
      case FMT_MOD_PTRDIFF: goto mod_ptrdiff_t; \
      case FMT_MOD_INTMAX:  goto mod_intmax_t; \
      case FMT_PERCENT:     goto fmt_percent; \
      case FMT_OBJECT:      goto fmt_object; \
      case FMT_POINTER:     goto fmt_pointer; \
      case FMT_INTEGER:     goto fmt_integer; \
      case FMT_OCTAL:       goto fmt_octal; \
      case FMT_HEX:         goto fmt_hex; \
      case FMT_UINTEGER:    goto fmt_uinteger; \
      case FMT_GETCOUNT:    goto fmt_getcount; \
      case FMT_DOUBLE:      goto fmt_double; \
      case FMT_DOUBLEHEX:   goto fmt_doublehex; \
      case FMT_CHARACTER:   goto fmt_character; \
      case FMT_STRING:      goto fmt_string; \
    } \
} while (0)

/* Step 2: After processing precision */
#define STEP_2_JUMP(_fmt) do \
{ \
  UniChar _c = *fmt++; \
  switch (((_fmt) <= fmtlimit && (_c >= 0x20 && _c <= 0x7A)) ? \
          fmt_table[_c - 0x20] : 0) \
    { \
      case FMT_UNKNOWN:     goto handle_error; \
      case FMT_SPACE:       goto handle_error; \
      case FMT_HASH:        goto handle_error; \
      case FMT_QUOTE:       goto handle_error; \
      case FMT_PLUS:        goto handle_error; \
      case FMT_MINUS:       goto handle_error; \
      case FMT_ZERO:        goto handle_error; \
      case FMT_NUMBER:      goto handle_error; \
      case FMT_POSITION:    goto handle_error; \
      case FMT_WIDTH_AST:   goto handle_error; \
      case FMT_PRECISION:   goto handle_error; \
      case FMT_MOD_SHORT:   goto mod_short; \
      case FMT_MOD_LONG:    goto mod_long; \
      case FMT_MOD_SIZE:    goto mod_size_t; \
      case FMT_MOD_PTRDIFF: goto mod_ptrdiff_t; \
      case FMT_MOD_INTMAX:  goto mod_intmax_t; \
      case FMT_PERCENT:     goto fmt_percent; \
      case FMT_OBJECT:      goto fmt_object; \
      case FMT_POINTER:     goto fmt_pointer; \
      case FMT_INTEGER:     goto fmt_integer; \
      case FMT_OCTAL:       goto fmt_octal; \
      case FMT_HEX:         goto fmt_hex; \
      case FMT_UINTEGER:    goto fmt_uinteger; \
      case FMT_GETCOUNT:    goto fmt_getcount; \
      case FMT_DOUBLE:      goto fmt_double; \
      case FMT_DOUBLEHEX:   goto fmt_doublehex; \
      case FMT_CHARACTER:   goto fmt_character; \
      case FMT_STRING:      goto fmt_string; \
    } \
} while (0)

/* Step 3L: After reading length modifier 'l' */
#define STEP_3L_JUMP(_fmt) do \
{ \
  UniChar _c = *fmt++; \
  switch (((_fmt) <= fmtlimit && (_c >= 0x20 && _c <= 0x7A)) ? \
          fmt_table[_c - 0x20] : 0) \
    { \
      case FMT_UNKNOWN:     goto handle_error; \
      case FMT_SPACE:       goto handle_error; \
      case FMT_HASH:        goto handle_error; \
      case FMT_QUOTE:       goto handle_error; \
      case FMT_PLUS:        goto handle_error; \
      case FMT_MINUS:       goto handle_error; \
      case FMT_ZERO:        goto handle_error; \
      case FMT_NUMBER:      goto handle_error; \
      case FMT_POSITION:    goto handle_error; \
      case FMT_WIDTH_AST:   goto handle_error; \
      case FMT_PRECISION:   goto handle_error; \
      case FMT_MOD_SHORT:   goto handle_error; \
      case FMT_MOD_LONG:    goto mod_longlong; \
      case FMT_MOD_SIZE:    goto handle_error; \
      case FMT_MOD_PTRDIFF: goto handle_error; \
      case FMT_MOD_INTMAX:  goto handle_error; \
      case FMT_PERCENT:     goto fmt_percent; \
      case FMT_OBJECT:      goto fmt_object; \
      case FMT_POINTER:     goto fmt_pointer; \
      case FMT_INTEGER:     goto fmt_integer; \
      case FMT_OCTAL:       goto fmt_octal; \
      case FMT_HEX:         goto fmt_hex; \
      case FMT_UINTEGER:    goto fmt_uinteger; \
      case FMT_GETCOUNT:    goto fmt_getcount; \
      case FMT_DOUBLE:      goto fmt_double; \
      case FMT_DOUBLEHEX:   goto fmt_doublehex; \
      case FMT_CHARACTER:   goto fmt_character; \
      case FMT_STRING:      goto fmt_string; \
    } \
} while (0)

/* Step 3H: After reading length modifier 'h' */
#define STEP_3H_JUMP(_fmt) do \
{ \
  UniChar _c = *fmt++; \
  switch (((_fmt) <= fmtlimit && (_c >= 0x20 && _c <= 0x7A)) ? \
          fmt_table[_c - 0x20] : 0) \
    { \
      case FMT_UNKNOWN:     goto handle_error; \
      case FMT_SPACE:       goto handle_error; \
      case FMT_HASH:        goto handle_error; \
      case FMT_QUOTE:       goto handle_error; \
      case FMT_PLUS:        goto handle_error; \
      case FMT_MINUS:       goto handle_error; \
      case FMT_ZERO:        goto handle_error; \
      case FMT_NUMBER:      goto handle_error; \
      case FMT_POSITION:    goto handle_error; \
      case FMT_WIDTH_AST:   goto handle_error; \
      case FMT_PRECISION:   goto handle_error; \
      case FMT_MOD_SHORT:   goto mod_char; \
      case FMT_MOD_LONG:    goto handle_error; \
      case FMT_MOD_SIZE:    goto handle_error; \
      case FMT_MOD_PTRDIFF: goto handle_error; \
      case FMT_MOD_INTMAX:  goto handle_error; \
      case FMT_PERCENT:     goto fmt_percent; \
      case FMT_OBJECT:      goto fmt_object; \
      case FMT_POINTER:     goto fmt_pointer; \
      case FMT_INTEGER:     goto fmt_integer; \
      case FMT_OCTAL:       goto fmt_octal; \
      case FMT_HEX:         goto fmt_hex; \
      case FMT_UINTEGER:    goto fmt_uinteger; \
      case FMT_GETCOUNT:    goto fmt_getcount; \
      case FMT_DOUBLE:      goto fmt_double; \
      case FMT_DOUBLEHEX:   goto fmt_doublehex; \
      case FMT_CHARACTER:   goto fmt_character; \
      case FMT_STRING:      goto fmt_string; \
    } \
} while (0)

/* Step 4: Read the conversion specifier */
#define STEP_4_JUMP(_fmt) do \
{ \
  UniChar _c = *fmt++; \
  switch (((_fmt) <= fmtlimit && (_c >= 0x20 && _c <= 0x7A)) ? \
          fmt_table[_c - 0x20] : 0) \
    { \
      case FMT_UNKNOWN:     goto handle_error; \
      case FMT_SPACE:       goto handle_error; \
      case FMT_HASH:        goto handle_error; \
      case FMT_QUOTE:       goto handle_error; \
      case FMT_PLUS:        goto handle_error; \
      case FMT_MINUS:       goto handle_error; \
      case FMT_ZERO:        goto handle_error; \
      case FMT_NUMBER:      goto handle_error; \
      case FMT_POSITION:    goto handle_error; \
      case FMT_WIDTH_AST:   goto handle_error; \
      case FMT_PRECISION:   goto handle_error; \
      case FMT_MOD_SHORT:   goto handle_error; \
      case FMT_MOD_LONG:    goto handle_error; \
      case FMT_MOD_SIZE:    goto handle_error; \
      case FMT_MOD_PTRDIFF: goto handle_error; \
      case FMT_MOD_INTMAX:  goto handle_error; \
      case FMT_PERCENT:     goto fmt_percent; \
      case FMT_OBJECT:      goto fmt_object; \
      case FMT_POINTER:     goto fmt_pointer; \
      case FMT_INTEGER:     goto fmt_integer; \
      case FMT_OCTAL:       goto fmt_octal; \
      case FMT_HEX:         goto fmt_hex; \
      case FMT_UINTEGER:    goto fmt_uinteger; \
      case FMT_GETCOUNT:    goto fmt_getcount; \
      case FMT_DOUBLE:      goto fmt_double; \
      case FMT_DOUBLEHEX:   goto fmt_doublehex; \
      case FMT_CHARACTER:   goto fmt_character; \
      case FMT_STRING:      goto fmt_string; \
    } \
} while (0)

CF_INLINE CFIndex
_ustring_length (const UniChar * string, size_t maxlen)
{
  size_t len;
#if HOST_OS_WINDOWS
  if (maxlen < 0)
    maxlen = STRSAFE_MAX_CCH * sizeof (UniChar);
  return SUCCEEDED (StringCbLengthW (string, maxlen, &len)) ? len : 0;
#elif (SIZEOF_WCHAR_T == SIZEOF_UNICHAR) && HAVE_WCHAR_H
  if (maxlen < 0)
    len = wcslen (string);
  else
    len = wcsnlen (string, maxlen);
#else
  if (maxlen < 0)
    maxlen = INT_MAX;
  /* We test 64-bits at a time */
  for (len = 0; maxlen != 0 && string[len] != '\0'; ++len, --maxlen)
    {
      if (--maxlen == 0 || string[++len] == '\0')
        break;
      if (--maxlen == 0 || string[++len] == '\0')
        break;
      if (--maxlen == 0 || string[++len] == '\0')
        break;
    }
  return len;
#endif
}

CF_INLINE CFIndex
_cstring_length (const char *string, size_t maxlen)
{
  size_t len;
#if HOST_OS_WINDOWS
  if (maxlen < 0)
    maxlen = STRSAFE_MAX_CCH * sizeof (char);
  return SUCCEEDED (StringCbLengthA (string, maxlen, &len)) ? len : 0;
#elif HAVE_STRING_H
  if (maxlen < 0)
    len = strlen (string);
  else
    len = strnlen (string, maxlen);
  return len;
#else
  if (maxlen < 0)
    maxlen = INT_MAX;
  /* We test 64-bits at a time */
  for (len = 0; maxlen != 0 && string[len] != '\0'; ++len, --maxlen)
    {
      if (--maxlen == 0 || string[++len] == '\0')
        break;
      if (--maxlen == 0 || string[++len] == '\0')
        break;
      if (--maxlen == 0 || string[++len] == '\0')
        break;
      if (--maxlen == 0 || string[++len] == '\0')
        break;
      if (--maxlen == 0 || string[++len] == '\0')
        break;
      if (--maxlen == 0 || string[++len] == '\0')
        break;
      if (--maxlen == 0 || string[++len] == '\0')
        break;
    }
  return len;
#endif
}

/* format MUST already be pointing to a digit */
static int
_read_number (const UniChar ** __restrict__ format)
{
  int number;

  number = *(*format)++ - '0';
  while (**format >= '0' && **format <= '9')
    number = (number * 10) + (*(*format++) - '0');

  return number;
}

static const UniChar _lookup_upper[] = { '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

static const UniChar _lookup_lower[] = { '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

CF_INLINE UniChar *
_uint_to_string (unsigned long long int value, UniChar * bufend,
                 int base, Boolean uppercase)
{
  UniChar *cur = bufend;
  const UniChar *lookup = uppercase ? _lookup_upper : _lookup_lower;
  switch (base)
    {
    case 8:
      do
        *--cur = lookup[value & 7];
      while ((value >>= 3));
      break;
    case 10:
      do
        *--cur = lookup[value % base];
      while ((value /= base));
      break;
    case 16:
      do
        *--cur = lookup[value & 15];
      while ((value >>= 4));
      break;
    }
  return cur;
}

#define _write(_s, _n) do \
{ \
  int _left = output_bufend - output_buffer; \
  int _towrite = (_n); \
  if (_left > 0) \
    GSMemoryCopy (output_buffer, (_s), _left < _towrite ? _left : _towrite); \
  output_buffer += _towrite; \
} while (0)

#define PAD_SIZE 8
static const UniChar _pad_space[] = { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' };
static const UniChar _pad_zero[] = { '0', '0', '0', '0', '0', '0', '0', '0' };

#define _pad(_padchar, _count) do \
{ \
  if ((_count) > 0) \
    { \
      int i; \
      const UniChar *pad_string; \
      if ((_padchar) == ' ') \
        pad_string = _pad_space; \
      else \
        pad_string = _pad_zero; \
      for (i = (_count); i >= PAD_SIZE; i -= PAD_SIZE) \
        _write ((char *) pad_string, PAD_SIZE); \
      if (i > 0) \
        _write ((char *) pad_string, i); \
    } \
} while (0)

CF_INLINE format_argument_t *
GSUnicodeCreateArgumentList (const UniChar * __restrict__ format,
                             CFIndex fmtlen, va_list ap)
{
  int max;
  int idx;
  const UniChar *fmt;
  const UniChar *fmtlimit;
  format_argument_t *arglist;
  UInt8 *lengthlist;
  UInt8 *typelist;

  max = 0;
  fmt = format;
  fmtlimit = fmt + fmtlen;
  /* Find the maximum number of arguments. */

  /* We'll first get to the first occurrence of '%' and make sure it includes
   * a position field.
   * POSIX.1-2008 allows the following pattern: '%n$[*m$][.*o$]<type>'
   * http://pubs.opengroup.org/onlinepubs/9699919799/functions/printf.html
   */
  while (fmt < fmtlimit)
    {
      int pos;
      Boolean doingposition;

      doingposition = true;
      while (fmt < fmtlimit && *fmt != '%')
        fmt++;
      if (!(fmt < fmtlimit))
        break;
      fmt++;
      if (*fmt == '%')          /* Search again if we find a '%%' pattern. */
        continue;
      /* Must start with 1 - 9 or it is not a position argument */
      if (!(*fmt >= '1' && *fmt <= '9'))
        return NULL;

      /* Get the maximum position */
      while (fmt < fmtlimit)
        {
          switch (*fmt)
            {
              /* Skip flags */
            case ' ':
            case '#':
            case '\'':
            case '+':
            case '-':
            case '0':
              fmt++;
              continue;
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
              if (doingposition == true)
                {
                  pos = _read_number (&fmt);
                  /* If there isn't a '$' at this point, there is an error in
                   * the specification.  Return NULL and the error will be
                   * caught when doing the real parsing later.
                   */
                  if (*fmt != '$')
                    return NULL;
                  if (pos > max)
                    max = pos;
                  doingposition = false;        /* Position already done. */
                }
              fmt++;
              continue;
            case '.':
              /* Precision is the last possibility for a position argument.
               * If it is not '*' we can just move on.
               */
              if (*++fmt != '*')
                break;
            case '*':
              fmt++;
              doingposition = true;
              continue;
            default:
              break;
            }
          break;
        }
    }
  if (max <= 0)
    return NULL;

  arglist =
    (format_argument_t *) CFAllocatorAllocate (kCFAllocatorSystemDefault,
                                               sizeof (format_argument_t) * max,
                                               0);
  lengthlist =
    (UInt8 *) CFAllocatorAllocate (kCFAllocatorSystemDefault,
                                   sizeof (UInt8) * max, 0);
  typelist =
    (UInt8 *) CFAllocatorAllocate (kCFAllocatorSystemDefault,
                                   sizeof (UInt8) * max, 0);
  if (arglist == NULL || lengthlist == NULL || typelist == NULL)
    {
      if (arglist != NULL)
        CFAllocatorDeallocate (kCFAllocatorSystemDefault, arglist);
      if (lengthlist != NULL)
        CFAllocatorDeallocate (kCFAllocatorSystemDefault, lengthlist);
      if (typelist != NULL)
        CFAllocatorDeallocate (kCFAllocatorSystemDefault, typelist);
      return NULL;
    }
  GSMemoryZero (lengthlist, sizeof (UInt8) * max);
  GSMemoryZero (typelist, sizeof (UInt8) * max);

  /* Gather all the length and type. */
  fmt = format;
  while (1)
    {
      int pos;

      while (fmt < fmtlimit && *fmt != '%')
        fmt++;
      if (!(fmt < fmtlimit))
        break;
      fmt++;
      if (*fmt == '%')
        {
          fmt++;
          continue;
        }

      /* Read position parameter
       * The previous loop already established that the spec is well formed,
       * so we do not need to check it again.
       */
      pos = _read_number (&fmt) - 1;
      fmt++;                    /* Skip '$' */

      /* Skip over any flags */
      while (fmt < fmtlimit)
        {
          switch (*fmt)
            {
            case ' ':
            case '#':
            case '\'':
            case '+':
            case '-':
            case '0':
              fmt++;
              continue;
            default:
              break;
            }
          break;
        }

      /* Width */
      if (fmt < fmtlimit && *fmt == '*')
        {
          int width_pos;

          width_pos = _read_number (&fmt) - 1;
          if (*fmt == '$')
            {
              lengthlist[width_pos] = FMT_MOD_INT;
              fmt++;
            }
        }
      else
        {
          while (fmt < fmtlimit && (*fmt >= '0' && *fmt <= '9'))
            fmt++;
        }
      /* Precision */
      if (fmt < fmtlimit && *fmt == '.')
        {
          fmt++;
          if (fmt < fmtlimit && *fmt++ == '*')
            {
              int prec_pos;

              prec_pos = -1;
              if (*fmt >= '1' && *fmt <= '9')
                {
                  prec_pos = _read_number (&fmt) - 1;
                  if (*fmt == '$')
                    {
                      lengthlist[prec_pos] = FMT_MOD_INT;
                      fmt++;
                    }
                }
            }
          else
            {
              while (fmt < fmtlimit && (*fmt >= '0' && *fmt <= '9'))
                fmt++;
            }
        }
      if (!(fmt < fmtlimit))    /* Double check */
        goto args_handle_error;

      /* Length */
      switch (*fmt++)
        {
        case 'L':
          lengthlist[pos] = FMT_MOD_LONG;
          break;
        case 'h':
          if (*fmt == 'h')
            {
              fmt++;
              lengthlist[pos] = FMT_MOD_CHAR;
            }
          else
            {
              lengthlist[pos] = FMT_MOD_SHORT;
            }
          break;
        case 'j':
          lengthlist[pos] = FMT_MOD_INTMAX;
          break;
        case 'l':
          if (*fmt == 'l')
            {
              ++fmt;
              lengthlist[pos] = FMT_MOD_LONGLONG;
            }
          else
            {
              lengthlist[pos] = FMT_MOD_LONG;
            }
          break;
        case 't':
          lengthlist[pos] = FMT_MOD_PTRDIFF;
          break;
        case 'z':
          lengthlist[pos] = FMT_MOD_SIZE;
          break;
        default:
          fmt--;
          break;
        }

      /* Get the type */
      if (fmt < fmtlimit)
        {
          switch (*fmt++)
            {
            case 'C':
            case 'c':
            case 'd':
            case 'i':
            case 'o':
            case 'u':
            case 'X':
            case 'x':
              typelist[pos] = FMT_INTEGER;
              break;
            case 'A':
            case 'a':
            case 'E':
            case 'e':
            case 'F':
            case 'f':
            case 'G':
            case 'g':
              typelist[pos] = FMT_DOUBLE;
              break;
            case '@':
            case 'n':
            case 'p':
            case 'S':
            case 's':
              typelist[pos] = FMT_POINTER;
              break;
            default:
              goto args_handle_error;
            }
        }
    }

  /* Collect the arguments */
  for (idx = 0; idx < max; ++idx)
    {
      switch (typelist[idx])
        {
        case FMT_POINTER:
          arglist[idx].ptrValue = va_arg (ap, void *);
          break;
        case FMT_INTEGER:
          if (lengthlist[idx] == FMT_MOD_LONGLONG)
            arglist[idx].intValue = va_arg (ap, long long);
          else if (lengthlist[idx] == FMT_MOD_LONG)
            arglist[idx].lintValue = va_arg (ap, long);
          else
            arglist[idx].intValue = va_arg (ap, int);
          break;
        case FMT_DOUBLE:
#if SIZEOF_LONG_DOUBLE > SIZEOF_DOUBLE
          if (lengthlist[idx] == FMT_MOD_LONG)
            arglist[idx].ldblValue = va_arg (ap, long double);
          else
#endif
            arglist[idx].dblValue = va_arg (ap, double);
          break;
        default:
          goto args_handle_error;
        }
    }

  CFAllocatorDeallocate (kCFAllocatorSystemDefault, typelist);
  CFAllocatorDeallocate (kCFAllocatorSystemDefault, lengthlist);

  return arglist;

args_handle_error:
  CFAllocatorDeallocate (kCFAllocatorSystemDefault, arglist);
  CFAllocatorDeallocate (kCFAllocatorSystemDefault, typelist);
  CFAllocatorDeallocate (kCFAllocatorSystemDefault, lengthlist);

  return NULL;
}

CFIndex
GSUnicodeFormat (UniChar * __restrict__ s, CFIndex n, CFTypeRef locale,
                 const UniChar * __restrict__ format, CFIndex fmtlen, ...)
{
  CFIndex result;
  va_list ap;

  va_start (ap, fmtlen);
  result = GSUnicodeFormatWithArguments (s, n, locale, format, fmtlen, ap);
  va_end (ap);

  return result;
}

static const UniChar nil_string[] = { '(', 'n', 'i', 'l', ')' };

static const CFIndex nil_string_len = 5;

/* String Formatting function.
 * I used GLIBC's implementation of vfprintf() as a model.  This seemed like
 * the most sensible thing to do at the time.  The implementation is fairly
 * efficient in that it doesn't call a lot functions.  I still tried to
 * simplify it it even further but not separating between the slow
 * positional parameters path and regular path.
 * 
 * POSIX.1-2008 (http://pubs.opengroup.org/onlinepubs/9699919799/) allows
 * two possible styles of format strings:
 *   format-string  = non-positional / positional
 * 
 *   non-positional = "%" [flags] [width] [precision] [length] specifier
 *   positional     = "%" DIGIT "$" [flags] [pos-width] [pos-precision] [length] specifier
 * 
 *   flags          = *("'" / "-" / "+" / SP / "#" / "0")
 * 
 *   width          = *DIGIT / "*"
 *   pos-width      = *DIGIT / ("*" 1*DIGIT)
 * 
 *   precision      = "." (*DIGIT / "*")
 *   pos-precision  = "." *DIGIT / ("*" 1*DIGIT)
 * 
 *   length         = "hh" / "h" / "l" / "ll" / "j" / "z" / "t" / "L"
 * 
 *   specifier      = int-spec / float-spec / special-spec
 *   int-specifier  = "d" / "i" / "o" / "u" / "x" / "X"
 *   float-spec     = "f" / "F" / "e" / "E" / "g" / "G" / "a" / "A"
 *   special-spec   = "p" / "c" / "C" / "s" / "S" / "n" / "%"
 * 
 * -- Stefan
 */

CFIndex
GSUnicodeFormatWithArguments (UniChar * __restrict__ s, CFIndex n,
                              CFTypeRef locale,
                              const UniChar * __restrict__ format,
                              CFIndex fmtlen, va_list ap)
{
  UniChar *output_buffer;
  UniChar *output_bufend;
  const UniChar *fmt;
  const UniChar *fmtlimit;
  format_argument_t *arglist;

  if (fmtlen == 0)
    return 0;
  if (s == NULL)
    n = 0;

  output_buffer = s;
  output_bufend = s + n;
  fmt = format;
  fmtlimit = fmt + fmtlen;
  arglist = GSUnicodeCreateArgumentList (fmt, fmtlen, ap);
  while (fmt < fmtlimit)
    {
      const UniChar *prev;
      UniChar buffer[BUFFER_SIZE];
      UniChar *bufend = buffer + BUFFER_SIZE;
      UniChar *string;
      CFIndex string_len;
      Boolean free_string = false;
      int base;
      unsigned long long number;
      long double dbl_number;
      int position;
      Boolean is_negative;
      Boolean space_prefix = false;
      Boolean alternate = false;
      Boolean grouping = false;
      Boolean left_align = false;
      Boolean show_sign = false;
      Boolean pad_zeros = false;
      int width = 0;            /* 0 for not specified */
      int prec = -1;            /* -1 for not specified */
      int length = FMT_MOD_INT; /* Standard Length */
      UniChar type = 0;

      prev = fmt;
      while (fmt < fmtlimit && *fmt != '%')
        fmt++;
      _write (prev, fmt - prev);
      if (!(fmt < fmtlimit))
        break;
      fmt++;
      STEP_0_JUMP (fmt);

      /* Process the flags */
    flag_space_prefix:
      space_prefix = true;
      STEP_0_JUMP (fmt);
    flag_alternate:
      alternate = true;
      STEP_0_JUMP (fmt);
    flag_grouping:
      grouping = true;
      STEP_0_JUMP (fmt);
    flag_show_sign:
      show_sign = true;
      STEP_0_JUMP (fmt);
    flag_left_align:
      left_align = true;
      STEP_0_JUMP (fmt);
    flag_pad_zeros:
      pad_zeros = true;
      STEP_0_JUMP (fmt);

      /* Process width or position (if '$' exists at the end) */
    width_number:
      fmt--;                    /* STEP_)_JUMP would have moved fmt forward */
      width = _read_number (&fmt);
      if (width < 0)
        goto handle_error;
      STEP_1_JUMP (fmt);
    get_position:
      if (arglist == NULL)
        goto handle_error;
      position = width - 1;
      width = 0;
      STEP_0_JUMP (fmt);        /* This is a position field, so do step 0 again. */
    width_asterisk:
      fmt++;
      if (fmt < fmtlimit && (*fmt >= '1' && *fmt <= '9'))
        {
          int width_pos = _read_number (&fmt);
          if (fmt < fmtlimit && (*fmt == '$' && arglist != NULL))
            width = arglist[width_pos].intValue;
          else
            goto handle_error;
        }
      else
        {
          width = va_arg (ap, int);
        }
      STEP_1_JUMP (fmt);

      /* Process precision */
    precision:
      fmt++;
      if (fmt < fmtlimit)
        {
          if (*fmt == '*')
            {
              fmt++;
              if (fmt < fmtlimit && (*fmt >= '1' && *fmt <= '9'))
                {
                  int prec_pos = _read_number (&fmt);
                  if (fmt < fmtlimit && (*fmt == '$' && arglist != NULL))
                    prec = arglist[prec_pos].intValue;
                  else
                    goto handle_error;
                }
              else
                {
                  prec = va_arg (ap, int);
                }
            }
          else if (*fmt >= '0' && *fmt <= '9')
            {
              prec = _read_number (&fmt);
            }
          else
            {
              /* Treat %.? as %.0? */
              prec = 0;
            }
          STEP_2_JUMP (fmt);
        }
      else
        {
          goto handle_error;
        }

      /* Process length modifiers */
    mod_char:
      length = FMT_MOD_CHAR;
      STEP_4_JUMP (fmt);
    mod_short:
      length = FMT_MOD_SHORT;
      STEP_3H_JUMP (fmt);
    mod_long:
      length = FMT_MOD_LONG;
      STEP_3L_JUMP (fmt);
    mod_longlong:
      length = FMT_MOD_LONGLONG;
      STEP_4_JUMP (fmt);
    mod_size_t:
      length = FMT_MOD_SIZE;
      STEP_4_JUMP (fmt);
    mod_ptrdiff_t:
      length = FMT_MOD_PTRDIFF;
      STEP_4_JUMP (fmt);
    mod_intmax_t:
      length = FMT_MOD_INTMAX;
      STEP_4_JUMP (fmt);

      /* Process specification */
    fmt_percent:
      buffer[0] = '%';
      string = buffer;
      string_len = 1;
      goto print_string;

    fmt_object:
      {
        CFTypeRef o;
        o = arglist == NULL ? va_arg (ap, CFTypeRef) :
          arglist[position].ptrValue;
        if (o == NULL)
          {
            string = (UniChar *) nil_string;
            string_len = nil_string_len;
            goto print_string;
          }
        else
          {
            const CFRuntimeClass *cls;
            CFStringRef desc;
            Boolean free_string = false;

            cls = _CFRuntimeGetClassWithTypeID (CFGetTypeID (o));
            if (cls->copyFormattingDesc)
              desc = cls->copyFormattingDesc (o, locale);
            else
              desc = CFCopyDescription (o);
            string = (UniChar *) CFStringGetCharactersPtr (desc);
            string_len = CFStringGetLength (desc);
            if (string == NULL)
              {
                if (string_len <= BUFFER_SIZE)
                  {
                    string = buffer;
                    CFStringGetCharacters (desc, CFRangeMake (0, string_len),
                                           string);
                  }
                else
                  {
                    string = CFAllocatorAllocate (kCFAllocatorSystemDefault,
                                                  string_len * sizeof (UniChar),
                                                  0);
                    if (string == NULL)
                      {
                        CFRelease (desc);
                        goto handle_error;
                      }
                    free_string = true;
                    CFStringGetCharacters (desc, CFRangeMake (0, string_len),
                                           string);
                  }
              }
            if (left_align == false)
              _pad (' ', width);
            _write (string, string_len);
            if (left_align == true)
              _pad (' ', width);
            if (free_string)
              CFAllocatorDeallocate (kCFAllocatorSystemDefault, string);
            CFRelease (desc);
          }
      }
      continue;

    fmt_pointer:
      {
        const void *ptr;
        ptr = arglist == NULL ? va_arg (ap, void *) :
          arglist[position].ptrValue;
        if (ptr == NULL)
          {
            string = (UniChar *) nil_string;
            string_len = nil_string_len;
            goto print_string;
          }
        else
          {
            base = 16;
            alternate = true;
            is_negative = false;
            show_sign = false;
            space_prefix = false;
            type = 'x';
            number = (unsigned long long) ptr;
            goto fmt_signed_number;
          }
      }

    fmt_integer:
      {
        signed long long int signed_number;

        if (arglist == NULL)
          {
            if (length == FMT_MOD_INT)
              signed_number = va_arg (ap, int);
            else if (length == FMT_MOD_LONG)
              signed_number = va_arg (ap, long int);
            else if (length == FMT_MOD_LONGLONG)
              signed_number = va_arg (ap, long long int);
            else if (length == FMT_MOD_SHORT)
              signed_number = (short int) va_arg (ap, int);
            else                /* Must be FMT_MOD_CHAR */
              signed_number = (char) va_arg (ap, int);
          }
        else
          {
            if (length == FMT_MOD_INT)
              signed_number = arglist[position].intValue;
            else if (length == FMT_MOD_LONG)
              signed_number = arglist[position].lintValue;
            else if (length == FMT_MOD_LONGLONG)
              signed_number = arglist[position].llintValue;
            else if (length == FMT_MOD_SHORT)
              signed_number = (short int) arglist[position].intValue;
            else                /* Must be FMT_MOD_CHAR */
              signed_number = (char) arglist[position].intValue;
          }
        is_negative = (signed_number < 0);
        number = is_negative ? (-signed_number) : signed_number;
        base = 10;
      }
      goto fmt_signed_number;

    fmt_octal:
      base = 8;
      goto fmt_unsigned_number;

    fmt_hex:
      base = 16;
      goto fmt_unsigned_number;

    fmt_uinteger:
      base = 10;

    fmt_unsigned_number:
      if (arglist == NULL)
        {
          if (length == FMT_MOD_INT)
            number = va_arg (ap, unsigned int);
          else if (length == FMT_MOD_LONG)
            number = va_arg (ap, unsigned long int);
          else if (length == FMT_MOD_LONGLONG)
            number = va_arg (ap, unsigned long long int);
          else if (length == FMT_MOD_SHORT)
            number = (unsigned short int) va_arg (ap, unsigned int);
          else                  /* Must be FMT_MOD_CHAR */
            number = (unsigned char) va_arg (ap, unsigned int);
        }
      else
        {
          if (length == FMT_MOD_INT)
            number = arglist[position].intValue;
          else if (length == FMT_MOD_LONG)
            number = arglist[position].lintValue;
          else if (length == FMT_MOD_LONGLONG)
            number = arglist[position].llintValue;
          else if (length == FMT_MOD_SHORT)
            number = (unsigned short int) arglist[position].intValue;
          else                  /* Must be FMT_MOD_CHAR */
            number = (unsigned char) arglist[position].intValue;
        }

      is_negative = false;
      show_sign = false;
      space_prefix = false;
      if (prec == 0 && number == 0)
        {
          /* If number and precision are zero we print nothing, unless
           * we are formatting an octal value with the alternate flag.
           */
          string = bufend;
          if (base == 8 && alternate)
            *--string = '0';
        }
      else
        {
        fmt_signed_number:
          if (base == 10 && locale != NULL)
            {
              /* This is a slower than using _uint_to_string () because
               * we have to go through the trouble of opening a formatter.
               * That's why we only do it if a locale is specified.
               */
              string = NULL;
            }
          else
            {
              /* This is the "fast" integer formatting path. */
              string = _uint_to_string (number, bufend, base, type == 'X');

              string_len = bufend - string;

              if (base == 8 && alternate)
                *--string = '0';

              if (is_negative)
                *--string = '-';
              else if (show_sign)
                *--string = '+';
              else if (space_prefix)
                *--string = ' ';
            }
        }
      string_len = bufend - string;
      goto print_string;

    fmt_getcount:
      {
        int written = output_buffer - s;
        if (arglist == NULL)
          {
            if (length == FMT_MOD_INT)
              *(int *) va_arg (ap, void *) = written;
            else if (length == FMT_MOD_LONG)
              *(long int *) va_arg (ap, void *) = written;
            else if (length == FMT_MOD_LONGLONG)
              *(long long int *) va_arg (ap, void *) = written;
            else if (length == FMT_MOD_SHORT)
              *(short int *) va_arg (ap, void *) = written;
            else                  /* Must be FMT_MOD_CHAR */
              *(char *) va_arg (ap, void *) = written;
          }
        else
          {
            if (length == FMT_MOD_INT)
              *(int *) arglist[position].ptrValue = written;
            else if (length == FMT_MOD_LONG)
              *(long int *) arglist[position].ptrValue = written;
            else if (length == FMT_MOD_LONGLONG)
              *(long long int *) arglist[position].ptrValue = written;
            else if (length == FMT_MOD_SHORT)
              *(short int *) arglist[position].ptrValue = written;
            else                  /* Must be FMT_MOD_CHAR */
              *(char *) arglist[position].ptrValue = written;
          }
      }
      continue;

    fmt_double:
#if SIZEOF_LONG_DOUBLE > SIZEOF_DOUBLE
      if (length == FMT_MOD_LONG)
        {
          if (arglist == NULL)
            dbl_number = va_arg (ap, long double);
          else
            dbl_number = arglist[position].ldblValue;
          else
        }
#endif
      {
        if (arglist == NULL)
          dbl_number = va_arg (ap, double);
        else
          dbl_number = arglist[position].dblValue;
      }

    fmt_doublehex:

    fmt_character:
      if (length == FMT_MOD_LONG || type == 'C')
        {
          if (arglist == NULL)
            buffer[0] = (UniChar) va_arg (ap, int);
          else
            buffer[0] = (UniChar) arglist[position].intValue;
          string = buffer;
        }
      else
        {
          if (arglist == NULL)
            buffer[0] = (char) va_arg (ap, int);
          else
            buffer[0] = (char) arglist[position].intValue;
          string = buffer;
        }
      string_len = 1;
      goto print_string;

    fmt_string:
      string = arglist == NULL ? va_arg (ap, UniChar *) :
        arglist[position].ptrValue;
      if (length == FMT_MOD_LONG || type == 'S')
        {
          string_len = _ustring_length (string, prec);
        }
      else
        {
          const char *cstring = (const char *) string;
          string_len = _cstring_length (cstring, prec);
          if (string_len <= BUFFER_SIZE)
            {
              string = buffer;
            }
          else
            {
              string = CFAllocatorAllocate (kCFAllocatorSystemDefault,
                                            string_len * sizeof (UniChar), 0);
              free_string = true;
            }
          GSToUnicode ((const UInt8 *) cstring, string_len,
                       CFStringGetSystemEncoding (), 0, false, string,
                       string_len, &string_len);
        }

    print_string:
      width -= string_len;
      if (left_align == false)
        _pad (' ', width);
      _write (string, string_len);
      if (left_align == true)
        _pad (' ', width);
      if (free_string == true)
        CFAllocatorDeallocate (kCFAllocatorSystemDefault, string);
    }

  if (arglist != NULL)
    CFAllocatorDeallocate (kCFAllocatorSystemDefault, arglist);

  return output_buffer - s;

handle_error:
  if (arglist != NULL)
    CFAllocatorDeallocate (kCFAllocatorSystemDefault, arglist);

  return -1;
}
