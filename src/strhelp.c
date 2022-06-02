#include <strhelp.h>

int atoi(const char *st)
{
	int r = 0;
	int p = 1;
	int i;
	int l = 0;
	int n = 0;

	if (st[0] == '-')
	{
		st++;
		n = 1;
	}

	while (st[l] >= '0' && st[l] <= '9')
		l++;

	for (i = l - 1; i >= 0; i--)
	{
		r += p * (st[i] - '0');
		p *= 10;
	}

	return (n ? -r : r);
}

int strcmp(const char *l, const char *r)
{
	const unsigned char *s1 = (const unsigned char *)l;
	const unsigned char *s2 = (const unsigned char *)r;
	unsigned int c1, c2;

	do
	{
		c1 = (unsigned char)*s1++;
		c2 = (unsigned char)*s2++;
		if (c1 == '\0')
			return c1 - c2;
	} while (c1 == c2);

	return c1 - c2;
}

int strncmp(const char *left, const char *right, int n)
{
	unsigned int c1 = '\0';
	unsigned int c2 = '\0';

	if (n >= 4)
	{
		int n4 = n >> 2;
		do
		{
			c1 = (unsigned char)*left++;
			c2 = (unsigned char)*right++;
			if (c1 == '\0' || c1 != c2) {
				return c1 - c2;
			}

			c1 = (unsigned char)*left++;
			c2 = (unsigned char)*right++;
			if (c1 == '\0' || c1 != c2) {
				return c1 - c2;
			}
			c1 = (unsigned char)*left++;
			c2 = (unsigned char)*right++;
			if (c1 == '\0' || c1 != c2) {
				return c1 - c2;
			}
			c1 = (unsigned char)*left++;
			c2 = (unsigned char)*right++;
			if (c1 == '\0' || c1 != c2) {
				return c1 - c2;
			}
		} while (--n4 > 0);
		n &= 3;
	}

	while (n > 0)
	{
		c1 = (unsigned char)*left++;
		c2 = (unsigned char)*right++;
		if (c1 == '\0' || c1 != c2)
			return c1 - c2;
		n--;
	}

	return c1 - c2;
}

int strfindchr(const char *r, char t)
{
	int i = 0;
	while (r[i] != t)
	{
		if (r[i] == '\0')
		{
			return -1;
		}
		i++;
	}
	return i;
}

int strlen(const char *s)
{
	int len = 0;
	while (s[len] && ++len)
		;
	return len;
}

char *strcpy(char *dest, const char *s)
{
    char *o = dest;
    while (*s) {
        *dest++ = *s++;
    }
    *dest = '\0';
    return o;
}

char *strchr(const char *s, int c)
{
    while (*s != (char)c)
        if (!*s++)
            return 0;
    return (char *)s;
}