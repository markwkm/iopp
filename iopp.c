/*
 * Copyright (C) 2008 Mark Wong
 */

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#define PROC "/proc"

#define GET_VALUE(v) \
		p = strchr(p, ':'); \
		++p; \
		q = strchr(p, '\n'); \
		length = q - p; \
		strncpy(value, p, length); \
		value[length] = '\0'; \
		v = atoll(value);

#define BTOKB(b) b >> 10
#define BTOMB(b) b >> 20

struct io_node
{
	int pid;
	long long rchar;
	long long wchar;
	long long syscr;
	long long syscw;
	long long read_bytes;
	long long write_bytes;
	long long cancelled_write_bytes;
	char command[64];
	struct io_node *next;
};

struct io_node *head = NULL;
int command_flag = 0;
int idle_flag = 0;
int mb_flag = 0;
int kb_flag = 0;

/* Prototypes */
struct io_node *get_ion(int);
struct io_node *new_ion(char *);
void upsert_data(struct io_node *);

int
get_cmdline(struct io_node *ion)
{
	int fd;
	int length;
	char filename[64];
	char buffer[256];
	char *p;
	char *q;


	sprintf(filename, "%s/%d/cmdline", PROC, ion->pid);
	fd = open(filename, O_RDONLY);
	if (fd == -1)
		return 1;
	length = read(fd, buffer, sizeof(buffer) - 1);
	close(fd);
	if (length == 0)
		return 2;
	buffer[length] = '\0';
	if (command_flag == 0)
	{
		/*
		 * The command is near the beginning; we don't need to be able to
		 * the entire stat file.
		 */
		p = strchr(buffer, '(');
		++p;
		q = strchr(p, ')');
		length = q - p;
	}
	else
		p = buffer;
	strncpy(ion->command, p, length);
	ion->command[length] = '\0';
	return 0;
}

struct io_node *
get_ion(int pid)
{
	struct io_node *c = head;

	while (c != NULL)
	{
		if (c->pid == pid)
			break;
		c = c->next;
	}
	return c;
}

int
get_tcomm(struct io_node *ion)
{
	int fd;
	int length;
	char filename[64];
	char buffer[256];
	char *p;
	char *q;

	sprintf(filename, "%s/%d/stat", PROC, ion->pid);
	fd = open(filename, O_RDONLY);
	if (fd == -1)
		return 1;
	length = read(fd, buffer, sizeof(buffer) - 1);
	close(fd);
	buffer[length] = '\0';
	/*
	 * The command is near the beginning; we don't need to be able to
	 * the entire stat file.
	 */
	p = strchr(buffer, '(');
	++p;
	q = strchr(p, ')');
	length = q - p;
	strncpy(ion->command, p, length);
	ion->command[length] = '\0';
	return 0;
}

struct io_node *
insert_ion(struct io_node *ion)
{
	struct io_node *c;
	struct io_node *p;

	/* Check the head of the list as a special case. */
	if (ion->pid < head->pid)
	{
		ion->next = head;
		head = ion;
		return head;
	}

	c = head->next;
	p = head;
	while (c != NULL)
	{
		if (ion->pid < c->pid)
		{
			ion->next = c;
			p->next = ion;
			return head;
		}
		p = c;
		c = c->next;
	}

	/* Append to the end of the list. */
	if (c == NULL)
		p->next = ion;

	return head;
}

void
get_stats()
{
	DIR *dir = opendir(PROC);
	struct dirent *ent;
	char filename[64];
	char buffer[256];

	char value[64];

	/* Display column headers. */
	if (kb_flag == 1)
		printf("%5s %8s %8s %8s %8s %8s %8s %8s %s\n", "pid", "rchar", "wchar",
				"syscr", "syscw", "rkb", "wkb", "cwkb", "command");
	else if (mb_flag == 1)
		printf("%5s %8s %8s %8s %8s %8s %8s %8s %s\n", "pid", "rchar", "wchar",
				"syscr", "syscw", "rmb", "wmb", "cwmb", "command");
	else
		printf("%5s %8s %8s %8s %8s %8s %8s %8s %s\n", "pid", "rchar", "wchar",
				"syscr", "syscw", "rbytes", "wbytes", "cwbytes", "command");

	/* Loop through the process table and display a line per pid. */
	while ((ent = readdir(dir)) != NULL)
	{
		int rc;
		int fd;
		int length;

		char *p;
		char *q;

		struct io_node *ion;
		struct io_node *old_ion;

		long long rchar;
		long long wchar;
		long long syscr;
		long long syscw;
		long long read_bytes;
		long long write_bytes;
		long long cancelled_write_bytes;

		if (!isdigit(ent->d_name[0]))
			continue;

		ion = new_ion(ent->d_name);

		if (command_flag == 1)
			rc = get_cmdline(ion);
		if (command_flag == 0 || rc != 0)
			/* If the full command line is not asked for or is empty... */
			rc = get_tcomm(ion);

		if (rc != 0)
		{
			free(ion);
			continue;
		}

		/* Read 'io' file. */
		sprintf(filename, "%s/%s/io", PROC, ent->d_name);
		fd = open(filename, O_RDONLY);
		if (fd == -1)
		{
			free(ion);
			continue;
		}
		length = read(fd, buffer, sizeof(buffer) - 1);
		close(fd);
		buffer[length] = '\0';

		/* Parsing the io file data. */
		p = buffer;
		GET_VALUE(ion->rchar);
		GET_VALUE(ion->wchar);
		GET_VALUE(ion->syscr);
		GET_VALUE(ion->syscw);
		GET_VALUE(ion->read_bytes);
		GET_VALUE(ion->write_bytes);
		GET_VALUE(ion->cancelled_write_bytes);

		old_ion = get_ion(ion->pid);

		/* Display the pid's io data. */
		if (old_ion != NULL)
		{
			rchar = ion->rchar - old_ion->rchar;
			wchar = ion->wchar - old_ion->wchar;
			syscr = ion->syscr - old_ion->syscr;
			syscw = ion->syscw - old_ion->syscw;
			read_bytes = ion->read_bytes - old_ion->read_bytes;
			write_bytes = ion->write_bytes - old_ion->write_bytes;
			cancelled_write_bytes = ion->cancelled_write_bytes -
					old_ion->cancelled_write_bytes;

			if (kb_flag == 1)
			{
				rchar = BTOKB(rchar);
				wchar = BTOKB(wchar);
				syscr = BTOKB(syscr);
				syscw = BTOKB(syscw);
				read_bytes = BTOKB(read_bytes);
				write_bytes = BTOKB(write_bytes);
				cancelled_write_bytes = BTOKB(cancelled_write_bytes);
			}
			else if (mb_flag == 1)
			{
				rchar = BTOMB(rchar);
				wchar = BTOMB(wchar);
				syscr = BTOMB(syscr);
				syscw = BTOMB(syscw);
				read_bytes = BTOMB(read_bytes);
				write_bytes = BTOMB(write_bytes);
				cancelled_write_bytes = BTOMB(cancelled_write_bytes);
			}

			if (idle_flag == 1 && rchar == 0 && wchar == 0 && syscr == 0 &&
					syscw == 0 && read_bytes == 0 && write_bytes == 0 &&
					cancelled_write_bytes == 0)
				continue;

			printf("%5d %8lld %8lld %8lld %8lld %8lld %8lld %8lld %s\n",
					ion->pid,
					rchar,
					wchar,
					syscr,
					syscw,
					read_bytes,
					write_bytes,
					cancelled_write_bytes,
					ion->command);
		}
		else if (idle_flag != 1)
			/*
			 * No previous data, show 0's instead of calculating negatives
			 * only if we are shoring idle processes.
			 */
			printf("%5d %8d %8d %8d %8d %8d %8d %8d %s\n",
					ion->pid, 0, 0, 0, 0, 0, 0, 0, ion->command);

		upsert_data(ion);
	}
	closedir(dir);
	return;
}

struct io_node *
new_ion(char *pid)
{
	struct io_node *ion;

	ion = (struct io_node *) malloc(sizeof(struct io_node));
	bzero(ion, sizeof(struct io_node));
	ion->pid = atoi(pid);

	return ion;
}

void
upsert_data(struct io_node *ion)
{
	struct io_node *n;

	/* List is empty. */
	if (head == NULL)
	{
		head = ion;
		return;
	}

	/* Check if we have seen this pid before. */
	n = head;
	while (n != NULL)
	{
		if (n->pid == ion->pid)
		{
			n->rchar = ion->rchar;
			n->wchar = ion->wchar;
			n->syscr = ion->syscr;
			n->syscw = ion->syscw;
			n->read_bytes = ion->read_bytes;
			n->write_bytes = ion->write_bytes;
			n->cancelled_write_bytes = ion->cancelled_write_bytes;
			/*
			 * If the pids wrap, then the command may be different then before.
			 */
			strcpy(n->command, ion->command);
			free(ion);
			return;
		}
		n = n->next;
	}

	/* Add this pid to the list. */
	head = insert_ion(ion);
	return;
}

void
usage()
{
	printf("usage: iopp -h|--help\n");
	printf("usage: iopp [-ci] [-k|-m] [delay [count]]\n");
	printf("            -c, --command display full command line\n");
	printf("            -h, --help display help\n");
	printf("            -i, --idle hides idle processes\n");
	printf("            -k, --kilobytes display data in kilobytes\n");
	printf("            -m, --megabytes display data in megabytes\n");
}

int
main(int argc, char *argv[])
{
	int c;

	int delay = 0;
	int count = 0;
	int max_count = 1;

	while (1)
	{
		int option_index = 0;
		static struct option long_options[] = {
				{ "command", no_argument, 0, 'c' },
				{ "help", no_argument, 0, 'h' },
				{ "idle", no_argument, 0, 'i' },
				{ "kilobytes", no_argument, 0, 'k' },
				{ "megabytes", no_argument, 0, 'm' },
				{ 0, 0, 0, 0 }
		};

		c = getopt_long(argc, argv, "chikm", long_options, &option_index);
		if (c == -1)
		{
			/* Handle delay and count arguments. */

			if (argc == optind)
				break; /* No additional arguments. */
			else if ((argc - optind) == 1)
			{
				delay = atoi(argv[optind]);
				max_count = -1;
			}
			else if ((argc - optind) == 2)
			{
				delay = atoi(argv[optind]);
				max_count = atoi(argv[optind + 1]);
			}
			else
			{
				/* Too many additional arguments. */
				usage();
				return 3;
			}
			break;
		}

		switch (c)
		{
		case 'c':
			command_flag = 1;
			break;
		case 'h':
			usage();
			return 0;
		case 'i':
			idle_flag = 1;
			break;
		case 'k':
			kb_flag = 1;
			break;
		case 'm':
			mb_flag = 1;
			break;
		default:
			usage();
			return 2;
		}
	}

	while (max_count == -1 || count++ < max_count)
	{
		get_stats();
		if (count != max_count)
			sleep(delay);
	}
	return 0;
}
