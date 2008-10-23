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

#define PROC "/proc"

#define GET_VALUE(v) \
		p = strchr(p, ':'); \
		++p; \
		q = strchr(p, '\n'); \
		length = q - p; \
		strncpy(value, p, length); \
		value[length] = '\0'; \
		v = atoll(value);

void
get_stats()
{
	DIR *dir = opendir(PROC);
	struct dirent *ent;
	char filename[64];
	char buffer[256];
	long long rchar;
	long long wchar;
	long long syscr;
	long long syscw;
	long long read_bytes;
	long long write_bytes;
	long long cancelled_write_bytes;
	char command[64];

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

		if (!isdigit(ent->d_name[0]))
			continue;

		/* Read 'io' file. */
		sprintf(filename, "%s/%s/stat", PROC, ent->d_name);
		fd = open(filename, O_RDONLY);
		if (fd == -1)
			continue;
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
		strncpy(command, p, length);
		command[length] = '\0';

		/* Read 'io' file. */
		sprintf(filename, "%s/%s/io", PROC, ent->d_name);
		fd = open(filename, O_RDONLY);
		if (fd == -1)
			continue;
		length = read(fd, buffer, sizeof(buffer) - 1);
		close(fd);
		buffer[length] = '\0';

		/* Parsing the io file data. */
		p = buffer;
		GET_VALUE(rchar);
		GET_VALUE(wchar);
		GET_VALUE(syscr);
		GET_VALUE(syscw);
		GET_VALUE(read_bytes);
		GET_VALUE(write_bytes);
		GET_VALUE(cancelled_write_bytes);

		/* Display the pid's io data. */
		printf("%5s %8lld %8lld %8lld %8lld %8lld %8lld %8lld %s\n",
				ent->d_name, rchar, wchar, syscr, syscw, read_bytes,
				write_bytes, cancelled_write_bytes, command);
	}
	closedir(dir);
}

int
main(int argc, char *argv[])
{
	get_stats();
	return 0;
}
