#ifndef CONFIG_H
#define CONFIG_H

extern int config_has(const char *path, const char *name);
extern char *config_get(const char *path, const char *name);

#endif