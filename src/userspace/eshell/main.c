//#define _POSIX_C_SOURCE 200809L
extern char **environ;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <glob.h>
//#include <setjmp.h>

#define DEBUG_PARSING
//#define USE_READLINE

#ifdef USE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

//jmp_buf loop;

//size_t strlcat(char *dst, const char *src, size_t size);
//size_t strlcpy(char *dst, const char *src, size_t size);

char *get_cwd(void) {
	return getcwd(NULL, 0);
}

void str_replace(char *buf, const char *old, const char *new, int size) {
	char *start = strstr(buf, old);
	if (start == NULL)
		return;

	ssize_t buf_len = (ssize_t)strlen(buf);
	ssize_t old_len = (ssize_t)strlen(old);
	ssize_t new_len = (ssize_t)strlen(new);

	if (buf_len + (new_len - old_len) > size) {
		// This won't fit!
		return;
	}

	char *end = start + old_len;
	char *ending = malloc((buf + buf_len) - end + 1);
	strlcpy(ending, end, (buf + buf_len) - end + 1);

	// OK, we have the stuff after the variable stored away; now we're free
	// to destroy the buffer contents after /end/ with no ill consequences.

	// Do the replace
	int t = (size - (start - buf));
	strlcpy(start, new, t);
	// Copy back the ending
	strlcat(start + new_len, ending, (size - new_len));

	free(ending);
}

void replace_variables(char *buf, int size) {
	// Replace all variables in buf with their environment values
	// e.g. s/$HOME/$(getenv("HOME"))/g

	char *p = buf;
	while ((p = strchr(p, '$')) != NULL) {
		if (p > buf && *(p - 1) == '\\') {
			// This is escaped; remove the backslash and move on
			memmove(p - 1, p, (buf + size) - p);
			while (*p != ' ' && *p != 0) p++;
			continue;
		}
		char varname[256] = {0};
		int vi = 0;
		char *c = p;
		while (*c != 0 && *c != ' ' && *c != '\n' && *c != '\t') {
			varname[vi++] = *c++;
		}
		varname[vi] = 0;

		if (varname[1] == 0)
			continue;

		const char *val = getenv(varname + 1);
		if (val == NULL)
			val = "";

		str_replace(p, varname, val, size - (p - buf));
		p++;
	}

#if 0
	// Do the same, but for ~ and ~user
	p = buf;
	while ((p = strchr(p, '~')) != NULL) {
		if (p > buf && *(p - 1) == '\\') {
			// This is escaped, so ignore it
			memmove(p - 1, p, (buf + size) - p);
			p++;
			continue;
		}
		else if (p > buf && *(p - 1) != ' ') {
			// This usage is not relevant for paths, so ignore this one
			continue;
		}
		else if (*(p+1) == ' ' || *(p+1) == '\n' || *(p+1) == '\t' || *(p+1) == 0 || *(p+1) == '/') {
			// Replace with the user name
			char *home = getenv("HOME");
			if (!home) {
				fprintf(stderr, "eshell: HOME not set\n");
				continue;
			}
			str_replace(p, "~", home, size - (p - buf));
			p++;
		}
		else if (*(p+1) > ' ' && *(p+1) < 0x7f) {
			// Treat this like another user's name, e.g. ~root
			char *c = p;
			char username[64] = {0};
			int ui = 0;
			while (*c > ' ' && *c < 0x7f && *c != '/') { username[ui++] = *c++; }
			username[ui] = 0;

			struct passwd *pwd = getpwnam(username + 1 /* skip ~ */);
			if (!pwd || !(pwd->pw_dir)) {
				// Don't replace
				continue;
			}

			str_replace(p, username, pwd->pw_dir, size - (p - buf));
			p++;
		}
	}
#endif
}

char **parse_command_line(char *cmdline, int *argc, char ***env_extras) {
	// Count the worst-case number of arguments. There may be fewer due to quoting
	int max_args = 1; // the command name is always there
	int max_env = 0; // extra environment variables
	size_t len = strlen(cmdline);
	for (size_t i=0; i < len; i++) {
		if (cmdline[i] == ' ') {
			max_args++;
			while (cmdline[i] == ' ' && i < len) i++; // don't count "a      b   c" as more than three arguments
		}
		else if (cmdline[i] == '=') {
			max_env++;
		}
	}

	// Allocate memory for the arguments and environment variables
	char **argv = malloc((max_args + 1) * sizeof(char *));
	memset(argv, 0, (max_args + 1) * sizeof(char *));

	char **env = NULL;
	int env_i = 0;
	if (max_env > 0) {
		env = malloc((max_env + 1) * sizeof(char *));
		memset(env, 0, (max_env + 1) * sizeof(char *));
	}

	const char *c = cmdline;

	char *p = cmdline + strlen(cmdline) - 1;
	while (*p == '\n' || *p == ' ') *p-- = 0;

	// To hold the current argument while parsing
	// (we don't know what size to malloc() until we're done with an argument)
	char arg[256] = {0};
	size_t ai = 0; // Index into arg

	len = strlen(c);
	bool quote = false;
	while (*c == ' ' || *c == '\t') c++; // skip leading spaces

	if (*c == 0) {
		// Nothing to do, i.e. input was all whitespace or such
		return NULL;
	}

	bool handled = false;
	bool env_done = false; // set to true when we reach the first argument with no equals sign
	while (c < cmdline + len) {
		handled = false;
		while ((*c > ' ' && *c != '"' && *c != 0) || (*c == ' ' && quote)) {
			// Regular text, or stuff within a quote: just copy this
			arg[ai++] = *c++;
			handled = true;
		}

		if (*c == '"') {
			// We hit a quotemark. Change the quote status, skip the character (don't copy it), and continue
			quote = !quote;
			handled = true;
			c++;
			if (*c != 0)
				continue; // else save before we exit the loop, just below
		}
		if ((*c == ' ' && !quote) || *c == 0) {
			// Allocate memory, save this argument, increase argc
			handled = true;
			arg[ai++] = 0;

			if (!env_done) {
				// Check to see if this is a variable for the child
				p = strchr(arg, '=');
				if (p) {
					// Yup!
					assert(env_i < max_env);
					env[env_i] = malloc(strlen(arg + 1));
					strcpy(env[env_i], arg);
					env_i++;
					ai = 0;
					while (*c == ' ' || *c == '\t') c++;
					continue;
				}
				else
					env_done = true;
			}

			argv[*argc] = malloc(ai);
			assert(max_args > *argc);
			strlcpy(argv[*argc], arg, ai);
			(*argc)++;
			ai = 0;
		}
		else if (!handled) {
			c++; // Ignore character
		}

		// Skip whitespace until the next argument
		while (*c == ' ' || *c == '\t') c++;
	}

	if (env)
		env[env_i] = NULL;

	*env_extras = env;

	return argv;
}

//extern char **environ;

#if 0
void sigint_handler(int sig) {
	assert(sig == SIGINT);
	putc('\n', stdout);
	fflush(stdout);
	longjmp(loop, 1);
}
#endif

int main(int my_argc, char **my_argv) {
	setenv("PATH", "/:/initrd", 1);
	char buf[1024] = {0};
	//int last_exit = 0;

	char *last_wd = get_cwd();

	// Read the host name
	char hostname[] = "exscapeos";
	char *p = NULL;
#if 0
	char hostname[256] = {0};
	gethostname(hostname, 256);
	char *p = strchr(hostname, '.');
	if (p)
		*p = 0;
#endif

	// Read the user name (pwd->pw_name)
	//struct passwd *pwd = getpwuid(geteuid());

	char *cwd_str = NULL;

	while (true) {
next_input:
		//setjmp(loop, 1);
		//signal(SIGINT, sigint_handler);

		// Get the current working dir
		if (cwd_str)
			free(cwd_str);
		cwd_str = get_cwd();

		// Replace the home directory name with ~, if relevant
		setenv("HOME", "/", 1);
		char *home = getenv("HOME");
		if (home && strlen(home) <= strlen(cwd_str) && strncmp(cwd_str, home, strlen(home)) == 0 && strcmp(home, "/") != 0) {
			char tmp[256] = {0};
			strcpy(tmp, "~");
			if (home[strlen(home) - 1] == '/' && strlen(cwd_str) > 1)
				strlcat(tmp, "/", 256);
			strlcat(tmp, cwd_str + strlen(home), 256);
			free(cwd_str);
			cwd_str = malloc(strlen(tmp) + 1);
			strcpy(cwd_str, tmp);
		}

		char prompt[256] = {0};
		// Print the prompt (user@host cwd $)
		//snprintf(prompt, 256, "\e[01;32m%s@\e[01;31m%s\e[01;34m %s (eshell!) %c\e[00m ", pwd->pw_name, hostname, cwd_str, (geteuid() == 0 ? '#' : '$'));
		snprintf(prompt, 256, "%s %s # ", hostname, cwd_str);

		// Read a command
#ifdef USE_READLINE
		char *line = NULL;
		if ((line = readline(prompt)) == NULL) {
			printf("\n");
			return 0;
		}
		strlcpy(buf, line, 1024);
		if (buf[0])
			add_history(buf);
		free(line);
#else
		printf("%s", prompt);
		if (fgets(buf, 1024, stdin) == NULL) {
			printf("\n");
			return 0;
		}
#endif
		// Pre-parse the command line and substitute variable values
		replace_variables(buf, 1024);

		// Parse it
		char **argv = NULL;
		int argc = 0;
		char **extra_env = NULL;
		argv = parse_command_line(buf, &argc, &extra_env);

		if (argv == NULL || argv[0] == NULL) {
			// Nothing to do
			continue;
		}
		else if (strcmp(argv[0], "exit") == 0) {
			return 0;
		}

#ifdef DEBUG_PARSING
		for (int i=0; i < argc; i++) {
			printf("argv[%d] = %s\n", i, argv[i]);
		}
#endif

		char *redir_stdin = NULL, *redir_stdout = NULL, *redir_stderr = NULL;
		bool redir_append_stdout = false, redir_append_stderr = false; // use >>?

		// Handle redirects
		int i = 0;
#if 1
		while (i < argc) {
			assert(argv[i] != NULL);
			// Exact parsing is a bitch, but this is fairly easy.
			// Not 100% compatible with other shells, but this is for learning,
			// so who cares?
			// Stuff like 'echo a>b' (without the quotes) will fail and simply echo
			// the literal a>b.
			if (strcmp(argv[i], ">") == 0 || strcmp(argv[i], ">>") == 0 || strcmp(argv[i], "<") == 0 || \
				strcmp(argv[i], "2>") == 0 || strcmp(argv[i], "2>>") == 0) {
				if (i == 0) {
					fprintf(stderr, "Error: redirection only supported with commands, e.g. echo a > b\n");
					goto next_input;
				}
				if (i+1 >= argc) {
					// The redirection operator is the last argument
					fprintf(stderr, "Error: unexpected end of argument list\n");
					goto next_input;
				}

				if (argv[i][0] == '>') {
					// Redirect stdout to argv[i+1]
					redir_stdout = strdup(argv[i+1]);
					redir_append_stdout = (argv[i][1] == '>');
				}
				else if (argv[i][0] == '<') {
					// Redirect stdin from argv[i+1]
					redir_stdin = strdup(argv[i+1]);
				}
				else if (argv[i][0] == '2') {
					if (strcmp(argv[i+1], "&1") == 0) {
						redir_stderr = (char *)1; // ugh! Hack!
					}
					else
						redir_stderr = strdup(argv[i+1]);
					redir_append_stderr = (argv[i][2] == '>');
				}

				// Remove the two arguments (e.g. ">" and "output_file" for > output_file), and
				// move later arguments, if any, backwards two steps.
				// argv[argc] is guaranteed to be NULL, so it's safe to access.
				for (; i <= argc - 2; i++) {
					argv[i] = argv[i + 2];
				}
				argc -= 2;
				i = -1; // Restart parsing to simplify our lives
			}

			i++;
		}
#endif

#ifdef DEBUG_PARSING
		if (redir_stdout || redir_stdin || redir_stderr) {
			printf("----- after parsing redirects -----\n");
			for (i=0; i < argc; i++) {
				printf("argv[%d] = %s\n", i, argv[i]);
			}
		}
		if (redir_stdin) {
			printf("redirect stdin < %s\n", redir_stdin);
		}
		if (redir_stdout) {
			printf("redirect stdout >%s %s\n", (redir_append_stdout ? ">" : ""), redir_stdout);
		}
		if (redir_stderr) {
			printf("redirect stderr >%s %s\n", (redir_append_stderr ? ">" : ""), (redir_stderr == (char *)1 ? "<stdout>" : redir_stderr));
		}
#endif

		/* Take care of built-in commands */
		if (strcmp(argv[0], "cd") == 0) {
			char *dir;

			if (argc == 1) {
				dir = getenv("HOME");
				if (!dir) {
					fprintf(stderr, "eshell: cd: HOME not set\n");
				}
			}
			else if (strcmp(argv[1], "-") == 0) {
				dir = last_wd;
				printf("%s\n", dir);
			}
			else
				dir = argv[1];

			// Save the current working dir, in case chdir() fails
			char *tmp = last_wd;

			last_wd = get_cwd();
			if (dir && chdir(dir) != 0) {
				// It failed! Restore the last WD
				fprintf(stderr, "eshell: cd: ");
				free(last_wd);
				last_wd = tmp;
				perror(dir);
			}
			else if (dir) {
				// chdir succeeded; free the old last WD
				free(tmp);
			}

			goto next_input;
		}
		else if (strcmp(argv[0], "pwd") == 0) {
			char *dir = get_cwd();
			if (dir == NULL) {
				fprintf(stderr, "eshell: ");
				perror("get_cwd");
			}
			else {
				printf("%s\n", dir);
				free(dir);
			}

			goto next_input;
		}
		else if (strcmp(argv[0], "export") == 0) {
			// Handle exported variables; simply set them for the shell
			if (argc == 1) {
				// Print them all out
				i = 0;
				while (environ[i] != NULL) {
					printf("export %s\n", environ[i]);

					i++;
				}
			}
			else {
				for (i = 1; i < argc; i++) {
					p = strchr(argv[i], '=');
					if (!p)
						continue;
					else {
						*p = 0;
						char key[256] = {0};
						strlcpy(key, argv[i], 256);
						char value[512] = {0};
						strlcpy(value, p + 1, 512);
						setenv(key, value, 1);
						*p = '=';
					}
				}
			}

			goto next_input;
		}
		else if (strcmp(argv[0], "unset") == 0) {
			if (argc > 1) {
				for (i = 1; i < argc; i++) {
					p = strchr(argv[i], '=');
					if (p) {
						fprintf(stderr, "eshell: unset: %s is not a valid identifier\n", argv[i]);
						continue;
					}
					else {
						if (unsetenv(argv[1]) != 0)
							perror("eshell: unsetenv");
					}
				}
			}

			goto next_input;
		}

		int pid;
		if ((pid = fork()) > 0) {
			// Parent
			// Clean up the arguments; WE don't need them, only the child does. We,
			// on the other hand, will keep existing, so memory leaks matter!
			for (i = 0; i < argc; i++) {
				assert(argv[i] != NULL);
				free(argv[i]);
				argv[i] = NULL;
			}
			free(argv);
			argv = NULL;

			// Free the extra environment variables for the child
			if (extra_env) {
				i = 0;
				while (extra_env[i] != NULL) {
					free(extra_env[i]);
					extra_env[i] = NULL;
					i++;
				}
				free(extra_env);
				extra_env = NULL;
			}

			int stat = 0;
			while (waitpid(-1, &stat, 0) != -1) { /* Wait for all child tasks to finish */ }
			printf("child existed with exit status %d\n", WEXITSTATUS(stat));
			//last_exit = WEXITSTATUS(stat); // Extract the 8-bit return value
		}
		else if (pid == 0) {
			// Child
			// Re-enable the SIGINT signal
			//signal(SIGINT, SIG_DFL);

			// Take care of globbing, if necessary
#if 0
			for (i = 0; i < argc; i++) {
				if (argv[i] && (p = strchr(argv[i], '*')) != NULL) {
					if (p > argv[i] && *(p-1) == '\\') {
						// Escape this one
						memmove(p-1, p, (argv[i] + strlen(argv[i])) - p + 1);
						continue; // Might miss other asterisks in this SAME ARGUMENT, but
						          // I'm not going to bother fixing that.
					}

					// Still here? Let's glob it!
					glob_t gl;
					if (glob(argv[i], GLOB_MARK, NULL, &gl) != 0) {
						// Glob failed!
						// Ignore this argument and pass it as-is.
						continue;
					}

					// Glob succeeded.
					if (gl.gl_pathc == 0) {
						// No matches, pass this on as-is
						continue;
					}

					if (gl.gl_pathc == 1) {
						// Unlikely, but way easy
						argv[i] = strdup(gl.gl_pathv[0]);
						globfree(&gl);
						continue;
					}

					int newargs = argc + gl.gl_pathc; // - 1, but eh
					argv = realloc(argv, (newargs + 1) * sizeof(char *));
					for (int j = argc; j < newargs + 1; j++) {
						argv[j] = NULL;
					}

					char *start = (char *)&argv[i+1];
					char *end = (char *)&argv[argc+1]; /* we copy to [argc+1] exclusive */

					char *ending = malloc(end - start);
					memcpy(ending, start, end - start);

					// Copy the arguments in, one by one (so that we can strdup() them)
					for (int g = 0; g < gl.gl_pathc; g++) {
						argv[i + g] = strdup(gl.gl_pathv[g]);
					}

					// Copy the ending back after all that
					memcpy(&argv[i + gl.gl_pathc], ending, end - start);

					// Adjust argc and i for the new stuff
					argc += (gl.gl_pathc - 1); // - 1 because one was stored where the * was
					i    += (gl.gl_pathc - 1);
				}
			}
#endif

#if 1
			// Set up redirects
			if (redir_stdin) {
				if (freopen(redir_stdin, "r", stdin) == NULL) {
					fprintf(stderr, "eshell: ");
					perror(redir_stdin);
					exit(-1);
				}
			}
			if (redir_stdout) {
				if (freopen(redir_stdout, (redir_append_stderr ? "a" : "w"), stdout) == NULL) {
					fprintf(stderr, "eshell: ");
					perror(redir_stdout);
					exit(-1);
				}
			}
			if (redir_stderr) {
				if (redir_stderr == (char *)1) {
					// Redirect stderr to stdout
					if (dup2(fileno(stdout), fileno(stderr)) < 0) {
						fprintf(stderr, "eshell: ");
						perror("dup2");
					}
				}
				else {
					// Redirect stderr to a file
					if (freopen(redir_stderr, (redir_append_stderr ? "a" : "w"), stderr) == NULL) {
						fprintf(stderr, "eshell: ");
						perror(redir_stderr);
						exit(-1);
					}
				}
			}
#endif

			// Set up environment variables specified before the command, e.g. A=B ls
			if (extra_env) {
				i = 0;
				while (extra_env[i] != NULL) {
					p = strchr(extra_env[i], '=');
					if (!p) {
						i++;
						continue;
					}
					*p = 0;
					char key[256] = {0};
					char value[512] = {0};
					strlcpy(key, extra_env[i], 256);
					strlcpy(value, p + 1, 512);

					setenv(key, value, 1);
					i++;
				}

				// Free the extra environment variables now that setenv is done
				i = 0;
				while (extra_env[i] != NULL) {
					free(extra_env[i]);
					extra_env[i] = NULL;
					i++;
				}
				free(extra_env);
				extra_env = NULL;
			}

			// Finally, switch to the child program
			execvp(argv[0], argv);
			fprintf(stderr, "eshell: ");
			perror(argv[0]);
			exit(-1);
		}
		else {
			// Error!
			perror("fork");
			exit(-1);
		}
	}
	return 0;
}
