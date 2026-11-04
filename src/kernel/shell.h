/* DEBIL OS - command shell and line editor */
#ifndef DEBIL_SHELL_H
#define DEBIL_SHELL_H

#include "types.h"

#define SHELL_MAX_LINE 160

void shell_run(void);
int  shell_read_line(const char *prompt, char *buf, int max);
void shell_execute(char *line);
void shell_print_prompt(void);

#endif
