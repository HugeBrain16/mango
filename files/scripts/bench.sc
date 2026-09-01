include "test.sc";
include "string.sc";

func bench_concat() {
	let x = "";

	for (let i = 0; i < 10000; i += 1)
		x += "a";

	return sizeof(x) == 10000;
}

func bench_string_trim() {
	let sample = "\n\t  meow \n\t";

	let result;
	let good = 0;
	for (let i = 0; i < 10000; i += 1) {
		result = string_trim(sample);

		if (result == "meow")
			good += 1;
	}

	return good == 10000;
}

func bench_index() {
	let sample = "hello world";
	
	let value = "";
	let count = 0;
	let good = 0;
	while (count < 10000) {
		for (let i = 0; i < sizeof(sample); i += 1)
			value += sample[i];

		if (value == sample)
			good += 1;
		value = "";
		count += 1;
	}

	return good == 10000;
}

func bench_convert() {
	let sample = "0123456789";

	let value;
	let good = 0;
	for (let i = 0; i < 10000; i += 1) {
		value = as_int(sample);

		if (value == 123456789)
			good += 1;
	}

	return good == 10000;
}

test_init();
test_colors();
test_timed();
test_report("bench_report.txt");
test_add_by_prefix("bench_");
test_run();