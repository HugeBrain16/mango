#ifndef COMMAND_H
#define COMMAND_H

#define COMMAND_MAX_NAME 128
#define COMMAND_MAX_ARGC 32
#define COMMAND_MAX_ARGV 512

void command_handle(const char *command, int printcaret);

#endif
