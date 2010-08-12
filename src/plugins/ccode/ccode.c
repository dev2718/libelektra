/***************************************************************************
                     ccode.c  -  Skeleton of a plugin
                             -------------------
    begin                : Fri May 21 2010
    copyright            : (C) 2010 by Markus Raab
    email                : elektra@markus-raab.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the BSD License (revised).                      *
 *                                                                         *
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This is the skeleton of the methods you'll have to implement in order *
 *   to provide a valid plugin.                                            *
 *   Simple fill the empty functions with your code and you are            *
 *   ready to go.                                                          *
 *                                                                         *
 ***************************************************************************/


#include "ccode.h"

#include <stdlib.h>
#include <string.h>

/**
  * Gives the integer number 0-15 to a corresponding
  * hex character '0'-'9', 'a'-'f' or 'A'-'F'.
  */
static inline int elektraHexcodeConvFromHex(char c)
{
	if    (c == '0') return 0;
	else if (c=='1') return 1;
	else if (c=='2') return 2;
	else if (c=='3') return 3;
	else if (c=='4') return 4;
	else if (c=='5') return 5;
	else if (c=='6') return 6;
	else if (c=='7') return 7;
	else if (c=='8') return 8;
	else if (c=='9') return 9;
	else if (c=='a' || c=='A') return 10;
	else if (c=='b' || c=='B') return 11;
	else if (c=='c' || c=='C') return 12;
	else if (c=='d' || c=='D') return 13;
	else if (c=='e' || c=='E') return 14;
	else if (c=='f' || c=='F') return 15;
	else return 0; /* Unknown escape char */
}

int elektraCcodeOpen(Plugin *handle, Key *k)
{
	CCodeData *d = calloc (1, sizeof(CCodeData));

	/* Store for later use...*/
	elektraPluginSetData (handle, d);

	KeySet *config = elektraPluginGetConfig (handle);

	Key *escape = ksLookupByName (config, "/escape", 0);
	d->escape = '\\';
	if (escape && keyGetBaseNameSize(escape) && keyGetValueSize(escape))
	{
		int res;
		res = elektraHexcodeConvFromHex(keyBaseName(escape)[1]);
		res += elektraHexcodeConvFromHex(keyBaseName(escape)[0])*16;

		d->escape = res & 255;
	}

	Key *root = ksLookupByName (config, "/chars", 0);

	Key *cur = 0;
	if (!root)
	{
		/* Some default config */

		d->encode['\b'] = 'b';
		d->encode['\t'] = 't';
		d->encode['\n'] = 'n';
		d->encode['\v'] = 'v';
		d->encode['\f'] = 'f';
		d->encode['\r'] = 'r';
		d->encode['\\'] = '\\';
		d->encode['\''] = '\'';
		d->encode['\"'] = '"';
		d->encode['\0'] = '0';

		d->decode['b'] = '\b';
		d->decode['t'] = '\t';
		d->decode['n'] = '\n';
		d->decode['v'] = '\v';
		d->decode['f'] = '\f';
		d->decode['r'] = '\r';
		d->decode['\\'] = '\\';
		d->decode['\''] = '\'';
		d->decode['"'] = '\"';
		d->decode['0'] = '\0';
	} else {
		while ((cur = ksNext(config)) != 0)
		{
			/* ignore all keys not direct below */
			if (keyRel (root, cur) == 1)
			{
				/* ignore invalid size */
				if (keyGetBaseNameSize(cur) != 3) continue;
				if (keyGetValueSize(cur) != 3) continue;

				int res;
				res = elektraHexcodeConvFromHex(keyBaseName(cur)[1]);
				res += elektraHexcodeConvFromHex(keyBaseName(cur)[0])*16;

				int val;
				val = elektraHexcodeConvFromHex(keyString(cur)[1]);
				val += elektraHexcodeConvFromHex(keyString(cur)[0])*16;

				/* Hexencode this character! */
				d->encode [res & 255] = val;
				d->decode [val & 255] = res;
			}
		}
	}

	return 0;
}

int elektraCcodeClose(Plugin *handle, Key *k)
{
	CCodeData *d = elektraPluginGetData (handle);
	free (d);

	return 0;
}

/** Reads the value of the key and decodes all escaping
  * codes into the buffer.
  * @pre the buffer needs to be as large as value's size.
  * @param cur the key holding the value to decode
  * @param buf the buffer to write to
  */
void elektraCcodeDecode (Key *cur, char* buf, CCodeData *d)
{
	size_t valsize = keyGetValueSize(cur);
	const char *val = keyValue(cur);

	size_t out=0;
	for (size_t in=0; in<valsize-1; ++in)
	{
		unsigned char c = val[in];
		char *n = buf+out;

		if (c == '\\')
		{
			++in; /* Advance twice */
			c = val[in];

			*n = d->decode[c & 255];
		} else {
			*n = c;
		}
		++out; /* Only one char is written */
	}

	buf[out] = 0; // null termination for keyString()

	keySetRaw(cur, buf, out+1);
}


int elektraCcodeGet(Plugin *handle, KeySet *returned, Key *parentKey)
{
	/* get all keys */

	if (!strcmp (keyName(parentKey), "system/elektra/modules/ccode"))
	{
		KeySet *pluginConfig = ksNew (30,
			keyNew ("system/elektra/modules/ccode",
				KEY_VALUE, "ccode plugin waits for your orders", KEY_END),
			keyNew ("system/elektra/modules/ccode/exports", KEY_END),
			keyNew ("system/elektra/modules/ccode/exports/get",
				KEY_FUNC, elektraCcodeGet, KEY_END),
			keyNew ("system/elektra/modules/ccode/exports/set",
				KEY_FUNC, elektraCcodeSet, KEY_END),
			keyNew ("system/elektra/modules/ccode/infos",
				KEY_VALUE, "All information you want to know", KEY_END),
			keyNew ("system/elektra/modules/ccode/infos/author",
				KEY_VALUE, "Markus Raab <elektra@markus-raab.org>", KEY_END),
			keyNew ("system/elektra/modules/ccode/infos/licence",
				KEY_VALUE, "BSD", KEY_END),
			keyNew ("system/elektra/modules/ccode/infos/description",
				KEY_VALUE, "Decoding/Encoding engine which escapes unwanted characters.", KEY_END),
			keyNew ("system/elektra/modules/ccode/infos/provides",
				KEY_VALUE, "code", KEY_END),
			keyNew ("system/elektra/modules/ccode/infos/placements",
				KEY_VALUE, "postgetstorage presetstorage", KEY_END),
			keyNew ("system/elektra/modules/ccode/infos/needs",
				KEY_VALUE, "", KEY_END),
			keyNew ("system/elektra/modules/ccode/infos/version",
				KEY_VALUE, PLUGINVERSION, KEY_END),
			KS_END);
		ksAppend (returned, pluginConfig);
		ksDel (pluginConfig);
		return 1;
	}

	CCodeData *d = elektraPluginGetData (handle);

	Key *cur;
	char *buf = malloc (1000);
	size_t bufalloc = 1000;

	ksRewind(returned);
	while ((cur = ksNext(returned)) != 0)
	{
		size_t valsize = keyGetValueSize(cur);
		if (valsize > bufalloc)
		{
			bufalloc = valsize;
			buf = realloc (buf, bufalloc);
		}

		elektraCcodeDecode (cur, buf, d);
	}

	free (buf);

	return 1; /* success */
}


/** Reads the value of the key and encodes it in
  * c-style in the buffer.
  *
  * @param cur the key which value is to encode
  * @param buf the buffer
  * @pre the buffer needs to have twice as much space as the value's size
  */
void elektraCcodeEncode (Key *cur, char* buf, CCodeData *d)
{
	size_t valsize = keyGetValueSize(cur);
	const char *val = keyValue(cur);

	size_t out=0;
	for (size_t in=0; in<valsize-1; ++in)
	{
		unsigned char c = val[in];
		char *n = buf+out+1;

		if (d->encode[c])
		{
			*n = d->encode[c];
			//Escape char
			buf[out] = d->escape;
			out += 2;
		}
		else
		{
			// just copy one character
			buf[out] = val[in];
			// advance out cursor
			out ++;
			// go to next char
		}
	}

	buf[out] = 0; // null termination for keyString()

	keySetRaw(cur, buf, out+1);
}


int elektraCcodeSet(Plugin *handle, KeySet *returned, Key *parentKey)
{
	/* set all keys */
	CCodeData *d = elektraPluginGetData (handle);

	Key *cur;
	char *buf = malloc (1000);
	size_t bufalloc = 1000;

	ksRewind(returned);
	while ((cur = ksNext(returned)) != 0)
	{
		size_t valsize = keyGetValueSize(cur);
		if (valsize*2 > bufalloc)
		{
			bufalloc = valsize*2;
			buf = realloc (buf, bufalloc);
		}

		elektraCcodeEncode (cur, buf, d);
	}

	free (buf);

	return 1; /* success */
}

Plugin *ELEKTRA_PLUGIN_EXPORT(ccode)
{
	return elektraPluginExport("ccode",
		ELEKTRA_PLUGIN_OPEN,	&elektraCcodeOpen,
		ELEKTRA_PLUGIN_CLOSE,	&elektraCcodeClose,
		ELEKTRA_PLUGIN_GET,	&elektraCcodeGet,
		ELEKTRA_PLUGIN_SET,	&elektraCcodeSet,
		ELEKTRA_PLUGIN_END);
}

