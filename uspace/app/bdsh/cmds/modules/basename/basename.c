/* Automatically generated by mknewcmd on Wednesday 28 October 2020 05:35:04 PM
 * IST This is machine generated output. The author of mknewcmd claims no
 * copyright over the contents of this file. Where legally permitted, the
 * contents herein are donated to the public domain.
 *
 * You should apply any license and copyright that you wish to this file,
 * replacing this header in its entirety. */

#include "basename.h"
//#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <str.h>
#include <mem.h>
//#include <string.h>
#include "cmds.h"
#include "config.h"
#include "entry.h"
#include "errors.h"
#include "util.h"

static const char *cmdname = "basename";

/* Dispays help for basename in various levels */
void help_cmd_basename(unsigned int level)
{
	printf("This is the %s help for '%s'.\n", level ? EXT_HELP : SHORT_HELP,
	    cmdname);
	return;
}

static char *gnu_basename(char *path)
{
	char *base = str_rchr(path, '/');
	return base ? base + 1 : path;
}

/* Main entry point for basename, accepts an array of arguments */
int cmd_basename(char **argv)
{
	unsigned int argc;
	// unsigned int i;

	/* Count the arguments */
	for (argc = 0; argv[argc] != NULL; argc++)
		;

	// printf("%s %s\n", TEST_ANNOUNCE, cmdname);
	// printf("%d arguments passed to %s", argc - 1, cmdname);

	// if (argc < 2) {
	// 	printf("\n");
	// 	return CMD_SUCCESS;
	// }

	// printf(":\n");
	// for (i = 1; i < argc; i++)
	// 	printf("[%d] -> %s\n", i, argv[i]);

	if (argc < 2) {
		fprintf(stderr, "%s: expected argument\n", argv[0]);
		return CMD_FAILURE;
	}

	char *c = gnu_basename(argv[1]);

	if (argc > 2) {
		char *suffix = argv[2];
		char *found = str_str(c + str_length(c) - str_length(suffix), suffix);
		if (found
		    && (found - c == (int) (str_length(c) - str_length(suffix)))) {
			*found = '\0';
		}
	}

	fprintf(stdout, "%s\n", c);

	return CMD_SUCCESS;
}
