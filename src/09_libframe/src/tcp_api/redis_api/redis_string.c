//
//  redis_string.c
//  supex
//
//  Created by 周凯 on 16/1/11.
//  Copyright © 2016年 zhoukai. All rights reserved.
//

#include "tcp_api/redis_api/redis_string.h"

ssize_t redis_string_append(struct redis_string *req, const char *field, int size)
{
	ssize_t b = 0;

	ASSERTOBJ(req);
	
	if (!field) {
		b = cache_appendf(req->cache, "$%d\r\n", -1);
		return_val_if_fail(b > 0, -1);
	} else {
		return_val_if_fail(size != 0, 0);
		size = unlikely(size < 0) ? (int)strlen(field) : size;
		
		b = cache_appendf(req->cache, "$%d\r\n", size);
		return_val_if_fail(b > 0, -1);
		
		b = cache_append(req->cache, field, size);
		return_val_if_fail(b > 0, -1);
		
		b = cache_append(req->cache, "\r\n", 2);
		return_val_if_fail(b > 0, -1);
	}
	
	req->fields++;
	
	return b;
}

ssize_t redis_string_appendf(struct redis_string *req, const char *fmt, ...)
{
	ssize_t b = 0;

	ASSERTOBJ(req);
	assert(fmt);

	struct cache    tmp = {};
	bool            flag = false;
	flag = cache_init(&tmp);
	return_val_if_fail(flag, -1);

	va_list ap;
	va_start(ap, fmt);
	b = cache_vappendf(&tmp, fmt, ap);
	va_end(ap);

	if (likely(b > 0)) {
		b = cache_insertf(&tmp, "$%zd\r\n", b);

		if (likely(b > 0)) {
			b = cache_append(&tmp, "\r\n", 2);

			if (likely(b > 0)) {
				b = cache_append(req->cache, cache_data_address(&tmp),
						cache_data_length(&tmp));

				if (likely(b > 0)) {
					req->fields++;
				}
			}
		}
	}
	
	cache_clean(&tmp);
	cache_finally(&tmp);

	return b;
}

ssize_t redis_string_complete(struct redis_string *req)
{
	ssize_t b = 0;

	ASSERTOBJ(req);
	ASSERTOBJ(req->cache);
	return_val_if_fail(req->fields, 0);

	unsigned l = req->cache->end - req->cache->start;
	return_val_if_fail(l, 0);
	
	b = cache_insertf(req->cache, "*%u\r\n", req->fields);
	return_val_if_fail(b > 0, -1);
	
	UNREFOBJ(req);
	
	return cache_data_length(req->cache);
}

ssize_t redis_string_bulk(struct cache *cache, const char *fmt, ...)
{
	if (fmt == NULL) {
		return cache_append(cache, "$-1\r\n", -1);
	} else if (fmt[0] == '\0') {
		return cache_append(cache, "$0\r\n", -1);
	}

	struct cache    buff = {};
	ssize_t         ret = -1;
	va_list         ap;

	cache_init(&buff);
	va_start(ap, fmt);
	ret = cache_vappendf(&buff, fmt, ap);
	va_end(ap);

	if (likely(ret > 0)) {
		ret = cache_insertf(&buff, "$%u\r\n", cache_data_length(&buff));
		if (likely(ret > 0)) {
			ret = cache_append(&buff, "\r\n", 2);
			if (likely(ret > 0)) {
				ret = cache_append(cache, cache_data_address(&buff),
						cache_data_length(&buff));
			}
		}
	}
	cache_clean(&buff);
	cache_finally(&buff);

	return ret;
}

ssize_t redis_string_info(struct cache *cache, const char *fmt, ...)
{
	assert(fmt && fmt[0]);
	struct cache    buff = {};
	ssize_t         ret = -1;
	va_list         ap;
	
	cache_init(&buff);
	ret = cache_append(&buff, "+", 1);
	if (likely(ret > 0)) {
		va_start(ap, fmt);
		ret = cache_vappendf(&buff, fmt, ap);
		va_end(ap);
		if (likely(ret > 0)) {
			ret = cache_append(&buff, "\r\n", -1);
			if (likely(ret > 0)) {
				ret = cache_append(cache, cache_data_address(&buff),
						cache_data_length(&buff));
			}
		}
	}
	cache_clean(&buff);
	cache_finally(&buff);
	
	return ret;
}

ssize_t redis_string_error(struct cache *cache, const char *fmt, ...)
{
	assert(fmt && fmt[0]);
	struct cache    buff = {};
	ssize_t         ret = -1;
	va_list         ap;
	
	cache_init(&buff);
	ret = cache_append(&buff, "-", 1);
	if (likely(ret > 0)) {
		va_start(ap, fmt);
		ret = cache_vappendf(&buff, fmt, ap);
		va_end(ap);
		if (likely(ret > 0)) {
			ret = cache_append(&buff, "\r\n", -1);
			if (likely(ret > 0)) {
				ret = cache_append(cache, cache_data_address(&buff),
						cache_data_length(&buff));
			}
		}
	}
	cache_clean(&buff);
	cache_finally(&buff);
	
	return ret;
}

ssize_t redis_string_number(struct cache *cache, long number)
{
	struct cache    buff = {};
	ssize_t         ret = -1;
	
	cache_init(&buff);
	ret = cache_appendf(&buff, ":%ld\r\n", number);
	if (likely(ret > 0)) {
		ret = cache_append(cache, cache_data_address(&buff), cache_data_length(&buff));
	}
	cache_clean(&buff);
	cache_finally(&buff);
	
	return ret;
}

/* ---------------					*/

/* macro to `unsign' a character */
#undef uchar
#define uchar(c) ((unsigned char)(c))

/*format split symbol*/
#undef L_ESC
#define L_ESC '%'

/* valid flags in a format specification */
#undef FLAGS
#define FLAGS "-+ #0"

struct scalprec
{
	int     scal;
	int     prec;
	int     stat;
#define sp_stat_none    0x00
#define sp_stat_scal    1 << 0
#define sp_stat_prec    1 << 1
};

struct value_type
{
	union
	{
		int             i;
		long            l;
		long long       ll;
		double          d;
		long double     ld;
		void            *p;
		ssize_t         ss;
		size_t          us;
	}       value;
	char    flag1;
	char    flag2;
	char    type;
	int     dt;
#define value_type_int          1
#define value_type_long         2
#define value_type_llong        3
#define value_type_double       4
#define value_type_ldouble      5
#define value_type_pointer      6
#define value_type_ssize        7
#define value_type_size         8
};

/*根据宽度和精度附加不同的参数*/
#define _item_append_value(T)														     \
	({															     \
		ssize_t _b = 0;													     \
		if ((scalprec->stat & sp_stat_scal) && (scalprec->stat & sp_stat_prec)) {					     \
			_b = cache_appendf(item, &format->buff[format->start], scalprec->scal, scalprec->prec, (T)); \
		} else if (scalprec->stat & sp_stat_scal) {									     \
			_b = cache_appendf(item, &format->buff[format->start], scalprec->scal, (T));		     \
		} else if (scalprec->stat & sp_stat_prec) {									     \
			_b = cache_appendf(item, &format->buff[format->start], scalprec->prec, (T));		     \
		} else {													     \
			_b = cache_appendf(item, &format->buff[format->start], (T));				     \
		}														     \
		_b;														     \
	})

static inline ssize_t _deal_item(struct cache *item, struct cache *format,
	struct value_type *vt, struct scalprec *scalprec)
{
	ssize_t b = -1;

	switch (vt->dt)
	{
		case value_type_int:

			b = _item_append_value(vt->value.i);

			break;

		case value_type_long:

			b = _item_append_value(vt->value.l);

			break;

		case value_type_llong:
			b = _item_append_value(vt->value.ll);

			break;

		case value_type_double:

			b = _item_append_value(vt->value.d);

			break;

		case value_type_ldouble:

			b = _item_append_value(vt->value.ld);

			break;

		case value_type_pointer:

			b = _item_append_value(vt->value.p);

			break;

		case value_type_ssize:

			b = _item_append_value(vt->value.ss);

			break;

		case value_type_size:

			b = _item_append_value(vt->value.us);

			break;

		default:
			errno = EINVAL;
			break;
	}
	cache_clean(format);
	return b;
}

static const char *_scanformat(const char *strfrmt, struct cache *format,
	struct scalprec *scalprec)
{
	const char *p = strfrmt;

	scalprec->stat = sp_stat_none;

	/* skip flags */
	while (strchr(FLAGS, *p)) {
		p++;
	}

	if (unlikely((size_t)(p - strfrmt) >= sizeof(FLAGS))) {
		/*occur error, 1 character at most for each of `-+ #0` */
		errno = EINVAL;
		return NULL;
	}

	/*skip usual scale*/
	if (*p == '*') {
		scalprec->stat |= sp_stat_scal;
		p++;
	} else {
		/*skip digit scale*/
		while (ISDIGIT(uchar(*p))) {
			p++;
		}
	}

	if (*p == '.') {
		p++;

		/*skip usual precision*/
		if (*p == '*') {
			p++;
			scalprec->stat |= sp_stat_prec;
		} else {
			/*skip digit precision*/
			while (ISDIGIT(uchar(*p))) {
				p++;
			}
		}
	}

	cache_clean(format);
	ssize_t tmp = 0;
	tmp = cache_appendf(format, "%%%.*s", (int)(p - strfrmt), strfrmt);
	return likely(tmp > 0) ? p : NULL;
}

ssize_t redis_vstringf(struct cache *cache, const char *fmt, va_list ap)
{
	ssize_t volatile        tolen = 0;
	unsigned volatile       start = 0;
	unsigned volatile       end = 0;

	ASSERTOBJ(cache);
	
	/*附加空对象*/
	if (fmt == NULL) {
		return cache_append(cache, "*-1\r\n", -1);
	}
	
	start = cache->start;
	end = cache->end;

	struct cache    format = {};
	struct cache    item = {};
	struct cache    cureq = {};

	TRY
	{
		bool            flag = false;
		const char      *c = fmt;
		int             items = 0;

		flag = cache_init(&format);
		AssertRaise(flag, EXCEPT_SYS);
		flag = cache_init(&item);
		AssertRaise(flag, EXCEPT_SYS);
		flag = cache_init(&cureq);
		AssertRaise(flag, EXCEPT_SYS);

		/*remove space which is at head of format*/
		while (likely(*c != '\0')) {
			if (likely(*c != L_ESC)) {
				if (likely(*c != ' ')) {
					/*skip space*/
					cache_append(&item, c++, 1);
				} else {
					/*append to proto-string*/
					unsigned size = item.end - item.start;

					if (likely(size > 0)) {
						ssize_t tmp = 0;
						tmp = cache_appendf(&cureq, "$%u\r\n%.*s\r\n", size,
								size, &item.buff[item.start]);

						if (unlikely(tmp < 0)) {
							goto error;
						}

						items++;
						cache_clean(&item);
					}

					c++;
				}
			} else if (*++c == L_ESC) {
				cache_append(&item, c++, 1);				/* %% */
			} else {
				struct scalprec sp = {};
				/*skip scale and precision*/
				c = _scanformat(c, &format, &sp);
				AssertRaise(c, EXCEPT_SYS);

				/*get value of scale or precision from vector*/
				if (sp.stat & sp_stat_scal) {
					sp.scal = va_arg(ap, int);
				}

				if (sp.stat & sp_stat_prec) {
					sp.prec = va_arg(ap, int);
				}

				int prefixs = 0;
				/*parse prefix*/
				switch (c[0])
				{
					case 'h':

						if (c[1] == 'h') {
							prefixs++;
						}

						prefixs++;
						break;

					case 'l':

						if (c[1] == 'l') {
							prefixs++;
						}

						prefixs++;
						break;

					case 'L':
						prefixs++;
						break;

					case 'z':
						prefixs++;
						break;

					default:
						break;
				}

				ssize_t                 tmp = 0;
				struct value_type       vt = {};

				/*store prefix and type*/
				if (prefixs > 0) {
					vt.flag1 = c[0];

					if (prefixs > 1) {
						vt.flag2 = c[1];
						vt.type = c[2];
					} else {
						vt.type = c[1];
					}
				} else {
					vt.type = c[0];
				}

				tmp = cache_appendf(&format, "%.*s%c", 1 + prefixs, c, '\0');
				RAISE_SYS_ERROR(tmp);
				c += (1 + prefixs);

				switch (vt.type)
				{
					case 'c':
						vt.dt = value_type_int;

						/*only allow 'c'*/
						if (unlikely((vt.flag1 != '\0') || (vt.flag2 != '\0'))) {
							goto error;
						} else {
							vt.value.i = va_arg(ap, int);
						}

						break;

					case 'd':
					case 'i':
					case 'o':
					case 'u':
					case 'x':
					case 'X':
						/*only allow 'll_' or 'hh_' or '_' */
						vt.dt = value_type_int;

						if ((vt.flag1 == 'h') || (vt.flag1 == '\0')) {
							vt.value.i = va_arg(ap, int);
						} else if (vt.flag1 == 'l') {
							if (vt.flag2 == 'l') {
								vt.value.ll = va_arg(ap, long long);
								vt.dt = value_type_llong;
							} else {
								vt.value.l = va_arg(ap, long);
								vt.dt = value_type_long;
							}
						} else if (vt.flag1 == 'z') {
							if ((vt.type == 'd') || (vt.type == 'i')) {
								vt.value.l = va_arg(ap, ssize_t);
								vt.dt = value_type_ssize;
							} else {
								vt.value.l = va_arg(ap, size_t);
								vt.dt = value_type_size;
							}
						} else {
							goto error;
						}

						break;

					case 'a':
					case 'A':
					case 'e':
					case 'E':
					case 'f':
					case 'g':
					case 'G':
						vt.dt = value_type_double;

						if ((vt.flag1 == 'l') || (vt.flag1 == '\0')) {
							if (unlikely(vt.flag2 != '\0')) {
								goto error;
							}

							vt.value.d = va_arg(ap, double);
						} else if (vt.flag1 == 'L') {
							vt.value.ld = va_arg(ap, long double);
							vt.dt = value_type_ldouble;
						} else {
							goto error;
						}

						break;

					case 's':
					case 'p':
						vt.dt = value_type_pointer;

						if (unlikely((vt.flag1 != '\0') || (vt.flag2 != '\0'))) {
							goto error;
						} else {
							vt.value.p = va_arg(ap, void *);
						}

						break;

					default:
error:
						x_perror("not supported this format string : `%.*s`",
							(int)(format.end - format.start),
							&format.buff[format.start]);
						errno = EINVAL;
						RAISE(EXCEPT_SYS);
						break;
				}

				tmp = _deal_item(&item, &format, &vt, &sp);
				RAISE_SYS_ERROR(tmp);
			}
		}

		/*last item*/
		unsigned size = item.end - item.start;

		if (likely(size > 0)) {
			ssize_t tmp = 0;
			tmp = cache_appendf(&cureq, "$%u\r\n%.*s\r\n", size,
					size, &item.buff[item.start]);

			if (unlikely(tmp < 0)) {
				goto error;
			}

			items++;
			cache_clean(&item);
		}

		/*insert head of redis-protocal*/
		if (likely(items >= 0)) {
			ssize_t tmp = 0;
			tmp = cache_insertf(&cureq, "*%d\r\n", items);
			RAISE_SYS_ERROR(tmp);

			/*append result to cache*/
			tolen = cache_append(cache, &cureq.buff[cureq.start], cureq.end - cureq.start);
			RAISE_SYS_ERROR(tolen);
			cache_clean(&cureq);
		}
	}
	CATCH
	{
		tolen = -1;
		cache->start = start;
		cache->end = end;
	}
	FINALLY
	{
		cache_finally(&format);
		cache_finally(&item);
		cache_finally(&cureq);
	}
	END;

	return tolen;
}

ssize_t redis_stringf(struct cache *cache, const char *fmt, ...)
{
	if (fmt == NULL) {
		return cache_append(cache, "*-1\r\n", -1);
	}
	
	va_list ap;
	va_start(ap, fmt);
	ssize_t ret = redis_vstringf(cache, fmt, ap);
	va_end(ap);

	return ret;
}
