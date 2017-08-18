#include <linux/uaccess.h>

/* out-of-line parts */

#ifndef INLINE_COPY_FROM_USER
unsigned long _copy_from_user(void *to, const void __user *from, unsigned long n)
{
	unsigned long res = n;
	if (likely(access_ok(VERIFY_READ, from, n)))
		res = raw_copy_from_user(to, from, n);
	if (unlikely(res))
		memset(to + (n - res), 0, res);
	return res;
}
EXPORT_SYMBOL(_copy_from_user);
#endif

#ifndef INLINE_COPY_TO_USER
unsigned long _copy_to_user(void *to, const void __user *from, unsigned long n)
{
	if (likely(access_ok(VERIFY_WRITE, to, n)))
		n = raw_copy_to_user(to, from, n);
	return n;
}
EXPORT_SYMBOL(_copy_to_user);
#endif
