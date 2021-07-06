/*
 * The  information  in  this  document  is  subject  to  change
 * without  notice  and  should not be construed as a commitment
 * by Digital Equipment Corporation or by DECUS.
 *
 * Neither Digital Equipment Corporation, DECUS, nor the authors
 * assume any responsibility for the use or reliability of  this
 * document or the described software.
 *
 *      Copyright (C) 1980, DECUS
 *
 * General permission to copy or modify, but not for profit,  is
 * hereby  granted,  provided that the above copyright notice is
 * included and reference made to  the  fact  that  reproduction
 * privileges were granted by DECUS.
 */

#include "grep.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "cmds.h"
#include "config.h"
#include "entry.h"
#include "errors.h"
#include "util.h"

static const char *documentation[] = {
    "For a given pattern, grep searches the file to match the former. It is "
    "executed by",
    "   grep [flags]/swithes regular_expression file_list\n",
    "Flags/switches uses '-' followed by single characters:",
    "   -n      Prints line number before every line",
    "   -c      Shows the count of matched lines",
    "   -v      Print non-matched lines\n",
    "   -f      Print the file name for matching lines switch, see below",
    "The file_list is basically a list of files (wildcards are acceptable on "
    "RSX modes).",
    "\nIf a file is given, the file name is normally printed.",
    "The -f flag basically reverses this action (print name no file, not if "
    "more).\n",
    0};

static const char *patdoc[] = {
    "The regular_expression defines the pattern to search for.  Upper- and",
    "lower-case are always ignored.  Blank lines never match.  The expression",
    "should be quoted to prevent file-name translation.",
    "x      An ordinary character (not mentioned below) matches that "
    "character.",
    "'\\'    The backslash quotes any character.  \"\\$\" matches a "
    "dollar-sign.",
    "'^'    A circumflex at the beginning of an expression matches the",
    "       beginning of a line.",
    "'$'    A dollar-sign at the end of an expression matches the end of a "
    "line.",
    "'.'    A period matches any character except \"new-line\".",
    "':a'   A colon matches a class of characters described by the following",
    "':d'     character.  \":a\" matches any alphabetic, \":d\" matches "
    "digits,",
    "':n'     \":n\" matches alphanumerics, \": \" matches spaces, tabs, and",
    "': '     other control characters, such as new-line.",
    "'*'    An expression followed by an asterisk matches zero or more",
    "       occurrances of that expression: \"fo*\" matches \"f\", \"fo\"",
    "       \"foo\", etc.",
    "'+'    An expression followed by a plus sign matches one or more",
    "       occurrances of that expression: \"fo+\" matches \"fo\", etc.",
    "'-'    An expression followed by a minus sign optionally matches",
    "       the expression.",
    "'[]'   A string enclosed in square brackets matches any character in",
    "       that string, but no others.  If the first character in the",
    "       string is a circumflex, the expression matches any character",
    "       except \"new-line\" and the characters in the string.  For",
    "       example, \"[xyz]\" matches \"xx\" and \"zyx\", while \"[^xyz]\"",
    "       matches \"abc\" but not \"axb\".  A range of characters may be",
    "       specified by two characters separated by \"-\".  Note that,",
    "       [a-z] matches alphabetics, while [z-a] never matches.",
    "The concatenation of regular expressions is a regular expression.", 0};

#define LMAX 512
#define PMAX 256

#define CHAR_ 1
#define BOL_ 2
#define EOL_ 3
#define ANY_ 4
#define CLASS_ 5
#define NCLASS_ 6
#define STAR_ 7
#define PLUS_ 8
#define MINUS_ 9
#define ALPHA_ 10
#define DIGIT_ 11
#define NALPHA_ 12
#define PUNCT_ 13
#define RANGE_ 14
#define ENDPAT_ 15

int cflag = 0, fflag = 0, nflag = 0, vflag = 0, nfile = 0, debug = 0;

char *pp, lbuf[LMAX], pbuf[PMAX];

// Display name of a file
static int displayFile(char *s)
{
	printf("File %s:\n", s);
	return CMD_SUCCESS;
}

// Report an unopenable file
static int cantOpen(char *s)
{
	fprintf(stderr, "%s: cannot open\n", s);
	return CMD_SUCCESS;
}

// Gives good help
static int help(const char **hp)
{
	register const char **dp;

	for (dp = hp; *dp; ++dp)
		printf("%s\n", *dp);

	return CMD_SUCCESS;
}

// Display usage summary
static int usageSummary(const char *s)
{
	fprintf(stderr, "?GREP-E-%s\n", s);
	fprintf(stderr,
	    "Usage: grep [-cfnv] pattern [file ...].  grep ? for help\n");
	exit(1);
}

// Display error
static int displayError(const char *s)
{
	fprintf(stderr, "%s", s);
	return CMD_FAILURE;
}

// Store an entry in the pattern buffer
static int storeEntry(int op)
{
	if (pp >= &pbuf[PMAX])
		displayError("Pattern is complex\n");
	*pp++ = op;

	return CMD_SUCCESS;
}

// Report a bad pattern specification
static int badPattern(const char *message, char *source, char *stop)
/* Error message */
/* Pattern start & end */
{
	fprintf(stderr, "-GREP-E-%s, pattern is\"%s\"\n", message, source);
	fprintf(stderr, "-GREP-E-Stopped at byte %ld, '%c'\n", stop - source,
	    stop[-1]);
	displayError("?GREP-E-Bad pattern\n");

	return CMD_SUCCESS;
}

// Match partial line with the pattern
static char *partialMatch(char *line, char *pattern)
// (partial) line to match
// (partial) pattern to match
{
	register char *l;  // Current line pointer
	register char *p;  // Current pattern pointer
	register char c;   // Current character
	char *e;           // End for STAR and PLUS match
	int op;            // Pattern operation
	int n;             // Class counter
	char *are;         // Start of STAR match

	l = line;
	if (debug > 1)
		printf("patternMatch(\"%s\")\n", line);
	p = pattern;
	while ((op = *p++) != ENDPAT_) {
		if (debug > 1)
			printf("byte[%ld] = 0%o, '%c', op = 0%o\n", (l - line),
			    *l, *l, op);
		switch (op) {

		case CHAR_:
			if (tolower(*l++) != *p++)
				return (0);
			break;

		case BOL_:
			if (l != lbuf)
				return (0);
			break;

		case EOL_:
			if (*l != '\0')
				return (0);
			break;

		case ANY_:
			if (*l++ == '\0')
				return (0);
			break;

		case DIGIT_:
			if ((c = *l++) < '0' || (c > '9'))
				return (0);
			break;

		case ALPHA_:
			c = tolower(*l++);
			if (c < 'a' || c > 'z')
				return (0);
			break;

		case NALPHA_:
			c = tolower(*l++);
			if (c >= 'a' && c <= 'z')
				break;
			else if (c < '0' || c > '9')
				return (0);
			break;

		case PUNCT_:
			c = *l++;
			if (c == 0 || c > ' ')
				return (0);
			break;

		case CLASS_:
		case NCLASS_:
			c = tolower(*l++);
			n = *p++ & 0377;
			do {
				if (*p == RANGE_) {
					p += 3;
					n -= 2;
					if (c >= p[-2] && c <= p[-1])
						break;
				} else if (c == *p++)
					break;
			} while (--n > 1);
			if ((op == CLASS_) == (n <= 1))
				return (0);
			if (op == CLASS_)
				p += n - 2;
			break;

		case MINUS_:
			e = partialMatch(l, p);  // Find a match
			while (*p++ != ENDPAT_)
				;       // Skip the pattern
			if (e)          // if match found
				l = e;  // update the string
			break;

		case PLUS_:  // One or more ...
			if ((l = partialMatch(l, p)) == 0)
				return (0);  // Gotta have a match
			break;
		case STAR_:       // Zero or more ...
			are = l;  // Remember the line start
			while (*l && (e = partialMatch(l, p)))
				l = e;  // get the longest match
			while (*p++ != ENDPAT_)
				;           // Skip the pattern
			while (l >= are) {  // Try to match rest
				if ((e = partialMatch(l, p)))
					return (e);
				--l;  // No try the earlier one
			}
			return (0);  // Nothing else worked
			break;

		default:
			printf("Wrong op code %d\n", op);
			displayError("Can't happen -- match\n");
		}
	}
	return (l);
}

// Compile a class (within [])
static char *compileClass(char *source, char *src)
// Start the pattern -- for error msg.
// Start of class
{
	register char *s;   // Source pointer
	register char *cp;  // Pattern start
	register int c;     // Current character
	int o;              // Temp

	s = src;
	o = CLASS_;
	if (*s == '^') {
		++s;
		o = NCLASS_;
	}
	storeEntry(o);
	cp = pp;
	storeEntry(0);  // for byte count
	while ((c = *s++) && c != ']') {
		if (c == '\\') {  // Store quoted char
			if ((c = *s++) == '\0')
				badPattern(
				    "Class is terminated badly", source, s);
			else
				storeEntry(tolower(c));
		} else if (c == '-' && (pp - cp) > 1 && *s != ']'
		    && *s != '\0') {
			c = pp[-1];              // Start of range
			pp[-1] = RANGE_;         // Signal's range
			storeEntry(c);           // Restore start
			c = *s++;                // Get end char
			storeEntry(tolower(c));  // and store end char
		} else {
			storeEntry(tolower(c));  // Store normal char
		}
	}
	if (c != ']')
		badPattern("Class is unterminated", source, s);
	if ((c = (pp - cp)) >= 256)
		badPattern("Class very large", source, s);
	if (c == 0)
		badPattern("Class empty", source, s);
	*cp = c;
	return (s);
}

// Match the line -lbuf with the pattern -pbuf and if matched, return 1
static int matchBuf()
{
	register char *l;  // Line pointer

	for (l = lbuf; *l; ++l) {
		if (partialMatch(l, pbuf))
			return 1;

		// printf("%s",l);
	}

	return 0;
}

// Scan the file to match the pattern in pbuf[]
static int grep(FILE *fp, char *fn)
// File we want to process and file name (for -f option)
{
	register int lno, count, m;
	lno = 0;
	count = 0;
	while (fgets(lbuf, LMAX, fp)) {
		++lno;
		m = matchBuf();
		if ((m && !vflag) || (!m && vflag)) {
			++count;
			if (!cflag) {
				if (fflag && fn) {
					displayFile(fn);
					fn = 0;
				}
				if (nflag) {
					printf("%d\t", lno);
				}
				printf("%s\n", lbuf);
			}
		}
	}
	if (cflag) {
		if (fflag && fn)
			displayFile(fn);
		printf("%d\n", count);
	}

	return CMD_SUCCESS;
}

// Compile the matched pattern into global pbuf[]
static int compilePattern(char *source)
// The pattern we want to compile
{
	register char *s;   // Source string- pointer
	register char *lp;  // Last pattern- pointer
	register int c;     // Current character
	int o;              // Temp
	char *spp;          // Pointer to save beginning of pattern

	s = source;
	// debug=1;
	if (debug)
		printf("Pattern = %s \n", s);

	pp = pbuf;

	while ((c = *s++)) {

		// STAR, PLUS and MINUS are special.
		if (c == '*' || c == '+' || c == '-') {
			if (pp == pbuf || (o = pp[-1]) == BOL_ || o == EOL_
			    || o == STAR_ || o == PLUS_ || o == MINUS_)
				badPattern("Illegal occurrance op.", source, s);
			storeEntry(ENDPAT_);
			storeEntry(ENDPAT_);
			spp = pp;              // Save end of pattern
			while (--pp > lp)      // Move the pattern down
				*pp = pp[-1];  // one byte
			*pp = (c == '*') ? STAR_ : (c == '-') ? MINUS_ : PLUS_;
			pp = spp;  // Restore end of pattern
			continue;
		}

		// All the rest.

		lp = pp;  // Remember the start
		switch (c) {

		case '^':
			storeEntry(BOL_);
			break;

		case '$':
			storeEntry(EOL_);
			break;

		case '.':
			storeEntry(ANY_);
			break;

		case '[':
			s = compileClass(source, s);
			break;

		case ':':
			if (*s) {
				switch (tolower(c = *s++)) {

				case 'a':
				case 'A':
					storeEntry(ALPHA_);
					break;

				case 'd':
				case 'D':
					storeEntry(DIGIT_);
					break;

				case 'n':
				case 'N':
					storeEntry(NALPHA_);
					break;

				case ' ':
					storeEntry(PUNCT_);
					break;

				default:
					badPattern("Unknown : type", source, s);
				}
				break;
			} else
				badPattern("No : type", source, s);

			break;

		case '\\':
			if (*s)
				c = *s++;

			break;

		default:
			storeEntry(CHAR_);
			storeEntry(tolower(c));
		}
	}
	storeEntry(ENDPAT_);
	storeEntry(0);  // Terminate the string
	if (debug) {
		for (lp = pbuf; lp < pp;) {
			if ((c = (*lp++ & 0377)) < ' ')
				printf("\\%o ", c);
			else
				printf("%c ", c);
		}
		printf("\n");
	}

	return CMD_SUCCESS;
}

static const char *cmdname = "grep";

// Dispays help for the grep cmd in various levels
void help_cmd_grep(unsigned int level)
{
	printf("This is the %s help for '%s'.\n", level ? EXT_HELP : SHORT_HELP,
	    cmdname);
	return;
}

// Main entry point for grep cmd, accepts an array of arguments
int cmd_grep(char **argv)
{
	int argc;

	// Count the arguments
	for (argc = 0; argv[argc] != NULL; argc++)
		;

	register char *p;
	register int c, i;
	int gotpattern;

	FILE *f;

	if (argc <= 1)
		usageSummary("No arguments");
	if (argc == 2 && argv[1][0] == '?' && argv[1][1] == 0) {
		help(documentation);
		help(patdoc);
		return CMD_SUCCESS;
	}
	nfile = argc - 1;
	gotpattern = 0;
	for (i = 1; i < argc; ++i) {
		p = argv[i];
		if (*p == '-') {
			++p;
			while ((c = *p++)) {
				switch (tolower(c)) {

				case '?':
					help(documentation);
					break;

				case 'C':
				case 'c':
					++cflag;
					break;

				case 'D':
				case 'd':
					++debug;
					break;

				case 'F':
				case 'f':
					++fflag;
					break;

				case 'n':
				case 'N':
					++nflag;
					break;

				case 'v':
				case 'V':
					++vflag;
					break;

				default:
					usageSummary("Unknown flag");
				}
			}
			argv[i] = 0;
			--nfile;
		} else if (!gotpattern) {
			compilePattern(p);
			argv[i] = 0;
			++gotpattern;
			--nfile;
		}
	}

	if (!gotpattern)
		usageSummary("No pattern");
	if (nfile == 0)
		grep(stdin, 0);
	else {

		fflag = (fflag) ^ (nfile > 0);
		for (i = 1; i < argc; ++i) {
			if ((p = argv[i])) {
				if ((f = fopen(p, "r")) == NULL)
					cantOpen(p);
				else {
					grep(f, p);
					fclose(f);
				}
			}
		}
	}

	cflag = 0;
	fflag = 0;
	nflag = 0;
	vflag = 0;
	nfile = 0;
	debug = 0;
	return CMD_SUCCESS;
}
