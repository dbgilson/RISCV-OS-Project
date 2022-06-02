#include<memhelp.h>

//This function sets the first number of bytes (size) of a destination (dst)
//to be equal to the given data (data).  Returns pointer to destination.
void *memset(void *dst, char data, int64_t size)
{
	int64_t i;
	char *cdst = (char *)dst;

	for (i = 0; i < size; i++)
	{
		cdst[i] = data;
	}

	return dst;
}

//This function copies a certain amount of bytes (size) from a source (src) to
//a destination (dst).  Returns the destination.
void *memcpy(void *dst, const void *src, int64_t size)
{
	int64_t i;
	char *cdst = (char *)dst;
	const char *const csrc = (char *)src;

	for (i = 0; i < size; i++)
	{
		cdst[i] = csrc[i];
	}

	return dst;
}

//This function compares 2 different memory locations and sees if the
//bytes specified by size are equivalent (returns true/false).
bool memcmp(const void *haystack, const void *needle, int64_t size)
{
	const char *hay = (char *)haystack;
	const char *need = (char *)needle;
	int64_t i;

	for (i = 0; i < size; i++)
	{
		if (hay[i] != need[i])
		{
			return false;
		}
	}

	return true;
}
