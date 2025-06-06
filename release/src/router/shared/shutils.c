/*
 * Shell-like utility functions
 *
 * Copyright (C) 2012, Broadcom Corporation. All Rights Reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: shutils.c 625086 2016-03-15 12:51:47Z $
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <typedefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <sys/sysinfo.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <syslog.h>
#include <typedefs.h>
#include <wlioctl.h>

#include <bcmnvram.h>
#include <shutils.h>

/* Linux specific headers */
#ifdef linux
#if (defined(__GLIBC__) || defined(__UCLIBC__))
#include <error.h>
#endif
#include <termios.h>
#include <sys/time.h>
//#include <net/ethernet.h>
#else
#include <proto/ethernet.h>
#endif /* linux */

#include "shared.h"

#define T(x)		__TXT(x)
#define __TXT(s)	L ## s

#ifndef B_L
#define B_L		T(__FILE__),__LINE__
#define B_ARGS_DEC	char *file, int line
#define B_ARGS		file, line
#endif /* B_L */

#define bfree(B_ARGS, p) free(p)
#define balloc(B_ARGS, num) malloc(num)
#define brealloc(B_ARGS, p, num) realloc(p, num)

#define STR_REALLOC		0x1				/* Reallocate the buffer as required */
#define STR_INC			64				/* Growth increment */

unsigned char	used_shift='C';

typedef struct {
	char		*s;						/* Pointer to buffer */
	int		size;						/* Current buffer size */
	int		max;						/* Maximum buffer size */
	int		count;						/* Buffer count */
	int		flags;						/* Allocation flags */
} strbuf_t;

/*
 *	Sprintf formatting flags
 */
enum flag {
	flag_none = 0,
	flag_minus = 1,
	flag_plus = 2,
	flag_space = 4,
	flag_hash = 8,
	flag_zero = 16,
	flag_short = 32,
	flag_long = 64
};

/*
 * Print out message on console.
 */
void dbgprintf (const char * format, ...)
{
	FILE *f;
	int nfd;
	va_list args;

	if((nfd = open("/dev/console", O_WRONLY | O_NONBLOCK)) > 0){
		if((f = fdopen(nfd, "w")) != NULL){
			va_start(args, format);
			vfprintf(f, format, args);
			va_end(args);
			fclose(f);
		}
		else
		{
		close(nfd);
	}
}
}

void dbg(const char * format, ...)
{
	FILE *f;
	int nfd;
	va_list args;

	if (((nfd = open("/dev/console", O_WRONLY | O_NONBLOCK)) > 0) &&
	    (f = fdopen(nfd, "w")))
	{
		va_start(args, format);
		vfprintf(f, format, args);
		va_end(args);
		fclose(f);
		nfd = -1;
	}
	else
	{
		va_start(args, format);
		vfprintf(stderr, format, args);
		va_end(args);
	}

	if (nfd != -1) close(nfd);
}

/* XXX - this should be in a common file */
#define MAX_RADIOS 4
#define MAX_BSS_PER_RADIO 32
#define WLMBSS_DEV_NAME        "wlmbss"
#define WL_DEV_NAME "wl"
#define WDS_DEV_NAME   "wds"

/*
 * Reads file and returns contents
 * @param	fd	file descriptor
 * @return	contents of file or NULL if an error occurred
 */
char *
fd2str(int fd)
{
	char *buf = NULL;
	size_t count = 0, n;

	do {
		buf = realloc(buf, count + 512);
		n = read(fd, buf + count, 512);
		if (n < 0) {
			free(buf);
			buf = NULL;
		}
		count += n;
	} while (n == 512);

	close(fd);
	if (buf)
		buf[count] = '\0';
	return buf;
}

/*
 * Reads file and returns contents
 * @param	path	path to file
 * @return	contents of file or NULL if an error occurred
 */
char *
file2str(const char *path)
{
	int fd;

	if ((fd = open(path, O_RDONLY)) == -1) {
		perror(path);
		return NULL;
	}

	return fd2str(fd);
}

/*
 * Waits for a file descriptor to change status or unblocked signal
 * @param	fd	file descriptor
 * @param	timeout	seconds to wait before timing out or 0 for no timeout
 * @return	1 if descriptor changed status or 0 if timed out or -1 on error
 */
int
waitfor(int fd, int timeout)
{
	fd_set rfds;
	struct timeval tv = { timeout, 0 };

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	return select(fd + 1, &rfds, NULL, NULL, (timeout > 0) ? &tv : NULL);
}

int _eval_retry(char *const argv[], const char *path, int timeout, int *ppid, char *appname)
{
	int i = 0;

	for(i=0; i<5; ++i) {
		if(pids(appname))
			break;
		else {
			_dprintf("%s[%s]: counts %d!\n", __func__, appname, i+1);
			sleep(1);
			_eval(argv, path, timeout, ppid);
		}
	}

	return 0;
}

/*
 * Concatenates NULL-terminated list of arguments into a single
 * commmand and executes it
 * @param	argv	argument list
 * @param	path	NULL, ">output", or ">>output"
 * @param	timeout	seconds to wait before timing out or 0 for no timeout
 * @param	ppid	NULL to wait for child termination or pointer to pid
 * @return	return value of executed command or errno
 *
 * Ref: http://www.open-std.org/jtc1/sc22/WG15/docs/rr/9945-2/9945-2-28.html
 */
int _eval(char *const argv[], const char *path, int timeout, int *ppid)
{
	sigset_t set, sigmask;
	sighandler_t chld = SIG_IGN;
	pid_t pid, w;
	int status = 0;
	int fd, flags, sig, n;
	char s[256], *p;
	int debug_logeval = nvram_get_int("debug_logeval");
#if 0
	char *cpu = "0";
	char *cpu_argv[32] = { "taskset", "-c", cpu, NULL};
#endif

	if (!ppid) {
		// block SIGCHLD
		sigemptyset(&set);
		sigaddset(&set, SIGCHLD);
		sigprocmask(SIG_BLOCK, &set, &sigmask);
		// without this we cannot rely on waitpid() to tell what happened to our children
		chld = signal(SIGCHLD, SIG_DFL);
	}

#if defined(RTCONFIG_VALGRIND)
	setenv("USER", nvram_get("http_username")? : "admin", 1);
#endif
#ifdef HND_ROUTER
	p = nvram_safe_get("env_path");
	snprintf(s, sizeof(s), "%s%s/sbin:/bin:/usr/sbin:/usr/bin:/opt/sbin:/opt/bin", *p ? p : "", *p ? ":" : "");
	p = getenv("PATH");
	if (p == NULL || strcmp(p, s) != 0)
		setenv("PATH", s, 1);
#endif
	pid = fork();
	if (pid == -1) {
		perror("fork");
		status = errno;
		goto EXIT;
	}
	if (pid != 0) {
		// parent
		if (ppid) {
			*ppid = pid;
			return 0;
		}
		do {
			if ((w = waitpid(pid, &status, 0)) == -1) {
				status = errno;
				perror("waitpid");
				goto EXIT;
			}
		} while (!WIFEXITED(status) && !WIFSIGNALED(status));

		if (WIFEXITED(status)) status = WEXITSTATUS(status);
EXIT:
		if (!ppid) {
			// restore signals
			sigprocmask(SIG_SETMASK, &sigmask, NULL);
			signal(SIGCHLD, chld);
			// reap zombies
			chld_reap(0);
		}
		return status;
	}

	// child

	setsid();

	// reset signal handlers
	for (sig = 1; sig < _NSIG; sig++)
		signal(sig, SIG_DFL);

	// unblock signals if called from signal handler
	sigemptyset(&set);
	sigprocmask(SIG_SETMASK, &set, NULL);

	if ((fd = open("/dev/null", O_RDWR)) >= 0) {
		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		if (fd > STDERR_FILENO)
			close(fd);
	}

	// Redirect stdout & stderr to <path>
	if (path) {
		flags = O_WRONLY | O_CREAT | O_NONBLOCK;
		if (*path == '>') {
			++path;
			if (*path == '>') {
				++path;
				// >>path, append
				flags |= O_APPEND;
			}
			else {
				// >path, overwrite
				flags |= O_TRUNC;
			}
		}

		if ((fd = open(path, flags, 0644)) < 0) {
			perror(path);
		} else {
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			if (fd > STDERR_FILENO)
				close(fd);
		}
	} else if (debug_logeval) {
		pid = getpid();

		if ((fd = open("/dev/console", O_RDWR | O_NONBLOCK)) < 0) {
			snprintf(s, sizeof(s), "/tmp/eval.%d", pid);
			fd = open(s, O_CREAT | O_RDWR | O_NONBLOCK, 0600);
		} else
			dup2(fd, STDIN_FILENO);

		if (fd >= 0) {
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			if (fd > STDERR_FILENO)
				close(fd);

			printf("_eval +%ld pid=%d ", uptime(), getpid());
			for (n = 0; argv[n]; n++) printf("%s ", argv[n]);
			printf("\n");
		}
	}

	// execute command
#ifndef HND_ROUTER
	p = nvram_safe_get("env_path");
	snprintf(s, sizeof(s), "%s%s/sbin:/bin:/usr/sbin:/usr/bin:/opt/sbin:/opt/bin", *p ? p : "", *p ? ":" : "");
	p = getenv("PATH");
	if (p == NULL || strcmp(p, s) != 0)
		setenv("PATH", s, 1);
#endif

	alarm(timeout);

#if 1
	execvp(argv[0], argv);
#else
	for (n = 0; argv[n]; n++) {
		cpu_argv[n+3] = argv[n];
	execvp(cpu_argv[0], cpu_argv);
#endif

	perror(argv[0]);

	_exit(errno);
}

static int get_cmds_size(char *const *cmds)
{
        int i=0;
        for(; cmds[i]; ++i);
        return i;
}

int _cpu_eval(int *ppid, char *cmds[])
{
        int ncmds=0, n=0, i;
	int ret;
        int maxn = get_cmds_size(cmds)
#if defined (SMP) || defined(RTCONFIG_ALPINE) || defined(RTCONFIG_LANTIQ)
                + 4;
#else
                +1;
#endif
        char **cpucmd = (char **)malloc(maxn * sizeof(char *));
	if (!cpucmd)
		return -ENOMEM;

        for(i=0; i<maxn; ++i)
                cpucmd[i]=NULL;

#if defined (SMP) || defined(RTCONFIG_ALPINE) || defined(RTCONFIG_LANTIQ)
        cpucmd[ncmds++]="taskset";
        cpucmd[ncmds++]="-c";
	if(!strcmp(cmds[n], CPU0) || !strcmp(cmds[n], CPU1) || !strcmp(cmds[n], CPU2) || !strcmp(cmds[n], CPU3))
                cpucmd[ncmds++]=cmds[n++];
        else
#if defined(RTCONFIG_ALPINE) || defined(RTCONFIG_LANTIQ)
                cpucmd[ncmds++]=cmds[n++];;
#else
                cpucmd[ncmds++]=CPU0;
#endif
#else
	if(strcmp(cmds[n], CPU0) && strcmp(cmds[n], CPU1) && strcmp(cmds[n], CPU2) && strcmp(cmds[n], CPU3))
                cpucmd[ncmds++]=cmds[n++];
        else
                n++;
#endif
        for(; cmds[n]; cpucmd[ncmds++]=cmds[n++]);

	ret = _eval(cpucmd, NULL, 0, ppid);;
	free(cpucmd);
	return ret;
}

int _cpu_mask_eval(char *const argv[], const char *path, int timeout, int *ppid, unsigned int mask)
{
	int maxn = get_cmds_size(argv) + 3;
	char **cpuargv;
	int ret;
	int argc = 0;
	int i;
	char mask_str[16] = {0};

	cpuargv = (char **)malloc(maxn * sizeof(char *));
	if (!cpuargv)
		return -ENOMEM;

	for (i = 0;i < maxn; i++)
		cpuargv[i] = NULL;

	cpuargv[argc++] = "taskset";
	snprintf(mask_str, sizeof(mask_str), "0x%x", mask);
	cpuargv[argc++] = mask_str;
	for(i = 0; argv[i]; i++)
		cpuargv[argc++] = argv[i];

	//_dprintf("\n=====\n"); for(i = 0; cpuargv[i]; i++) _dprintf("%s ", cpuargv[i]); _dprintf("\n=====\n");
	ret = _eval(cpuargv, path, timeout, ppid);
	free(cpuargv);
	return ret;
}

/*
 * Concatenates NULL-terminated list of arguments into a single
 * commmand and executes it
 * @param	argv	argument list
 * @return	stdout of executed command or NULL if an error occurred
 */
char *
_backtick(char *const argv[])
{
	int filedes[2];
	pid_t pid;
	int status;
	char *buf = NULL;

	/* create pipe */
	if (pipe(filedes) == -1) {
		perror(argv[0]);
		return NULL;
	}

	switch (pid = fork()) {
	case -1:	/* error */
		return NULL;
	case 0:		/* child */
		close(filedes[0]);	/* close read end of pipe */
		dup2(filedes[1], 1);	/* redirect stdout to write end of pipe */
		close(filedes[1]);	/* close write end of pipe */
		execvp(argv[0], argv);
		exit(errno);
		break;
	default:	/* parent */
		close(filedes[1]);	/* close write end of pipe */
		buf = fd2str(filedes[0]);
		waitpid(pid, &status, 0);
		break;
	}

	return buf;
}


/*
 * fread() with automatic retry on syscall interrupt
 * @param	ptr	location to store to
 * @param	size	size of each element of data
 * @param	nmemb	number of elements
 * @param	stream	file stream
 * @return	number of items successfully read
 */
int
safe_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t ret = 0;

	do {
		clearerr(stream);
		ret += fread((char *)ptr + (ret * size), size, nmemb - ret, stream);
	} while (ret < nmemb && ferror(stream) && errno == EINTR);

	return ret;
}

/*
 * fwrite() with automatic retry on syscall interrupt
 * @param	ptr	location to read from
 * @param	size	size of each element of data
 * @param	nmemb	number of elements
 * @param	stream	file stream
 * @return	number of items successfully written
 */
int
safe_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t ret = 0;

	do {
		clearerr(stream);
		ret += fwrite((char *)ptr + (ret * size), size, nmemb - ret, stream);
	} while (ret < nmemb && ferror(stream) && errno == EINTR);

	return ret;
}

/*
 * Returns the process ID.
 *
 * @param	name	pathname used to start the process.  Do not include the
 *                      arguments.
 * @return	pid
 */
pid_t
get_pid_by_name(char *name)
{
	pid_t           pid = -1;
	DIR             *dir;
	struct dirent   *next;

	if ((dir = opendir("/proc")) == NULL) {
		perror("Cannot open /proc");
		return -1;
	}

	while ((next = readdir(dir)) != NULL) {
		FILE *fp;
		char filename[256];
		char buffer[256];

		/* If it isn't a number, we don't want it */
		if (!isdigit(*next->d_name))
			continue;

		snprintf(filename, sizeof(filename), "/proc/%s/cmdline", next->d_name);
		fp = fopen(filename, "r");
		if (!fp) {
			continue;
		}
		buffer[0] = '\0';
		fgets(buffer, 256, fp);
		fclose(fp);

		if (!strcmp(name, buffer)) {
			pid = strtol(next->d_name, NULL, 0);
			break;
		}
	}

	closedir(dir);

	return pid;
}

pid_t
get_pid_by_thrd_name(char *name)
{
        pid_t           pid = -1;
        DIR             *dir;
        struct dirent   *next;
        int cmp = 0;
        if ((dir = opendir("/proc")) == NULL) {
                perror("Cannot open /proc");
                return -1;
        }

        while ((next = readdir(dir)) != NULL) {
                FILE *fp;
                char filename[256];
                char buffer[256];

                /* If it isn't a number, we don't want it */
                if (!isdigit(*next->d_name))
                        continue;
                snprintf(filename, sizeof(filename), "/proc/%s/comm", next->d_name);
                fp = fopen(filename, "r");
                if (!fp) {
                        continue;
                }
                buffer[0] = '\0';
                fgets(buffer, 256, fp);
                fclose(fp);

                if (!(cmp = strncmp(name, buffer, strlen(name)))) {
                        pid = strtol(next->d_name, NULL, 0);
                        break;
                }
        }

        closedir(dir);
        return pid;
}

void replace_null_to_space(char *str, int len) {

	int i = 0;
	char *p = str;

	for(i=0; i<len-1; i++){
		if(*p == '\0')
			*p = ' ';
		p++;

	}
}

pid_t
get_pid_by_process_name(char *name)
{
	size_t size = 0;
	char p_name[128] = {0}, filename[256] = {0};
	pid_t           pid = -1;
	DIR             *dir;
	struct dirent   *next;

	if ((dir = opendir("/proc")) == NULL) {
		perror("Cannot open /proc");
		return -1;
	}

	while ((next = readdir(dir)) != NULL) {
		/* If it isn't a number, we don't want it */
		if (!isdigit(*next->d_name))
			continue;

		memset(filename, 0, sizeof(filename));
		snprintf(filename, sizeof(filename), "/proc/%s/cmdline", next->d_name);
		FILE* f = fopen(filename,"r");
		if(f){
			size = fread(p_name, sizeof(char), sizeof(p_name), f);

			if(size>0){
				replace_null_to_space(p_name, size);
				if('\n'==p_name[size-1])
				p_name[size-1]='\0';
			}else
				memset(p_name, 0, sizeof(p_name));

			fclose(f);
		}

		if (!strcmp(name, p_name)) {
			pid = strtol(next->d_name, NULL, 0);
			break;
		}
	}
	closedir(dir);

	return pid;
}

/*
 * Convert Ethernet address string representation to binary data
 * @param	a	string in xx:xx:xx:xx:xx:xx notation
 * @param	e	binary data
 * @return	TRUE if conversion was successful and FALSE otherwise
 */
int
ether_atoe(const char *a, unsigned char *e)
{
	char *c = (char *) a;
	int i = 0;

	memset(e, 0, ETHER_ADDR_LEN);
	for (;;) {
		e[i++] = (unsigned char) strtoul(c, &c, 16);
		if (!*c++ || i == ETHER_ADDR_LEN)
			break;
	}
	return (i == ETHER_ADDR_LEN);
}

/*
 * Convert Ethernet address binary data to string representation
 * @param	e	binary data
 * @param	a	string in xx:xx:xx:xx:xx:xx notation
 * @return	a
 */
char *
ether_etoa(const unsigned char *e, char *a)
{
	char *c = a;
	int i;

	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		if (i)
			*c++ = ':';
		c += sprintf(c, "%02X", e[i] & 0xff);
	}
	return a;
}

char *ether_etoa2(const unsigned char *e, char *a)
{
	sprintf(a, "%02X%02X%02X%02X%02X%02X", e[0], e[1], e[2], e[3], e[4], e[5]);
	return a;
}

/*
 * Increase Ethernet address e with n
 */
int ether_inc(unsigned char *e, const unsigned char n)
{
	int c = 0;
	int ret = 0;

	c = (e[5] >= (0xff - n + 1)) ? 1 : 0;
	e[5] += n;

	if (c) {
		c = (e[4] >= 0xff) ? 1 : 0;
		e[4] += 1;

		if (c) {
			ret = (e[3] >= 0xff) ? -1 : 0;
			e[3] += 1;
		}
	}

	return (ret);
}

#ifdef GTAC5300
static int dbg_noisy = -1;
#endif

void cprintf(const char *format, ...)
{
	FILE *f;
	int nfd;
	va_list args;

#if defined(DEBUG_NOISY) && !defined(HND_ROUTER)
	{
#else
#ifdef GTAC5300
	if(dbg_noisy == -1)
		dbg_noisy = nvram_get_int("debug_cprintf");

	if(dbg_noisy != 1)
		return;
	{
#else
	if (nvram_match("debug_cprintf", "1")) {
#endif
#endif
		if((nfd = open("/dev/console", O_WRONLY | O_NONBLOCK)) >= 0){
			if((f = fdopen(nfd, "w")) != NULL){
				va_start(args, format);
				vfprintf(f, format, args);
				va_end(args);
				fclose(f);
			} else
				close(nfd);
		}
	}
#if 1
	if (nvram_match("debug_cprintf_file", "1")) {
//		char s[32];
//		sprintf(s, "/tmp/cprintf.%d", getpid());
//		if ((f = fopen(s, "a")) != NULL) {
		if ((f = fopen("/tmp/cprintf", "a")) != NULL) {
			va_start(args, format);
			vfprintf(f, format, args);
			va_end(args);
			fclose(f);
		}
	}
#endif
#if 0
	if (nvram_match("debug_cprintf_log", "1")) {
		char s[512];

		va_start(args, format);
		vsnprintf(s, sizeof(s), format, args);
		s[sizeof(s) - 1] = 0;
		va_end(args);

		if ((s[0] != '\n') || (s[1] != 0)) syslog(LOG_DEBUG, "%s", s);
	}
#endif
}

#ifndef WL_BSS_INFO_VERSION
#error WL_BSS_INFO_VERSION
#endif

#if WL_BSS_INFO_VERSION >= 108
// xref (all): nas, wlconf
#if 0
/*
 * Get the ip configuration index if it exists given the
 * eth name.
 *
 * @param	wl_ifname 	pointer to eth interface name
 * @return	index or -1 if not found
 */
int
get_ipconfig_index(char *eth_ifname)
{
	char varname[64];
	char varval[64];
	char *ptr;
	char wl_ifname[NVRAM_MAX_PARAM_LEN];
	int index;

	/* Bail if we get a NULL or empty string */

	if (!eth_ifname) return -1;
	if (!*eth_ifname) return -1;

	/* Look up wl name from the eth name */
	if (osifname_to_nvifname(eth_ifname, wl_ifname, sizeof(wl_ifname)) != 0)
		return -1;

	snprintf(varname, sizeof(varname), "%s_ipconfig_index", wl_ifname);

	ptr = nvram_get(varname);

	if (ptr) {
	/* Check ipconfig_index pointer to see if it is still pointing
	 * the correct lan config block
	 */
		if (*ptr) {
			int index;
			char *ifname;
			char buf[64];
			index = atoi(ptr);

			snprintf(buf, sizeof(buf), "lan%d_ifname", index);

			ifname = nvram_get(buf);

			if (ifname) {
				if  (!(strcmp(ifname, wl_ifname)))
					return index;
			}
			nvram_unset(varname);
		}
	}

	/* The index pointer may not have been configured if the
	 * user enters the variables manually. Do a brute force search
	 *  of the lanXX_ifname variables
	 */
	for (index = 0; index < MAX_NVPARSE; index++) {
		snprintf(varname, sizeof(varname), "lan%d_ifname", index);
		if (nvram_match(varname, wl_ifname)) {
			/* if a match is found set up a corresponding index pointer for wlXX */
			snprintf(varname, sizeof(varname), "%s_ipconfig_index", wl_ifname);
			snprintf(varval, sizeof(varval), "%d", index);
			nvram_set(varname, varval);
			nvram_commit();
			return index;
		};
	}
	return -1;
}

/*
 * Set the ip configuration index given the eth name
 * Updates both wlXX_ipconfig_index and lanYY_ifname.
 *
 * @param	eth_ifname 	pointer to eth interface name
 * @return	0 if successful -1 if not.
 */
int
set_ipconfig_index(char *eth_ifname, int index)
{
	char varname[255];
	char varval[16];
	char wl_ifname[NVRAM_MAX_PARAM_LEN];

	/* Bail if we get a NULL or empty string */

	if (!eth_ifname) return -1;
	if (!*eth_ifname) return -1;

	if (index >= MAX_NVPARSE) return -1;

	/* Look up wl name from the eth name only if the name contains
	   eth
	*/

	if (osifname_to_nvifname(eth_ifname, wl_ifname, sizeof(wl_ifname)) != 0)
		return -1;

	snprintf(varname, sizeof(varname), "%s_ipconfig_index", wl_ifname);
	snprintf(varval, sizeof(varval), "%d", index);
	nvram_set(varname, varval);

	snprintf(varname, sizeof(varname), "lan%d_ifname", index);
	nvram_set(varname, wl_ifname);

	nvram_commit();

	return 0;
}

/*
 * Get interfaces belonging to a specific bridge.
 *
 * @param	bridge_name 	pointer to bridge interface name
 * @return	list of interfaces belonging to the bridge or NULL
 *              if not found/empty
 */
char *
get_bridged_interfaces(char *bridge_name)
{
	static char interfaces[255];
	char *ifnames = NULL;
	char bridge[64];

	if (!bridge_name) return NULL;

	memset(interfaces, 0, sizeof(interfaces));
	snprintf(bridge, sizeof(bridge), "%s_ifnames", bridge_name);

	ifnames = nvram_get(bridge);

	if (ifnames)
		strncpy(interfaces, ifnames, sizeof(interfaces));
	else
		return NULL;

	return  interfaces;

}

#endif	// 0

/*
 * Search a string backwards for a set of characters
 * This is the reverse version of strspn()
 *
 * @param	s	string to search backwards
 * @param	accept	set of chars for which to search
 * @return	number of characters in the trailing segment of s
 *		which consist only of characters from accept.
 */
static size_t
sh_strrspn(const char *s, const char *accept)
{
	const char *p;
	size_t accept_len = strlen(accept);
	int i;


	if (s[0] == '\0')
		return 0;

	p = s + strlen(s);
	i = 0;

	do {
		p--;
		if (memchr(accept, *p, accept_len) == NULL)
			break;
		i++;
	} while (p != s);

	return i;
}

/*
 * Parse the unit and subunit from an interface string such as wlXX or wlXX.YY
 *
 * @param	ifname	interface string to parse
 * @param	unit	pointer to return the unit number, may pass NULL
 * @param	subunit	pointer to return the subunit number, may pass NULL
 * @return	Returns 0 if the string ends with digits or digits.digits, -1 otherwise.
 *		If ifname ends in digits.digits, then unit and subuint are set
 *		to the first and second values respectively. If ifname ends
 *		in just digits, unit is set to the value, and subunit is set
 *		to -1. On error both unit and subunit are -1. NULL may be passed
 *		for unit and/or subuint to ignore the value.
 */
int
get_ifname_unit(const char* ifname, int *unit, int *subunit)
{
	const char digits[] = "0123456789";
	char str[64];
	char *p;
	size_t ifname_len = strlen(ifname);
	size_t len;
	unsigned long val;

	if (unit)
		*unit = -1;
	if (subunit)
		*subunit = -1;

	if (ifname_len + 1 > sizeof(str))
		return -1;

#if defined(RTCONFIG_QCA) && defined(RTCONFIG_WIGIG)
	/* QCA's 802.11ad Wigig interface name is wlan0 and unit number is WL_60G_BAND,
	 * that is, 3.  It's not compatible with below rule and we can't extract unit
	 * number from interface name.
	 */
	if (strstr(ifname, get_wififname(WL_60G_BAND)) != NULL) {
		int i;
		char tmp_ifname[IFNAMSIZ];

		if (unit)
			*unit = WL_60G_BAND;

		if (subunit) {
			*subunit = 0;

			if (strchr(ifname, '.') != NULL) {
				for (i = 1; subunit && i < MAX_NO_MSSID; ++i) {
					if (strcmp(ifname, get_wlxy_ifname(WL_60G_BAND, i, tmp_ifname)))
						continue;
					*subunit = i;
					break;
				}
			}
		}
		return 0;
	}
#endif

	strlcpy(str, ifname, sizeof(str));

	/* find the trailing digit chars */
	len = sh_strrspn(str, digits);

	/* fail if there were no trailing digits */
	if (len == 0)
		return -1;

	/* Check for WDS interface */
	if (strncmp(str, WDS_DEV_NAME, strlen(WDS_DEV_NAME))) {
		/* point to the beginning of the last integer and convert */
		p = str + (ifname_len - len);
		val = strtoul(p, NULL, 10);

		/* if we are at the beginning of the string, or the previous
		 * character is not a '.', then we have the unit number and
		 * we are done parsing
		 */
		if (p == str || p[-1] != '.') {
			if (unit)
			*unit = val;
			return 0;
		} else {
			if (subunit)
				*subunit = val;
		}

		/* chop off the '.NNN' and get the unit number */
		p--;
		p[0] = '\0';

		/* find the trailing digit chars */
		len = sh_strrspn(str, digits);

		/* fail if there were no trailing digits */
		if (len == 0)
			return -1;

		/* point to the beginning of the last integer and convert */
		p = p - len;
		val = strtoul(p, NULL, 10);

		/* save the unit number */
		if (unit)
			*unit = val;
	} else {
		/* WDS interface */
		/* point to the beginning of the first integer and convert */
		p = str + strlen(WDS_DEV_NAME);
		val = strtoul(p, &p, 10);

		/* if next character after integer is '.' then we have unit number else fail */
		if (p[0] == '.') {
			/* Save unit number */
			if (unit) {
				*unit = val;
			}
		} else {
			return -1;
		}

		/* chop off the '.' and get the subunit number */
		p++;
		val = strtoul(p, &p, 10);

		/* if next character after interger is '.' then we have subunit number else fail */
		if (p[0] == '.') {
			/* Save subunit number */
			if (subunit) {
				*subunit = val;
			}
		} else {
			return -1;
		}
	}

	return 0;
}

/* This utility routine builds the wl prefixes from wl_unit.
 * Input is expected to be a null terminated string
 *
 * @param       prefix          Pointer to prefix buffer
 * @param       prefix_size     Size of buffer
 * @param       Mode            If set generates unit.subunit output
 *                              if not set generates unit only
 * @param       ifname          Optional interface name string
 *
 *
 * @return                              pointer to prefix, NULL if error.
 */
char *
make_wl_prefix(char *prefix, int prefix_size, int mode, char *ifname)
{
        int unit = -1, subunit = -1;
        char *wl_unit = NULL;

        if (!prefix || !prefix_size)
                return NULL;

        if (ifname) {
                if (!*ifname)
                        return NULL;
                wl_unit = ifname;
        } else {
                wl_unit = nvram_get("wl_unit");

                if (!wl_unit)
                        return NULL;
        }

        if (get_ifname_unit(wl_unit, &unit, &subunit) < 0)
                return NULL;

        if (unit < 0) return NULL;

        if  ((mode) && (subunit > 0))
                snprintf(prefix, prefix_size, "wl%d.%d_", unit, subunit);
        else
                snprintf(prefix, prefix_size, "wl%d_", unit);

        return prefix;
}

/* In the space-separated/null-terminated list(haystack), try to
 * locate the string "needle"
 */
char *
_find_in_list(const char *haystack, const char *needle, char deli)
{
	const char *ptr = haystack;
	int needle_len = 0;
	int haystack_len = 0;
	int len = 0;
	char strde[2];

	if (!haystack || !needle || !*haystack || !*needle)
		return NULL;

	snprintf(strde, sizeof(strde), "%c", deli);
	needle_len = strlen(needle);
	haystack_len = strlen(haystack);

	while (*ptr != 0 && ptr < &haystack[haystack_len])
	{
		/* consume leading spaces */
		ptr += strspn(ptr, strde);

		/* what's the length of the next word */
		len = strcspn(ptr, strde);

		if ((needle_len == len) && (!strncmp(needle, ptr, len)))
			return (char*) ptr;

		ptr += len;
	}
	return NULL;
}


char *
find_in_list(const char *haystack, const char *needle)
{
	return _find_in_list(haystack, needle, ' ');
}

/**
 *	remove_from_list
 *	Remove the specified word from the list.

 *	@param name word to be removed from the list
 *	@param list Space separated list to modify
 *	@param listsize Max size the list can occupy

 *	@return	error code
 */
int
_remove_from_list(const char *name, char *list, int listsize, char deli)
{
	int namelen = 0;
	char *occurrence = list;

	if (!list || !name || (listsize <= 0))
		return EINVAL;

	namelen = strlen(name);

	occurrence = _find_in_list(occurrence, name, deli);

	if (!occurrence)
		return EINVAL;

	/* last item in list? */
	if (occurrence[namelen] == 0)
	{
		/* only item in list? */
		if (occurrence != list)
			occurrence--;
		occurrence[0] = 0;
	}
	else if (occurrence[namelen] == deli)
	{
		/* Using memmove because of possible overlapping source and destination buffers */
		memmove(occurrence, &occurrence[namelen+1 /* space */],
			strlen(&occurrence[namelen+1 /* space */]) +1 /* terminate */);
	}

	return 0;
}

int
remove_from_list(const char *name, char *list, int listsize)
{
	return _remove_from_list(name, list, listsize, ' ');
}

/**
 *		add_to_list
 *	Add the specified interface(string) to the list as long as
 *	it will fit in the space left in the list.

 *	NOTE: If item is already in list, it won't be added again.

 *	@param name Name of interface to be added to the list
 *	@param list List to modify
 *	@param listsize Max size the list can occupy

 *	@return	error code
 */
int
add_to_list(const char *name, char *list, int listsize)
{
	int listlen = 0;
	int namelen = 0;
	int newlen = 0;

	if (!list || !name || (listsize <= 0))
		return EINVAL;

	listlen = strlen(list);
	namelen = strlen(name);

	/* is the item already in the list? */
	if (find_in_list(list, name))
		return 0;

	newlen = listlen + namelen + 1 /* NULL */;
	/* only add a space if the list isn't empty */
	if (list[0] != 0)
		newlen += 1; /* space */

	if (listsize < newlen)
		return EMSGSIZE;

	/* add a space if the list isn't empty and it doesn't already have space */
	if (list[0] != 0 && list[listlen-1] != ' ')
	{
		list[listlen++] = 0x20;
	}

	strncpy(&list[listlen], name, namelen + 1 /* terminate */);

	return 0;
}

int
_add_to_list(const char *name, char *list, int listsize, char deli)
{
        int listlen = 0;
        int namelen = 0;
        int newlen = 0;

        if (!list || !name || (listsize <= 0))
                return EINVAL;

        listlen = strlen(list);
        namelen = strlen(name);

        /* is the item already in the list? */
        if (_find_in_list(list, name, deli))
                return 0;

        newlen = listlen + namelen + 1 /* NULL */;
        /* only add a space if the list isn't empty */
        if (list[0] != 0)
                newlen += 1; /* space */

        if (listsize < newlen)
                return EMSGSIZE;

        /* add a space if the list isn't empty and it doesn't already have space */
        if (list[0] != 0 && list[listlen-1] != deli)
        {
                list[listlen++] = deli;
        }

        strncpy(&list[listlen], name, namelen + 1 /* terminate */);

        return 0;
}

/* Utility function to remove duplicate entries in a space separated list
 */

char *
remove_dups(char *inlist, int inlist_size)
{
	char name[256], *next = NULL;
	char *outlist;

	if (!inlist_size)
		return NULL;

	if (!inlist)
		return NULL;

	outlist = (char *) malloc(inlist_size);

	if (!outlist) return NULL;

	memset(outlist, 0, inlist_size);

	foreach(name, inlist, next)
	{
		if (!find_in_list(outlist, name))
		{
			if (strlen(outlist) == 0)
			{
				snprintf(outlist, inlist_size, "%s", name);
			}
			else
			{
				strncat(outlist, " ", inlist_size - strlen(outlist));
				strncat(outlist, name, inlist_size - strlen(outlist));
			}
		}
	}

	strncpy(inlist, outlist, inlist_size);

	free(outlist);
	return inlist;

}

/* Initialization of strbuf structure */
void
str_binit(struct strbuf *b, char *buf, unsigned int size)
{
        b->origsize = b->size = size;
        b->origbuf = b->buf = buf;
}

/* Buffer sprintf wrapper to guard against buffer overflow */
int
str_bprintf(struct strbuf *b, const char *fmt, ...)
{
        va_list ap;
        int r;

        va_start(ap, fmt);

        r = vsnprintf(b->buf, b->size, fmt, ap);

	/* Non Ansi C99 compliant returns -1,
	 * Ansi compliant return r >= b->size,
	 * bcmstdlib returns 0, handle all
	 */
	/* r == 0 is also the case when strlen(fmt) is zero.
	 * typically the case when "" is passed as argument.
	 */
        if ((r == -1) || (r >= (int)b->size)) {
                b->size = 0;
        } else {
                b->size -= r;
                b->buf += r;
        }

        va_end(ap);

        return r;
}

/*
	 return true/false if any wireless interface has URE enabled.
*/
int
ure_any_enabled(void)
{
	return nvram_match("ure_disable", "0");
}


/**
 *	 nvifname_to_osifname()
 *  The intent here is to provide a conversion between the OS interface name
 *  and the device name that we keep in NVRAM.
 * This should eventually be placed in a Linux specific file with other
 * OS abstraction functions.

 * @param nvifname pointer to ifname to be converted
 * @param osifname_buf storage for the converted osifname
 * @param osifname_buf_len length of storage for osifname_buf
 */
int
nvifname_to_osifname(const char *nvifname, char *osifname_buf,
                     int osifname_buf_len)
{
	char varname[NVRAM_MAX_PARAM_LEN];
	char *ptr;

	/* Bail if we get a NULL or empty string */
	if ((!nvifname) || (!*nvifname) || (!osifname_buf)) {
		return -1;
	}

	memset(osifname_buf, 0, osifname_buf_len);

	if (strstr(nvifname, "eth") || strstr(nvifname, ".")) {
		strncpy(osifname_buf, nvifname, osifname_buf_len);
		return 0;
	}

#ifdef RTCONFIG_RALINK
	if (strstr(nvifname, "ra") || strstr(nvifname, ".")) {
		strncpy(osifname_buf, nvifname, osifname_buf_len);
		return 0;
	}
#elif defined(RTCONFIG_QCA)
	if (strstr(nvifname, "ath") || strstr(nvifname, "wifi")) {
		strncpy(osifname_buf, nvifname, osifname_buf_len);
		return 0;
	}
#if defined(RTCONFIG_WIGIG)
	if (strstr(nvifname, "wlan")) {
		strlcpy(osifname_buf, nvifname, osifname_buf_len);
		return 0;
	}
#endif
#endif

	snprintf(varname, sizeof(varname), "%s_ifname", nvifname);
	ptr = nvram_get(varname);
	if (ptr) {
		/* Bail if the string is empty */
		if (!*ptr) return -1;
		strncpy(osifname_buf, ptr, osifname_buf_len);
		return 0;
	}

	return -1;
}


/* osifname_to_nvifname()
 * Convert the OS interface name to the name we use internally(NVRAM, GUI, etc.)
 * This is the Linux version of this function

 * @param osifname pointer to osifname to be converted
 * @param nvifname_buf storage for the converted ifname
 * @param nvifname_buf_len length of storage for nvifname_buf
 */
int
osifname_to_nvifname(const char *osifname, char *nvifname_buf,
                     int nvifname_buf_len)
{
	char varname[NVRAM_MAX_PARAM_LEN];
	int pri, sec;

	/* Bail if we get a NULL or empty string */

	if ((!osifname) || (!*osifname) || (!nvifname_buf))
	{
		return -1;
	}

	memset(nvifname_buf, 0, nvifname_buf_len);

	if (strstr(osifname, "wl") || strstr(osifname, "br") ||
	    strstr(osifname, "wds")
#if defined(RTCONFIG_QCA) && defined(RTCONFIG_WIGIG)
	    || strstr(osifname, "wlan")
#endif
	   )
	{
		strncpy(nvifname_buf, osifname, nvifname_buf_len);
		return 0;
	}

	/* look for interface name on the primary interfaces first */
	for (pri = 0; pri < MAX_RADIOS; pri++) {
		snprintf(varname, sizeof(varname),
					"wl%d_ifname", pri);
		if (nvram_match(varname, (char *)osifname)) {
					snprintf(nvifname_buf, nvifname_buf_len, "wl%d", pri);
					return 0;
				}
	}

	/* look for interface name on the multi-instance interfaces */
	for (pri = 0; pri < MAX_RADIOS; pri++)
		for (sec = 0; sec < MAX_BSS_PER_RADIO; sec++) {
			snprintf(varname, sizeof(varname),
					"wl%d.%d_ifname", pri, sec);
			if (nvram_match(varname, (char *)osifname)) {
				snprintf(nvifname_buf, nvifname_buf_len, "wl%d.%d", pri, sec);
				return 0;
			}
		}

	return -1;
}

#endif	// #if WL_BSS_INFO_VERSION >= 108

/******************************************************************************/
/*
 *	Add a character to a string buffer
 */

static void put_char(strbuf_t *buf, char c)
{
	if (buf->count >= (buf->size - 1)) {
		if (! (buf->flags & STR_REALLOC)) {
			return;
		}
		buf->size += STR_INC;
		if (buf->size > buf->max && buf->size > STR_INC) {
/*
 *			Caller should increase the size of the calling buffer
 */
			buf->size -= STR_INC;
			return;
		}
		if (buf->s == NULL) {
			buf->s = balloc(B_L, buf->size * sizeof(char));
		} else {
			buf->s = brealloc(B_L, buf->s, buf->size * sizeof(char));
		}
	}
	buf->s[buf->count] = c;
	if (c != '\0') {
		++buf->count;
	}
}

/******************************************************************************/
/*
 *	Add a string to a string buffer
 */

static void put_string(strbuf_t *buf, char *s, int len, int width,
		int prec, enum flag f)
{
	int		i;

	if (len < 0) {
		len = strnlen(s, prec >= 0 ? prec : ULONG_MAX);
	} else if (prec >= 0 && prec < len) {
		len = prec;
	}
	if (width > len && !(f & flag_minus)) {
		for (i = len; i < width; ++i) {
			put_char(buf, ' ');
		}
	}
	for (i = 0; i < len; ++i) {
		put_char(buf, s[i]);
	}
	if (width > len && f & flag_minus) {
		for (i = len; i < width; ++i) {
			put_char(buf, ' ');
		}
	}
}

/******************************************************************************/
/*
 *	Add a long to a string buffer
 */

static void put_ulong(strbuf_t *buf, unsigned long int value, int base,
		int upper, char *prefix, int width, int prec, enum flag f)
{
	unsigned long	x, x2;
	int				len, zeros, i;

	for (len = 1, x = 1; x < ULONG_MAX / base; ++len, x = x2) {
		x2 = x * base;
		if (x2 > value) {
			break;
		}
	}
	zeros = (prec > len) ? prec - len : 0;
	width -= zeros + len;
	if (prefix != NULL) {
		width -= strnlen(prefix, ULONG_MAX);
	}
	if (!(f & flag_minus)) {
		if (f & flag_zero) {
			for (i = 0; i < width; ++i) {
				put_char(buf, '0');
			}
		} else {
			for (i = 0; i < width; ++i) {
				put_char(buf, ' ');
			}
		}
	}
	if (prefix != NULL) {
		put_string(buf, prefix, -1, 0, -1, flag_none);
	}
	for (i = 0; i < zeros; ++i) {
		put_char(buf, '0');
	}
	for ( ; x > 0; x /= base) {
		int digit = (value / x) % base;
		put_char(buf, (char) ((digit < 10 ? '0' : (upper ? 'A' : 'a') - 10) +
			digit));
	}
	if (f & flag_minus) {
		for (i = 0; i < width; ++i) {
			put_char(buf, ' ');
		}
	}
}

/******************************************************************************/
/*
 *	Dynamic sprintf implementation. Supports dynamic buffer allocation.
 *	This function can be called multiple times to grow an existing allocated
 *	buffer. In this case, msize is set to the size of the previously allocated
 *	buffer. The buffer will be realloced, as required. If msize is set, we
 *	return the size of the allocated buffer for use with the next call. For
 *	the first call, msize can be set to -1.
 */

static int dsnprintf(char **s, int size, char *fmt, va_list arg, int msize)
{
	strbuf_t	buf;
	char		c;

	assert(s);
	assert(fmt);

	memset(&buf, 0, sizeof(buf));
	buf.s = *s;

	if (*s == NULL || msize != 0) {
		buf.max = size;
		buf.flags |= STR_REALLOC;
		if (msize != 0) {
			buf.size = max(msize, 0);
		}
		if (*s != NULL && msize != 0) {
			buf.count = strlen(*s);
		}
	} else {
		buf.size = size;
	}

	while ((c = *fmt++) != '\0') {
		if (c != '%' || (c = *fmt++) == '%') {
			put_char(&buf, c);
		} else {
			enum flag f = flag_none;
			int width = 0;
			int prec = -1;
			for ( ; c != '\0'; c = *fmt++) {
				if (c == '-') {
					f |= flag_minus;
				} else if (c == '+') {
					f |= flag_plus;
				} else if (c == ' ') {
					f |= flag_space;
				} else if (c == '#') {
					f |= flag_hash;
				} else if (c == '0') {
					f |= flag_zero;
				} else {
					break;
				}
			}
			if (c == '*') {
				width = va_arg(arg, int);
				if (width < 0) {
					f |= flag_minus;
					width = -width;
				}
				c = *fmt++;
			} else {
				for ( ; isdigit((int)c); c = *fmt++) {
					width = width * 10 + (c - '0');
				}
			}
			if (c == '.') {
				f &= ~flag_zero;
				c = *fmt++;
				if (c == '*') {
					prec = va_arg(arg, int);
					c = *fmt++;
				} else {
					for (prec = 0; isdigit((int)c); c = *fmt++) {
						prec = prec * 10 + (c - '0');
					}
				}
			}
			if (c == 'h' || c == 'l') {
				f |= (c == 'h' ? flag_short : flag_long);
				c = *fmt++;
			}
			if (c == 'd' || c == 'i') {
				long int value;
				if (f & flag_short) {
					value = (short int) va_arg(arg, int);
				} else if (f & flag_long) {
					value = va_arg(arg, long int);
				} else {
					value = va_arg(arg, int);
				}
				if (value >= 0) {
					if (f & flag_plus) {
						put_ulong(&buf, value, 10, 0, ("+"), width, prec, f);
					} else if (f & flag_space) {
						put_ulong(&buf, value, 10, 0, (" "), width, prec, f);
					} else {
						put_ulong(&buf, value, 10, 0, NULL, width, prec, f);
					}
				} else {
					put_ulong(&buf, -value, 10, 0, ("-"), width, prec, f);
				}
			} else if (c == 'o' || c == 'u' || c == 'x' || c == 'X') {
				unsigned long int value;
				if (f & flag_short) {
					value = (unsigned short int) va_arg(arg, unsigned int);
				} else if (f & flag_long) {
					value = va_arg(arg, unsigned long int);
				} else {
					value = va_arg(arg, unsigned int);
				}
				if (c == 'o') {
					if (f & flag_hash && value != 0) {
						put_ulong(&buf, value, 8, 0, ("0"), width, prec, f);
					} else {
						put_ulong(&buf, value, 8, 0, NULL, width, prec, f);
					}
				} else if (c == 'u') {
					put_ulong(&buf, value, 10, 0, NULL, width, prec, f);
				} else {
					if (f & flag_hash && value != 0) {
						if (c == 'x') {
							put_ulong(&buf, value, 16, 0, ("0x"), width,
								prec, f);
						} else {
							put_ulong(&buf, value, 16, 1, ("0X"), width,
								prec, f);
						}
					} else {
                  /* 04 Apr 02 BgP -- changed so that %X correctly outputs
                   * uppercase hex digits when requested.
						put_ulong(&buf, value, 16, 0, NULL, width, prec, f);
                   */
						put_ulong(&buf, value, 16, ('X' == c) , NULL, width, prec, f);
					}
				}

			} else if (c == 'c') {
				char value = va_arg(arg, int);
				put_char(&buf, value);

			} else if (c == 's' || c == 'S') {
				char *value = va_arg(arg, char *);
				if (value == NULL) {
					put_string(&buf, ("(null)"), -1, width, prec, f);
				} else if (f & flag_hash) {
					put_string(&buf,
						value + 1, (char) *value, width, prec, f);
				} else {
					put_string(&buf, value, -1, width, prec, f);
				}
			} else if (c == 'p') {
				void *value = va_arg(arg, void *);
				put_ulong(&buf,
					(unsigned long int) value, 16, 0, ("0x"), width, prec, f);
			} else if (c == 'n') {
				if (f & flag_short) {
					short int *value = va_arg(arg, short int *);
					*value = buf.count;
				} else if (f & flag_long) {
					long int *value = va_arg(arg, long int *);
					*value = buf.count;
				} else {
					int *value = va_arg(arg, int *);
					*value = buf.count;
				}
			} else {
				put_char(&buf, c);
			}
		}
	}
	if (buf.s == NULL) {
		put_char(&buf, '\0');
	}

/*
 *	If the user requested a dynamic buffer (*s == NULL), ensure it is returned.
 */
	if (*s == NULL || msize != 0) {
		*s = buf.s;
	}

	if (*s != NULL && size > 0) {
		if (buf.count < size) {
			(*s)[buf.count] = '\0';
		} else {
			(*s)[buf.size - 1] = '\0';
		}
	}

	if (msize != 0) {
		return buf.size;
	}
	return buf.count;
}

/******************************************************************************/
/*
 *	sprintf and vsprintf are bad, ok. You can easily clobber memory. Use
 *	fmtAlloc and fmtValloc instead! These functions do _not_ support floating
 *	point, like %e, %f, %g...
 */

int fmtAlloc(char **s, int n, char *fmt, ...)
{
	va_list	ap;
	int		result;

	assert(s);
	assert(fmt);

	*s = NULL;
	va_start(ap, fmt);
	result = dsnprintf(s, n, fmt, ap, 0);
	va_end(ap);
	return result;
}

/******************************************************************************/
/*
 *	A vsprintf replacement.
 */

int fmtValloc(char **s, int n, char *fmt, va_list arg)
{
	assert(s);
	assert(fmt);

	*s = NULL;
	return dsnprintf(s, n, fmt, arg, 0);
}

/*
 *  * description: parse va and do system
 *  */
int doSystem(char *fmt, ...)
{
	va_list		vargs;
	char		*cmd = NULL;
	int 		rc = 0;
	#define CMD_BUFSIZE 256
	va_start(vargs, fmt);
	if (fmtValloc(&cmd, CMD_BUFSIZE, fmt, vargs) >= CMD_BUFSIZE) {
		fprintf(stderr, "doSystem: lost data, buffer overflow\n");
	}
	va_end(vargs);

	if(cmd) {
		if (!strncmp(cmd, "iwpriv", 6)
#if defined(RTCONFIG_CFG80211)
		    || !strncmp(cmd, "cfg80211tool", 12)
#endif
		   )
			_dprintf("[doSystem] %s\n", cmd);
		rc = system(cmd);
		bfree(B_L, cmd);
	}
	return rc;
}

int
swap_check()
{
	struct sysinfo info;

	sysinfo(&info);

	if(info.totalswap > 0)
		return 1;
	else	return 0;
}

// -----------------------------------------------------------------------------

/*
 * Kills process whose PID is stored in plaintext in pidfile
 * @param	pidfile	PID file
 * @sig  	signal to be send
 * @rm   	whether to remove this pid file (1) or not (0).
 * @return	0 on success and errno on failure
 */

int kill_pidfile_s_rm(char *pidfile, int sig, int rm)
{
	FILE *fp;
	char buf[256];

	if ((fp = fopen(pidfile, "r")) != NULL) {
		if (fgets(buf, sizeof(buf), fp)) {
			pid_t pid = strtoul(buf, NULL, 0);
			fclose(fp);
			if(rm)
				unlink(pidfile);
			return kill(pid, sig);
		}
		fclose(fp);
	}
	return errno;
}

int kill_pidfile(char *pidfile)
{
	return kill_pidfile_s_rm(pidfile, SIGTERM, 1);
}

int kill_pidfile_s(char *pidfile, int sig)
{
	return kill_pidfile_s_rm(pidfile, sig, 0);
}

long uptime(void)
{
	struct sysinfo info;
	sysinfo(&info);

	return info.uptime;
}

float uptime2(void)
{
	char uptime_str[64];
	float uptime, idle;

	if (f_read_string("/proc/uptime", uptime_str, sizeof(uptime_str)) <= 0 ||
	    sscanf(uptime_str, "%f %f", &uptime, &idle) != 2)
		return 0;

	return uptime;
}

int _vstrsep(char *buf, const char *sep, ...)
{
	va_list ap;
	char **p;
	int n;

	n = 0;
	va_start(ap, sep);
	while ((p = va_arg(ap, char **)) != NULL) {
		if ((*p = strsep(&buf, sep)) == NULL) break;
		++n;
	}
	va_end(ap);
	return n;
}

#if defined(CONFIG_BCMWL5) || defined(RTCONFIG_REALTEK) || defined(RTCONFIG_RALINK) || defined(RTCONFIG_LANTIQ)|| defined(RTCONFIG_QCA)
char *
wl_ether_etoa(const struct ether_addr *n)
{
	static char etoa_buf[ETHER_ADDR_LEN * 3];
	char *c = etoa_buf;
	int i;

	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		if (i)
			*c++ = ':';
#if defined(RTCONFIG_LANTIQ)|| defined(RTCONFIG_LANTIQ)|| defined(RTCONFIG_QCA)			
		c += snprintf(c, sizeof(etoa_buf) - (c - etoa_buf), "%02X", n->ether_addr_octet[i] & 0xff);
#else
		c += snprintf(c, sizeof(etoa_buf) - (c - etoa_buf), "%02X", n->octet[i] & 0xff);
#endif		
	}
	return etoa_buf;
}
#endif

void
shortstr_encrypt(unsigned char *src, unsigned char *dst, unsigned char *shift)
{
    unsigned char carry, temp, bytes, bits;
    int i;

    bytes = (*shift % (DATA_WORDS_LEN - 1)) + 1;
    for(i=0; i<DATA_WORDS_LEN; i++) {
        dst[(i + bytes) % DATA_WORDS_LEN] = src[i];
    }

    carry = 0;
    bits = (*shift % 7) + 1;
    for(i=0; i<DATA_WORDS_LEN; i++) {
        temp = dst[i] << (8 - bits);
        dst[i] = (dst[i] >> bits) | carry;
        carry = temp;
    }
    dst[0] |= carry;

    for(i=0; i<DATA_WORDS_LEN; i++) {
        dst[i] ^= ENC_XOR + i * 5;
    }
}

void
shortstr_decrypt(unsigned char *src, unsigned char *dst, unsigned char shift)
{
    unsigned char carry, temp, bytes, bits;
    int i;

    for(i=0; i<DATA_WORDS_LEN; i++) {
        src[i] ^= ENC_XOR + i * 5;
    }
    carry = 0;
    bits = (shift % 7) + 1;
    for(i=DATA_WORDS_LEN - 1; i>=0; i--) {
        temp = src[i] >> (8 - bits);
        src[i] = (src[i] << bits) | carry;
        carry = temp;
    }
    src[DATA_WORDS_LEN - 1] |= carry;

    bytes = (shift % (DATA_WORDS_LEN - 1)) + 1;
    for(i=0; i<DATA_WORDS_LEN; i++) {
        dst[i] = src[(i + bytes) % DATA_WORDS_LEN];
    }

    dst[DATA_WORDS_LEN] = 0;
}


char *enc_str(char *str, char *enc_buf)
{
        unsigned char buf[DATA_WORDS_LEN + 1];
        unsigned char buf2[DATA_WORDS_LEN + 1];

        memset(buf, 0, sizeof(buf));
        memset(buf2, 0, sizeof(buf2));
        memset(enc_buf, 0, ENC_WORDS_LEN);

        strlcpy((char *) buf, str, sizeof(buf));

        shortstr_encrypt(buf, buf2, &used_shift);
        memcpy(enc_buf, buf2, DATA_WORDS_LEN);
        enc_buf[DATA_WORDS_LEN] = used_shift;

        return enc_buf;
}

char *dec_str(char *ec_str, char *dec_buf)
{
        unsigned char buf[DATA_WORDS_LEN + 1];

        memset(buf, 0, sizeof(buf));
        memset(dec_buf, 0, DATA_WORDS_LEN);
        memcpy(buf, ec_str, DATA_WORDS_LEN+1);
        buf[DATA_WORDS_LEN] = 0;
        shortstr_decrypt(buf, (unsigned char *) dec_buf, used_shift);

        return dec_buf;
}

int generate_wireless_key(unsigned char *key)
{
	int i;
	unsigned char ea[ETHER_ADDR_LEN];
	char *mac = nvram_safe_get("et1macaddr");

	memset(key, 0, 32);
	ether_atoe(mac, ea);

	sprintf((char *) key, "%x%x%x%x%x%x%x%x",
		(ea[2] & 0xf0) >> 4,
		(ea[2] & 0x0f),
		(ea[3] & 0xf0) >> 4,
		(ea[3] & 0x0f),
		(ea[4] & 0xf0) >> 4,
		(ea[4] & 0x0f),
		(ea[5] & 0xf0) >> 4,
		(ea[5] & 0x0f));
	key[8] = 0x0;

	for (i = 0; i < 3; i++)
	{
		switch(key[i])
		{
			case '0': key[i] = 'a'; break;
			case '1': key[i] = 'b'; break;
			case '2': key[i] = 'c'; break;
			case '3': key[i] = 'd'; break;
			case '4': key[i] = 'e'; break;
			case '5': key[i] = 'f'; break;
			case '6': key[i] = 'g'; break;
			case '7': key[i] = 'h'; break;
			case '8': key[i] = 'j'; break;
			case '9': key[i] = 'k'; break;
		}
	}

	for (i = 3; i < 8; i++)
	{
		switch(key[i])
		{
			case '0': key[i] = '1'; break;
			case 'a': key[i] = '2'; break;
			case 'b': key[i] = '3'; break;
			case 'c': key[i] = '4'; break;
			case 'd': key[i] = '5'; break;
			case 'e': key[i] = '7'; break;
			case 'f': key[i] = '8'; break;
		}
	}

	printf("key:  %s (%d)\n", (char *)key, (int)strlen((const char *) key));

	return 0;
}

int
strArgs(int argc, char **argv, char *fmt, ...)
{
	va_list	ap;
	int arg;
	char *c;

	if (!argv)
		return 0;

	va_start(ap, fmt);
	for (arg = 0, c = fmt; c && *c && arg < argc;) {
		if (*c++ != '%')
			continue;
		switch (*c) {
		case 'd':
			*(va_arg(ap, int *)) = atoi(argv[arg]);
			break;
		case 's':
			*(va_arg(ap, char **)) = argv[arg];
			break;
		}
		arg++;
	}
	va_end(ap);

	return arg;
}

/**
** trimNL()
** trim trailing new line character(including '\r' and '\n')
**/
char *trimNL(char *str)
{
	int len = 0;

	if(!str)
	{
		return NULL;
	}

	len = strlen(str);

	while((len!=0)&&((str[len-1] == '\r' || str[len-1] == '\n')))
	{
		len--;
	}
	str[len] = '\0';
	return str;
}

/*******************************************************************
* NAME: trimWS
* AUTHOR: Renjie Lee
* CREATE DATE: 2021/05/20
* DESCRIPTION: remove leading and tailing white space(s)
* INPUT:  str: the string to be procesed
* OUTPUT:  None
* RETURN: the string which has neither leading nor tailing white space(s)
* NOTE:
*******************************************************************/
char *trimWS(char *str)
{
	char *end;

	while(*str == ' ')
	{
		str++;
	}

	if(*str == 0)
	{
		return str;
	}

	end = str + strlen(str) - 1;
	while((end > str) && (*end == ' '))
	{
		end--;
	}

	*(end+1) = '\0';

	return str;
}

/**
** get_char_count()
** return the number of occurrence of character 'ch' in the C string 'str'.
** The terminating null-character is considered part of the C string.
** Therefore, it can also be located in order to retrieve a pointer to the end of a string.
**/
int get_char_count(char *str, int ch)
{
	int count = 0;
	char *pch = NULL;

	if(!str)
	{
		return 0;
	}

	pch = strchr(str, ch);
	while(pch != NULL)
	{
		count++;
		pch = strchr(pch+1, ch);
	}
	return count;
}

char *get_process_name_by_pid(const int pid)
{
	static char name[1024];
	snprintf(name, sizeof(name), "/proc/%d/cmdline",pid);
	FILE* f = fopen(name,"r");
	if(f){
		size_t size;
		size = fread(name, sizeof(char), 1024, f);
		if(size>0){
			if('\n'==name[size-1])
			name[size-1]='\0';
		}
		else memset(name, 0, 1024);
		fclose(f);
	}
	else memset(name, 0, 1024);
	return name;
}

int writefile(char *fname,char *content)
{
 FILE *fp;
 int len;

 fp=fopen(fname,"w");
 if (!fp) return 0;
 len=fputs(content,fp);
 fclose(fp);
 if (len>0) return 1;
 return 0;
}

char *readfile(char *fname,int *fsize)
{
 FILE *fp;
 unsigned long size,lsize;
 char *pt;
 int len;
 char buf[100];

 size=0;
 pt=NULL;
 fp=fopen(fname,"r");
 if (!fp) return NULL;
 while (1)
  {
   len=fread(buf,1,100,fp);
   if (len==-1)
    goto sysfail;
   lsize=size;
   size+=len;
   pt=(char *)realloc(pt,size+1);
   if (len==0)
    {
     pt[size]='\0';
     break;
    }
   if (!pt)
    goto sysfail;
   memcpy(pt+lsize,buf,len);
  }
 fclose(fp);
 pt[size]='\0';
 *fsize=size;
 return pt;

sysfail:
 fclose(fp);
 if (pt)
  free(pt);
 return NULL;
}

int modprobe_q(const char *mod)
{
	return eval("modprobe", "-q" , (char *)mod);
}

int modprobe_r(const char *mod)
{
#if 1
	return eval("modprobe", "-r", (char *)mod);
#else
	int r = eval("modprobe", "-r", (char *)mod);
	cprintf("modprobe -r %s = %d\n", mod, r);
	return r;
#endif
}

/**
 * Load kernel modules in @kmods_list in original order.
 * @kmods_list:	a string contains all kernel modules should be loaded by this function.
 * @return:
 * 	0:	success
 *     -1:	invalid parameter
 */
int load_kmods(char *kmods_list)
{
	char kmod[128], *next;

	if (!kmods_list)
		return -1;

	foreach(kmod, kmods_list, next) {
		if (module_loaded(kmod))
			continue;

		modprobe(kmod);
	}

	return 0;
}

/**
 * Remove kernel modules in @kmods_list in REVERSE order.
 * @kmods_list:	a string contains all kernel modules should be loaded by this function.
 * @return:
 * 	0:	success
 *     -1:	invalid parameter
 *     -2:	can't allocate memory for holding parameter.
 */
int remove_kmods(char *kmods_list)
{
	char buf[256], *p, *q;

	if (!kmods_list)
		return -1;

	if (strlen(kmods_list) > sizeof(buf) - 1) {
		p = strdup(kmods_list);
		if (p == NULL) {
			dbg("%s: Can't allocate memory for [%s]\n",
				__func__, kmods_list);
			return -2;
		}
	} else
	{
		strlcpy(buf, kmods_list, sizeof(buf));
		p = buf;
	}

	for (q = NULL; q != p;) {
		q = strrchr(p, ' ') ? : p;
		if (*q == ' ')
			*q++ = '\0';
		if (*q == '\0')
			continue;
		if (!module_loaded(q))
			continue;

		modprobe_r(q);
	}

	if (p != buf)
		free(p);

	return 0;
}

int num_of_wl_if()
{
	char word[256], *next;
	int count = 0;
	char wl_ifnames[32] = { 0 };

	strlcpy(wl_ifnames, nvram_safe_get("wl_ifnames"), sizeof(wl_ifnames));
	foreach (word, wl_ifnames, next)
		count++;

	return count;
}

int num_of_5g_if()
{
#if !defined(CONFIG_BCMWL5)
	char prefix[] = "wlXXXXXXXXXXXX_";
	int band, count = 0;

	for (band = WL_2G_BAND; band < MAX_NR_WL_IF; band++) {
		SKIP_ABSENT_BAND(band);
		snprintf(prefix, sizeof(prefix), "wl%d_", band);
		if (nvram_pf_match(prefix, "nband", "1"))
			count++;
	}
#else
	char word[256], *next;
	char wl_ifnames[32] = { 0 };
	char prefix[] = "wlXXXXXXXXXXXX_", tmp[128];
	int idx = 0, count = 0;

	strlcpy(wl_ifnames, nvram_safe_get("wl_ifnames"), sizeof(wl_ifnames));
	foreach (word, wl_ifnames, next) {
		snprintf(prefix, sizeof(prefix), "wl%d_", idx++);
		if (nvram_match(strcat_r(prefix, "nband", tmp), "1"))
			count++;
	}
#endif
	return count;
}

int num_of_wan_if()
{
	char word[256], *next;
	int count = 0;
	char wan_ifnames[32] = { 0 };

	strlcpy(wan_ifnames, nvram_safe_get("wan_ifnames"), sizeof(wan_ifnames));
	foreach (word, wan_ifnames, next)
		count++;

	return count;
}

int num_of_6g_if()
{
#if defined(RTCONFIG_NOWL)
	char prefix[sizeof("wlXXXXX_")];
	int band, count = 0;

	for (band = 0; band < MAX_NR_WL_BAND; band++) {
		snprintf(prefix, sizeof(prefix), "wl%d_", band);
		if (nvram_pf_match(prefix, "nband", "4"))
			count++;

		if (nvram_pf_get_int(prefix, "nband") == 0)
			break;
	}
#else
#if !defined(CONFIG_BCMWL5)
	char prefix[] = "wlXXXXXXXXXXXX_";
	int band, count = 0;

	for (band = WL_2G_BAND; band < MAX_NR_WL_IF; band++) {
		SKIP_ABSENT_BAND(band);
		snprintf(prefix, sizeof(prefix), "wl%d_", band);
		if (nvram_pf_match(prefix, "nband", "4"))
			count++;
	}
#else
	char word[256], *next;
	char wl_ifnames[32] = { 0 };
	char prefix[] = "wlXXXXXXXXXXXX_", tmp[128];
	int idx = 0, count = 0;

	strlcpy(wl_ifnames, nvram_safe_get("wl_ifnames"), sizeof(wl_ifnames));
	foreach (word, wl_ifnames, next) {
		snprintf(prefix, sizeof(prefix), "wl%d_", idx++);
		if (nvram_match(strcat_r(prefix, "nband", tmp), "4"))
			count++;
	}
#endif
#endif	/* RTCONFIG_NOWL */
	return count;
}

/* hex2str()
 * Convert the hex array to string.
 * @param hex pointer to hex to be converted
 * @param str storage for the converted string
 * @param hex_len length of storage for hex
 * @return TRUE if conversion was successful and FALSE otherwise
 */
int hex2str(unsigned char *hex, char *str, int hex_len)
{
	int i = 0;
	char *d = NULL;
	unsigned char *s = NULL;
	const static char hexdig[] = "0123456789ABCDEF";
	if(hex == NULL||str == NULL)
		return 0;
	d = str;
	s = hex;

	for (i = 0; i < hex_len; i++,s++){
		*d++ = hexdig[(*s >> 4) & 0xf];
		*d++ = hexdig[*s & 0xf];
	}
	*d = 0;
	return 1;
} /* End of hex2str */

int char2hex (char ch)
{
	if(ch >= '0' && ch <= '9')
		return ch - '0';
	ch |= 0x20;
	if(ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	return -1;
}

int str2hex(const char *str, unsigned char *data, size_t size)
{
	int idx,len;
	int v1, v2;

	for(idx = 0, len = 0; len < size; len++) {
		if((v1 = char2hex(str[idx++])) < 0)
			return len;
		if((v2 = char2hex(str[idx++])) < 0)
			return -1;
		data[len] = (unsigned char)((v1 << 4)|v2);
	}
	return len;
}


void reset_stacksize(int newval)
{
	struct rlimit   lim;

	getrlimit(RLIMIT_STACK, &lim);
	printf("\nnow sys rlimit:cur=%d, max=%d\n", (int)lim.rlim_cur, (int)lim.rlim_max);

	if(newval == ASUSRT_STACKSIZE && nvram_get_int("asus_stacksize"))
		newval = nvram_get_int("asus_stacksize");

	lim.rlim_cur = newval;
	if(setrlimit(RLIMIT_STACK, &lim)==-1)
		printf("\nreset stack_size soft limit failed\n");
	else
		printf("\nreset stack_size soft limit as %d\n", newval);
}

#define ARP_CACHE       "/proc/net/arp"
#define ARP_BUFFER_LEN  512
#define IPLEN           16

/* 1/4/6 */
#define ARP_LINE_FORMAT "%20s %*s %*s %20s %*s %20s"

int arpcache(char *tgmac, char *tgip)
{
	FILE *arpCache = fopen(ARP_CACHE, "r");
	if (!arpCache) {
		perror("cannot open arp cache");
		return -1;
	}

	char header[ARP_BUFFER_LEN];
	if (!fgets(header, sizeof(header), arpCache))
	{
		return -1;
	}

	char ipAddr[ARP_BUFFER_LEN], hwAddr[ARP_BUFFER_LEN], device[ARP_BUFFER_LEN];
	while (fscanf(arpCache, ARP_LINE_FORMAT, ipAddr, hwAddr, device) == 3)
	{
		if(strncasecmp(tgmac, hwAddr, IPLEN-1) == 0) {
			strlcpy(tgip, ipAddr, IPLEN);
			break;
		}
	}

	fclose(arpCache);
	return 0;
}

/* In the space-separated/null-terminated list(haystack), try to
 * locate the string "needle" and get the next string from it
 * if required, do a circular search as well
 * if "needle" is NULL, get the first string in the list
 */
char *
find_next_in_list(const char *haystack, const char *needle, char *nextstr, int nextstrlen)
{
        const char *ptr = haystack;
        int needle_len = 0;
        int haystack_len = 0;
        int len = 0;

        if (!haystack || !needle || !nextstr || !*haystack)
                return NULL;

        if (!*needle) {
                goto found_next;
        }

        needle_len = strlen(needle);
        haystack_len = strlen(haystack);

        while (*ptr != 0 && ptr < &haystack[haystack_len])
        {
                /* consume leading spaces */
                ptr += strspn(ptr, " ");

                /* what's the length of the next word */
                len = strcspn(ptr, " ");

                if ((needle_len == len) && (!strncmp(needle, ptr, len))) {

                        ptr += len;

                        if (!(*ptr != 0 && ptr < &haystack[haystack_len])) {
                                ptr = haystack;
                        } else {
                                /* consume leading spaces */
                                ptr += strspn(ptr, " ");
                        }

found_next:
                        /* what's the length of the next word */
                        len = strcspn(ptr, " ");

                        /* copy next value in nextstr */
                        memset(nextstr, 0, nextstrlen);
                        strncpy(nextstr, ptr, len);

                        return (char*) ptr;
                }

                ptr += len;
        }
        return NULL;
}

#ifdef CONFIG_BCMWL5
void retrieve_static_maclist_from_nvram(int idx,int vidx,struct maclist *maclist,int maclist_buf_size)
{
	char prefix[16]={0};
	struct ether_addr *ea;
	char *buf = maclist;
	char tmp[100];
	char var[80], *next;
	unsigned char sta_ea[6] = {0};
	char *nv, *nvp, *b;
#ifdef RTCONFIG_AMAS
	char mac2g[32], mac5g[32], *next_mac;
	char *reMac, *maclist2g, *maclist5g, *timestamp;
	char stamac2g[18] = {0};
	char stamac5g[18] = {0};
#endif

	if(!maclist) return;

	if (vidx>0) {
		snprintf(prefix, sizeof(prefix), "wl%d.%d_", idx, vidx);
	}
	else {
#ifdef RTCONFIG_AMAS
		if (nvram_get_int("re_mode") == 1)
			snprintf(prefix, sizeof(prefix), "wl%d.1_", idx);
		else
#endif
		snprintf(prefix, sizeof(prefix), "wl%d_", idx);
	}

#ifdef RTCONFIG_AMAS
	if (is_cfg_relist_exist())
	{
		if (nvram_get_int("re_mode") == 1) {
			nv = nvp = get_cfg_relist(0);
			if (nv) {
				while ((b = strsep(&nvp, "<")) != NULL) {
					if ((vstrsep(b, ">", &reMac, &maclist2g, &maclist5g, &timestamp) != 4))
						continue;
					/* first mac for sta 2g of dut */
					foreach_44 (mac2g, maclist2g, next_mac)
						break;
					/* first mac for sta 5g of dut */
					foreach_44 (mac5g, maclist5g, next_mac)
						break;

					if (strcmp(reMac, get_lan_hwaddr()) == 0) {
						snprintf(stamac2g, sizeof(stamac2g), "%s", mac2g);
						//dbg("dut 2g sta (%s)\n", stamac2g);
						snprintf(stamac5g, sizeof(stamac5g), "%s", mac5g);
						//dbg("dut 5g sta (%s)\n", stamac5g);
						break;
					}
				}
				free(nv);
			}
		}
	}
#endif

	maclist->count = 0;
	if (!nvram_match(strcat_r(prefix, "macmode", tmp), "disabled")) {
		memset(maclist, 0, sizeof(maclist_buf_size));
		ea = &(maclist->ea[0]);

		nv = nvp = strdup(nvram_safe_get(strcat_r(prefix, "maclist_x", tmp)));
		if (nv) {
			while ((b = strsep(&nvp, "<")) != NULL) {
				if (strlen(b) == 0) continue;

#ifdef RTCONFIG_AMAS
				if(nvram_match(strcat_r(prefix, "macmode", tmp), "allow")){
					if (nvram_get_int("re_mode") == 1) {
						if (strcmp(b, stamac2g) == 0 ||
							strcmp(b, stamac5g) == 0)
							continue;
					}
				}
#endif
				//dbg("maclist sta (%s) in %s\n", b, wlif_name);
				ether_atoe(b, sta_ea);
				memcpy(ea, sta_ea, sizeof(struct ether_addr));
				maclist->count++;
				ea++;
			}
			free(nv);
		}
#ifdef RTCONFIG_AMAS
		if (nvram_match(strcat_r(prefix, "macmode", tmp), "allow"))
		{
			nv = nvp = get_cfg_relist(0);
			if (nv) {
				while ((b = strsep(&nvp, "<")) != NULL) {
					if ((vstrsep(b, ">", &reMac, &maclist2g, &maclist5g, &timestamp) != 4))
						continue;

					if (strcmp(reMac, get_lan_hwaddr()) == 0)
						continue;

					if (idx == 0) {
						foreach_44 (mac2g, maclist2g, next_mac) {
							if (check_re_in_macfilter(idx, mac2g))
								continue;
							//dbg("relist sta (%s) in %s\n", mac2g, wlif_name);
							ether_atoe(mac2g, sta_ea);
							memcpy(ea, sta_ea, sizeof(struct ether_addr));
							maclist->count++;
							ea++;
						}
					}
					else
					{
						foreach_44 (mac5g, maclist5g, next_mac) {
							if (check_re_in_macfilter(idx, mac5g))
								continue;
							//dbg("relist sta (%s) in %s\n", mac5g, wlif_name);
							ether_atoe(mac5g, sta_ea);
							memcpy(ea, sta_ea, sizeof(struct ether_addr));
							maclist->count++;
							ea++;
						}
					}
				}
				free(nv);
			}
		}
#endif

	}
}
#endif

/* Compare two space-separated/null-terminated lists(str1 and str2)
 * NOTE : The individual names in the list should not exceed NVRAM_MAX_VALUE_LEN
 *
 * @param      str1    space-separated/null-terminated list
 * @param      str2    space-separated/null-terminated list
 *
 * @return     0 if both strings are same else return -1
 */
int
compare_lists(char *str1, char *str2)
{
       char name[NVRAM_MAX_VALUE_LEN + 1], *next_str;

       /* Check for arg and len */
       if (!str1 || !str2 || (strlen(str1) != strlen(str2))) {
               return -1;
       }

       /* First check whether each element in str1 list is present in str2's list */
       foreach(name, str1, next_str) {
               if (find_in_list(str2, name) == NULL) {
                       return -1;
               }
       }

       /* Now check whether each element in str2 list is present in str1's list */
       foreach(name, str2, next_str) {
               if (find_in_list(str1, name) == NULL) {
                       return -1;
               }
       }

       return 0;
}
#ifdef RTCONFIG_AMAS
int check_if_exist_ifnames(char *need_check_ifname, char *ifname)
{
	char check_ifname[8] = {}, *next = NULL;
	if(need_check_ifname && ifname){
		if(nvram_safe_get(need_check_ifname)){
			 foreach(check_ifname, nvram_safe_get(need_check_ifname), next) { // find target port interface name
				if (!strncmp(check_ifname, ifname, strlen(check_ifname))) {
				    return 1;
				}
			}
		}
        }
	return 0;
}
#endif

/*******************************************************************
* NAME: get_sys_uptime
* AUTHOR: Renjie Lee
* CREATE DATE: 2022/08/15
* DESCRIPTION: get system uptime from struct sysinfo.
* INPUT:  None
* OUTPUT: None
* RETURN: 0, if something wrong; other values >= 0, if we get system uptime successfully.
* NOTE:
*******************************************************************/
long get_sys_uptime()
{
	struct sysinfo si;
	int err_code = -1;

	memset(&si, 0, sizeof(si));
	err_code = sysinfo(&si);
	if(err_code != 0)
	{
		_dprintf("[%s]Error code=%d\n", __FUNCTION__, err_code);
		return 0;
	}
	return si.uptime;
}

/*******************************************************************
* NAME: wait_ntp_repeat
* AUTHOR: Renjie Lee
* CREATE DATE: 2022/08/29
* DESCRIPTION: Wait for nvram 'ntp_ready' becoming to "1".
* INPUT:  usec, time in microseconds (10^-6 seconds).
*         count, number of loops.
*         The maximum waiting time is (usec x count) micorseconds.
*         So the maximum waiting time of wait_ntp_repeat(2*1000*1000, 3) is 2*3=6 seconds.
* OUTPUT: None
* RETURN: 0, if something wrong; other values >= 0, if we get system uptime successfully.
* NOTE:
*******************************************************************/
void wait_ntp_repeat(unsigned long usec, unsigned int count)
{
	useconds_t small_time = 0;
	unsigned int seconds = 0;

	if(usec <= 0)
	{
		//default: 1 second.
		small_time = 1000*1000;
	}
	else if(usec > 1000000)
	{
		seconds = usec/1000000;
		small_time = usec%1000000;
	}
	else
	{
		small_time = usec;
	}

	if(count <= 0)
	{
		//default: 1 loop.
		count = 1;
	}

	while(nvram_invmatch("ntp_ready", "1") && (count > 0))
	{
		sleep(seconds);
		usleep(small_time);
		count--;
		logmessage("wait_ntp_repeat", "wait for ntp_ready...%d", count);
	}

	if(nvram_invmatch("ntp_ready", "1"))
	{
		logmessage("wait_ntp_repeat", "NTP is still not ready...", count);
	}
}

int parse_ping_content(char *fname, ping_result_t *out)
{
	FILE *fp = NULL;
	char linebuf[256] = {0};
	char alias[160] = {0};
	char ip_addr[64] = {0};
	char scan_format[64] = {0};
	int n = 0;
	int ps;
	int pr;
	double t_min;
	double t_avg;
	double t_max;

	if(!fname)
	{
		_dprintf("Null filename.\n");
		return -1;
	}

	fp = fopen(fname, "r");
	if(fp)
	{
		memset(linebuf, 0, sizeof(linebuf));
		while(fgets(linebuf, sizeof(linebuf), fp))
		{
			if(strstr(linebuf, "PING "))
			{
				snprintf(scan_format, sizeof(scan_format), "PING %%%us (%%%u[^)]", sizeof(alias) -1, sizeof(ip_addr) -1);
				n = sscanf(linebuf, scan_format, alias, ip_addr);
				if(n == 2)
				{
					snprintf(out->alias, sizeof(out->alias), "%s", alias);
					snprintf(out->ip_addr, sizeof(out->ip_addr), "%s", ip_addr);
					out->name_valid = 1;
				}
			}
			else if(strstr(linebuf, "packet loss"))
			{
				n = sscanf(linebuf, "%d packets transmitted, %d packets received", &ps, &pr);
				if(n == 2)
				{
					out->pkt_sent = ps;
					out->pkt_recv = pr;
					if(ps > 0)
					{
						out->pkt_loss_rate = (ps - pr)*100.0/ps;
						if(pr > 0)
						{
							out->data_valid = 1;
						}
					}
				}
				else
				{
					_dprintf("Failed to parse the [%s].\n", linebuf);
					break;
				}
			}
			else if(strstr(linebuf, "round-trip min/avg/max"))
			{
				n = sscanf(linebuf, "round-trip min/avg/max = %lf/%lf/%lf ms", &t_min, &t_avg, &t_max);
				if(n == 3)
				{
					out->min = t_min;
					out->avg = t_avg;
					out->max = t_max;
					out->data_valid = 1;
					break;
				}
				else
				{
					_dprintf("Failed to parse the [%s].\n", linebuf);
					break;
				}
			}
			memset(linebuf, 0, sizeof(linebuf));
		}
		fclose(fp);
	}
	else
	{
		_dprintf("Cannot open [%s].\n", fname);
	}

	return 0;
}

/*******************************************************************
* NAME: ping_target_with_size
* AUTHOR: Renjie Lee
* CREATE DATE: 2022/10/07
* DESCRIPTION: Ping a target with specific packet size.
* INPUT:  target, the target for ping command
*         pkt_size, send pkt_size data bytes in packets (default:56)
*         ping_cnt, send only ping_cnt pings
*         wait_time, seconds to wait for the first response (default:10) (after all -c CNT packets are sent)
*         loss_rate, packet loss rate, 0.0 <= loss_rate <= 100.0
* OUTPUT: None
* RETURN: 0, if something wrong; 1, function works correctly.
* NOTE:
*******************************************************************/
int ping_target_with_size(char *target, unsigned int pkt_size, unsigned int ping_cnt, unsigned int wait_time, double loss_rate)
{
	char ping_result[160] = {0};
	char ping_done[160] = {0};
	char cmdbuf[1024] = {0};
	ping_result_t data;
	int sig;
	sigset_t set;

	if(!target || (pkt_size <= 0) || (ping_cnt <= 0) || (wait_time <= 0) || (loss_rate < 0))
	{
		return 0;
	}
	else
	{
		snprintf(ping_result, sizeof(ping_result), "/tmp/ping_%s_%d", target, pkt_size);
		unlink(ping_result);
		snprintf(ping_done, sizeof(ping_done), "/tmp/ping_%s_%d.done", target, pkt_size);
		unlink(ping_done);

		if(fork() == 0)
		{
			//child

			setsid();

			// reset signal handlers
			for (sig = 1; sig < _NSIG; sig++)
			{
				signal(sig, SIG_DFL);
			}

			// unblock signals if called from signal handler
			sigemptyset(&set);
			sigprocmask(SIG_SETMASK, &set, NULL);

			snprintf(cmdbuf, sizeof(cmdbuf), "ping -c %d -W %d -s %d %s > %s 2>&1 || true && echo \"\" >> %s", ping_cnt, wait_time, pkt_size, target, ping_result, ping_done);
			system(cmdbuf);
			logmessage("ping_target_with_size", "Ping test is complete.\n");
			exit(0);
		}
		else
		{
			//parent
			sleep(3);

			if(f_exists(ping_result))
			{
				memset(&data, 0, sizeof(ping_result_t));
				if(parse_ping_content(ping_result, &data) == 0)
				{
					//_dprintf("pkt_loss_rate=[%lf]\n", data.pkt_loss_rate);
					if(data.pkt_loss_rate <= loss_rate)
					{
						logmessage("ping_target_with_size", "Successful to ping target(%s) with size(%d)\n", target, pkt_size);
						return 1;
					}
					else
					{
						logmessage("ping_target_with_size", "Failed to ping target(%s) with size(%d)\n", target, pkt_size);
						return 0;
					}
				}
			}
			logmessage("ping_target_with_size", "Cannot find ping result(%s)\n", ping_result);
			return 0;
		}
	}
}

/*******************************************************************
* NAME: replace_literal_newline
* AUTHOR: Renjie Lee
* CREATE DATE: 2023/03/21
* DESCRIPTION: Replace literal newline(s) ("\n")  with newline character(s) ('\n').
*                        That is to say, from "\n" to '\n'.
* INPUT:  inputstr, the string to be replaced.
*         output, the replaced string will be stored in 'output'.
*         buflen, the size of 'output' buffer.
* OUTPUT: None
* RETURN: -1 or -2, if something went wrong; 1, function works correctly.
* NOTE:
*******************************************************************/
int replace_literal_newline(char *inputstr, char *output, int buflen)
{
	int in = 0;
	int out = 0;
	int len = 0;

	if((!inputstr) || (strlen(inputstr) <= 0))
	{
		logmessage("replace_literal_newline", "Wrong inputstr.\n");
		return -1;
	}

	if((!output) || (buflen == 0))
	{
		logmessage("replace_literal_newline", "Wrong output buffer\n");
		return -2;
	}

	len = strlen(inputstr);
	for(in = 0; (in < len) && (out < buflen); in++, out++)
	{
		if(in == len -1)
		{
			//boundary condition
			output[out] = inputstr[in];
		}
		else if((inputstr[in] == '\\') && (inputstr[in+1] == 'n'))
		{
			output[out] = '\n';
			in++;
		}
		else
		{
			output[out] = inputstr[in];
		}
	}
	return 1;
}
