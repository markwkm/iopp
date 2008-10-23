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

/* Prototypes */
struct io_node *get_ion(int);
struct io_node *new_ion(char *);
void upsert_data(struct io_node *);

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
	printf("%5s %8s %8s %8s %8s %8s %8s %8s %s\n", "pid", "rchar", "wchar",
			"syscr", "syscw", "rbytes", "wbytes", "cwbytes", "command");

	/* Loop through the process table and display a line per pid. */
	while ((ent = readdir(dir)) != NULL)
	{
		int fd;
		int length;
		char *p;
		char *q;

		struct io_node *ion;
		struct io_node *old_ion;

		if (!isdigit(ent->d_name[0]))
			continue;

		ion = new_ion(ent->d_name);

		/* Read 'io' file. */
		sprintf(filename, "%s/%s/stat", PROC, ent->d_name);
		fd = open(filename, O_RDONLY);
		if (fd == -1)
		{
			free(ion);
			continue;
		}
		/*
		 * The command is near the beginning; we don't need to be able to
		 * the entire stat file.
		 */
		length = read(fd, buffer, sizeof(buffer) - 1);
		close(fd);
		buffer[length] = '\0';
		p = strchr(buffer, '(');
		++p;
		q = strchr(p, ')');
		length = q - p;
		strncpy(ion->command, p, length);
		ion->command[length] = '\0';

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
			printf("%5d %8lld %8lld %8lld %8lld %8lld %8lld %8lld %s\n",
					ion->pid,
					ion->rchar - old_ion->rchar,
					ion->wchar - old_ion->wchar,
					ion->syscr - old_ion->syscr,
					ion->syscw - old_ion->syscw,
					ion->read_bytes - old_ion->read_bytes,
					ion->write_bytes - old_ion->write_bytes,
					ion->cancelled_write_bytes - old_ion->cancelled_write_bytes,
					ion->command);
		else
			/* No previous data, show 0's instead of calculating negatives. */
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
	printf("usage: iopp [delay [count]]\n");
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
				{ "help", no_argument, 0, 'h' },
				{ 0, 0, 0, 0 }
		};

		c = getopt_long(argc, argv, "h", long_options, &option_index);
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
		case 'h':
			usage();
			return 0;
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
