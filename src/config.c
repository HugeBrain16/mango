#include <stdint.h>

#include "config.h"
#include "string.h"
#include "heap.h"
#include "file.h"

static void config_parse(string_t *line, string_t *name, string_t *value) {
	int found_separator = 0;

	for (int i = 0; i < string_length(line); i++) {
		if (!found_separator) {
			if (line->value[i] != '=')
				string_putc(name, line->value[i]);
			else
				found_separator = 1;
		} else
			string_putc(value, line->value[i]);
	}

	if (name) string_trim(name);
	if (value) string_trim(value);
}

int config_has(const char *path, const char *name) {
	if (!file_path_isfile(path)) return 0;

	char *buffer = file_read(file_get_node(path));
	list_t *lines = readlines(buffer);
	for (size_t i = 0; i < lines->size; i++) {
		string_t *line = (string_t*) list_get(lines, i);

		string_t *line_name = string_init();
		config_parse(line, line_name, NULL);

		if (!strcmp(line_name->value, name)) {
			string_free(line_name);
			list_clear(lines);
			list_free(lines);
			heap_free(buffer);
			return 1;
		}
	}

	list_clear(lines);
	list_free(lines);
	heap_free(buffer);
	return 0;
}

char *config_get(const char *path, const char *name) {
	if (!file_path_isfile(path)) return NULL;

	char *buffer = file_read(file_get_node(path));
	list_t *lines = readlines(buffer);
	for (size_t i = 0; i < lines->size; i++) {
		string_t *line = (string_t*) list_get(lines, i);

		string_t *line_name = string_init();
		string_t *line_value = string_init();
		config_parse(line, line_name, line_value);

		if (!strcmp(line_name->value, name)) {
			char *value = heap_alloc(line_value->size);
			memcpy(value, line_value->value, line_value->size);
			
			string_free(line_name);
			heap_free(line_value);
			list_clear(lines);
			list_free(lines);
			heap_free(buffer);
			return value;
		}
	}

	list_clear(lines);
	list_free(lines);
	heap_free(buffer);
	return NULL;
}