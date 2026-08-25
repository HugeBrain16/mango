include "string.sc";

let __test_cases;
let __test_passed;
let __test_failed;
let __test_colors;
let __test_report;
let __test_buffer;

func test_write(msg) {
	if (msg != null) {
		if (__test_buffer != null)
			__test_buffer += msg;
		print(msg);
	}
}

func test_writeln(msg) {
	test_write(msg);

	__test_buffer += "\n";
	println();
}

func test_init() {
	__test_buffer = "";

	test_writeln("=== Mango Test Suite ===");
	__test_cases = list_init();
	__test_passed = 0;
	__test_failed = 0;
	__test_colors = false;
}

func test_colors() {
	__test_colors = true;
}

func test_add(fn) {
	list_push(__test_cases, fn);
}

func test_add_by_prefix(prefix) {
	let vars = internal_getvars("function");
	for (let i = 0; i < sizeof(vars); i += 1) {
		let name = internal_getname(vars[i]);

		if (string_startswith(name, prefix))
			test_add(internal_getvalue(vars[i]));
	}
}

func test_report(file) {
	if (!file_isfile(file))
		exec("newfile " + file);

	__test_report = file_open(file, "w");
}

func test_run() {
	let cases = sizeof(__test_cases);

	if (__test_cases && cases > 0) {
		for (let i = 0; i < cases; i += 1) {
			let fn = __test_cases[i];
			let fn_name = as_str(fn);
			let no = i + 1;

			test_writeln("Running test \"" + fn_name + "\"...");

			printcap_init();
			printcap_print(false);
			let result = fn();
			let output = printcap_get();
			printcap_print(true);
			printcap_close();

			if (sizeof(output) > 0)
				test_writeln("Output:\n" + output);
			else
				__test_buffer += output;

			if (result) {
				__test_passed += 1;
				test_write("Test \"" + fn_name + "\" ");
				if (__test_colors)
					color_setfg("green");
				test_write("passed");
				if (__test_colors)
					color_reset();
				test_writeln(" (" + as_str(no) + "/" + as_str(cases) + ")");
			} else {
				__test_failed += 1;
				test_write("Test \"" + fn_name + "\" ");
				if (__test_colors)
					color_setfg("red");
				test_write("failed");
				if (__test_colors)
					color_reset();
				test_writeln(" (" + as_str(no) + "/" + as_str(cases) + ")");
			}
		}

		test_writeln("=== Summary ===");
		test_writeln("Total: " + as_str(cases));
		if (__test_colors && __test_passed > 0)
			color_setfg("green");
		test_writeln("Passed: " + as_str(__test_passed));
		if (__test_colors)
			color_reset();

		if (__test_colors && __test_failed > 0)
			color_setfg("red");
		test_writeln("Failed: " + as_str(__test_failed));
		if (__test_colors)
			color_reset();
	} else test_writeln("No test case.");

	if (__test_report) {
		file_write(__test_report, __test_buffer);
		file_close(__test_report);
	}
}