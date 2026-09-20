#include "string.h"

#include <stddef.h> // for NULL

/*

 * See "man 3 strlen"

 */

int strlen(const char *str)
{

    // count characters up to, but not including, the terminator

    int len = 0;

    while (str[len] != '\0')
    {

        len++;
    }

    return len;
}

/*

 * See "man 3 strstr"

 */

char *strstr(const char *haystack, const char *needle)
{

    if (*needle == '\0')
    {

        return (char *)haystack;
    }

    long haystack_len = strlen(haystack);

    // try to match the needle starting at each position in the haystack

    for (long i = 0; i < haystack_len; i++)
    {

        long j = 0;

        while (needle[j] != '\0' && needle[j] == haystack[i + j])
        {

            j++;
        }

        // hit the end of the needle, so every character matched

        if (needle[j] == '\0')
        {

            return (char *)haystack + i;
        }
    }

    return NULL;
}

/*

 * See "man 3 strncat"

 */

char *strncat(char *dest, const char *src, long n)
{

    // append starting at dest's terminato -- overwriting it

    long start_src = strlen(dest);

    long i = 0;

    while (src[i] != '\0' && i < n)
    {

        dest[start_src + i] = src[i];

        i++;
    }

    dest[start_src + i] = '\0';

    return dest;
}

/*

 * See "man 3 strncmp"

 */

int strncmp(const char *s1, const char *s2, long n)
{

    // stop at the first difference, or when either string ends

    for (long i = 0; i < n; i++)
    {

        if (s1[i] != s2[i] || s1[i] == '\0' || s2[i] == '\0')
        {

            // cast to unsigned char so sign is defined

            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
    }

    // the first n characters are identical

    return 0;
}
