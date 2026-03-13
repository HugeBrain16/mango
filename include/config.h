#ifndef CONFIG_H
#define CONFIG_H

int config_has(const char *path, const char *name);
char *config_get(const char *path, const char *name);

#endif