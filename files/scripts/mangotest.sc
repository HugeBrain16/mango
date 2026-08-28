include "test.sc";
include "string.sc";

func mangotest_index() {
	let a = "hello";
	let b = "hi";

	return a[0] == b[0];
}

func mangotest_assign() {
	let var;
	let result;

	result = var == null && type_name(var) == "null";

	var = 10;
	result = var == 10 && type_name(var) == "int";

	var = "abcd";
	result = var == "abcd" && type_name(var) == "str";

	var = 1.8;
	result = var == 1.8 && type_name(var) == "float";

	var = true;
	result = var == true && type_name(var) == "bool";

	return result;
}

func mangotest_for() {
	let check;

	for (let i = 0; i <= 10; i += 1)
		check = i;

	return check == 10;
}

func mangotest_while() {
	let check = 0;

	while (check < 10)
		check += 1;

	return check == 10;
}

func mangotest_assignadd() {
	let x = 0;

	return (x += 1) == 1;
}

func mangotest_assignsub() {
	let x = 1;

	return (x -= 1) == 0;
}

func mangotest_assignmul() {
	let x = 2;

	return (x *= 2) == 4;
}

func mangotest_assignmul_string() {
	let x = "abc";

	return (x *= 2) == "abcabc";
}

func mangotest_assigndiv() {
	let x = 4;

	let r = (x /= 2);
	return r == 2 && type_name(r) == "int";
}

func mangotest_internals() {
	let vars = internal_getvars(null);
	return sizeof(vars) > 0;
}

func mangotest_string_startswith() {
	return string_startswith("hello world", "hello");
}

func mangotest_string_endswith() {
	return string_endswith("hello world", "world");
}

func mangotest_string_split() {
	let split = string_split("hello world", " ", 0);
	return sizeof(split) == 2 && split[0] == "hello" && split[1] == "world";
}

func mangotest_string_split_empty() {
	let split = string_split("", " ", 0);
	return sizeof(split) == 0;
}

func mangotest_string_split_just_token() {
	let split = string_split(" ", " ", 0);
	return sizeof(split) == 2;
}

func mangotest_string_split_once() {
	let split = string_split("hello world meow", " ", 1);

	return sizeof(split) == 2 &&
		split[0] == "hello" &&
		split[1] == "world meow";
}

func mangotest_string_split_trailing() {
	let split = string_split("hello world cat ", " ", 0);

	return sizeof(split) == 4 &&
		split[0] == "hello" &&
		split[1] == "world" &&
		split[2] == "cat" &&
		split[3] == "";
}

func mangotest_string_trim() {
	let sample = string_trim("\n\t  meow\n\t  ");

	return sample == "meow";
}

test_init();
test_colors();
test_report("mangotest_report.txt");
test_add_by_prefix("mangotest_");
test_run();