func string_startswith(string, value) {
	let slen = sizeof(string);
	let vlen = sizeof(value);

	if (vlen > slen)
		false;

	for (let i = 0; i < vlen; i += 1) {
		if (string[i] != value[i])
			return false;
	}

	return true;
}

func string_endswith(string, value) {
	let slen = sizeof(string);
	let vlen = sizeof(value);

	if (vlen > slen)
		return false;

	let start = slen - vlen;
	for (let i = 0; i < vlen; i += 1) {
		if (string[start + i] != value[i])
			return false;
	}

	return true;
}

func string_split(string, token, count) {
	let slen = sizeof(string);
	let tlen = sizeof(token);

	let result = list_init();
	let tbuf = "";
	let sbuf = "";
	let rbuf = "";
	let c = 0;
	let a = false;

	for (let i = 0; i < slen; i += 1) {
		if (sizeof(tbuf) == tlen)
			tbuf = "";
		tbuf += string[i];
		sbuf += string[i];
		a = false;

		if (tbuf == token && (c < count || count <= 0)) {
			for (let s = 0; s < sizeof(sbuf) - tlen; s += 1)
				rbuf += sbuf[s];
			list_push(result, rbuf);
			tbuf = "";
			sbuf = "";
			rbuf = "";

			c += 1;
			a = true;
		}
	}

	if (sbuf || a)
		list_push(result, sbuf);

	return result;
}

func string_iswhitespace(char) {
	return char == " " || char == "\n" || char == "\t" || char == "\r";
}

func string_ltrim(string) {
	let slen = sizeof(string);

	let i = 0;
	let buf = "";

	while (i < slen && string_iswhitespace(string[i]))
		i += 1;

	while (i < slen) {
		buf += string[i];
		i += 1;
	}

	return buf;
}

func string_rtrim(string) {
	let slen = sizeof(string);

	if (slen == 0) return "";

	let i = slen - 1;
	let buf = "";

	while (i >= 0 && string_iswhitespace(string[i]))
		i -= 1;

	while (i >= 0) {
		buf = string[i] + buf;
		i -= 1;
	}

	return buf;
}

func string_trim(string) {
	return string_rtrim(string_ltrim(string));
}