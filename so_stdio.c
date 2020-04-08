#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "so_stdio.h"

#define OK 0
#define BUF_SIZE 4096
#define WRITE 1
#define EOF_FLAG 1
#define BASH "/bin/sh"


struct _so_file {
	int fd;
	unsigned char *buff;
	int bufcnt;
	int bufpoz;
	int bufsize;
	int eof;
	int err;
	int write;
	int pid;
};
size_t xwrite(SO_FILE *stream)
{
	size_t bytes_written = 0;

	while (bytes_written < stream->bufcnt) {
		size_t bytes_written_now = write(stream->fd,
				stream->buff + bytes_written,
					stream->bufcnt - bytes_written);
		if (bytes_written_now <= 0)
			return SO_EOF;
		bytes_written += bytes_written_now;
	}
	return bytes_written;
}

int file_mode(const char *mode, int *ok)
{
	int flag = 0;

	if (strncmp(mode, "r", 1) == 0) {
		*ok = 1;
		flag = O_RDONLY;
	}

	if (strncmp(mode, "r+", 2) == 0) {
		*ok = 1;
		flag = O_RDWR;
	}

	if (strncmp(mode, "w", 1) == 0) {
		*ok = 1;
		flag = O_CREAT | O_TRUNC | O_WRONLY;
	}

	if (strncmp(mode, "w+", 2) == 0) {
		*ok = 1;
		flag =  O_CREAT | O_TRUNC | O_RDWR;
	}

	if (strncmp(mode, "a", 1) == 0) {
		*ok = 1;
		flag = O_CREAT | O_APPEND | O_WRONLY;
	}

	if (strncmp(mode, "a+", 2) == 0) {
		*ok = 1;
		flag = O_CREAT | O_APPEND | O_RDWR;
	}
	return flag;
}

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	int fd;
	int flag;
	int ok = 0;

	SO_FILE *file = (SO_FILE *) calloc(1, sizeof(SO_FILE));

	flag = file_mode(mode, &ok);

	if (ok == 0) {
		free(file);
		return NULL;
	}

	fd = open(pathname, flag, 0644);

	if (fd < 0) {
		free(file);
		return NULL;
	}

	file->buff = (unsigned char *)calloc(BUF_SIZE, sizeof(unsigned char));

	if (!file->buff) {
		free(file);
		return NULL;
	}
	file->fd = fd;
	file->bufsize = BUF_SIZE;
	file->pid = -1;
	return file;
}

int so_fclose(SO_FILE *stream)
{
	int ret = OK;

	ret = so_fflush(stream);
	if (ret == -1) {
		free(stream->buff);
		free(stream);
		return ret;
	}
	free(stream->buff);
	ret = close(stream->fd);
	free(stream);
	return ret;
}

int so_fflush(SO_FILE *stream)
{
	int rc = 0;

	if (stream->write && stream->bufcnt) {
		rc = xwrite(stream);
		stream->write = 0;
		if (rc < 0) {
			stream->err = -1;
			return SO_EOF;
		}
	}
	stream->bufpoz = 0;
	stream->bufcnt = 0;
	memset(stream->buff, 0, stream->bufsize);

	return 0;
}

int so_fseek(SO_FILE *stream, long offset, int whence)
{
	if (stream->write) {
		so_fflush(stream);
	} else {
		stream->bufpoz = 0;
		stream->bufcnt = 0;
		memset(stream->buff, 0, stream->bufsize);
	}

	if (lseek(stream->fd, offset, whence) >= 0)
		return 0;

	return -1;
}

long so_ftell(SO_FILE *stream)
{
	int file_pos = lseek(stream->fd, 0, SEEK_CUR);

	if (stream->write)
		return stream->bufcnt + file_pos;
	return file_pos - stream->bufcnt + stream->bufpoz;
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	int bytesToRead = nmemb * size;
	int bytesRead = 0, i;

	for (i = 0; i < bytesToRead; i++) {
		int charRead = so_fgetc(stream);

		if (so_feof(stream) == 0) {
			((char *)ptr)[i] = charRead;
			bytesRead++;
		} else
			break;
	}

	return bytesRead / size;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{

	int bytesToRead = nmemb * size;
	int i, bytesRead = 0;

	for (i = 0; i < bytesToRead; i++) {
		so_fputc(((char *)ptr)[i], stream);
		bytesRead++;
	}
	return bytesRead / size;
}

int so_fgetc(SO_FILE *stream)
{
	int res, readBytes = 0;

	if (stream->bufpoz - stream->bufcnt >= 0) {
		readBytes = read(stream->fd, stream->buff, stream->bufsize);
		if (readBytes > 0) {
			stream->bufcnt = readBytes;
			stream->bufpoz = 0;
		} else {
			stream->err = -1;
			stream->eof = EOF_FLAG;
			return SO_EOF;
		}
	}
	res = (int)stream->buff[stream->bufpoz];

	stream->bufpoz++;

	return res;
}

int so_fputc(int c, SO_FILE *stream)
{
	int rc = 0;

	stream->bufpoz = 0;

	if (stream->bufsize - stream->bufcnt == 0) {

		rc = xwrite(stream);
		memset(stream->buff, 0, stream->bufsize);
		stream->bufcnt = 0;
		if (rc == 0) {
			stream->eof = EOF_FLAG;
			return SO_EOF;
		} else if (rc < 0) {
			stream->err = -1;
			return SO_EOF;
		}
	}
	stream->buff[stream->bufcnt] = c;
	stream->bufcnt++;
	stream->write = WRITE;
	return c;
}

int so_fileno(SO_FILE *stream)
{
	return stream->fd;
}

int so_feof(SO_FILE *stream)
{
	if (stream->eof)
		return SO_EOF;
	else
		return OK;
}

int so_ferror(SO_FILE *stream)
{
	if (stream->err)
		return SO_EOF;
	else
		return OK;

}

SO_FILE *so_popen(const char *command, const char *type)
{
	int fd[2], file_descriptor;
	int childpid = -1;
	SO_FILE *file;

	pipe(fd);

	file = (SO_FILE *) calloc(1, sizeof(SO_FILE));
	file->buff = (unsigned char *)malloc(sizeof(char) * BUF_SIZE);

	if (!file->buff) {
		free(file);
		return NULL;
	}

	childpid = fork();
	if (childpid == -1) {
		free(file->buff);
		free(file);
		return NULL;
	}

	if (childpid == 0) {
		if (type[0] == 'r') {
			if (fd[1] != STDOUT_FILENO)
				dup2(fd[1], STDOUT_FILENO);
			close(fd[0]);
		} else {
			if (fd[0] != STDIN_FILENO) {
				dup2(fd[0], STDIN_FILENO);
				close(fd[0]);
			}
			close(fd[1]);
		}

		execlp(BASH, "sh", "-c", command, NULL);
		return NULL;
	}

	if (type[0] == 'r') {
		file_descriptor = fd[0];
		close(fd[1]);
	} else {
		file_descriptor = fd[1];
		close(fd[0]);
	}
	file->fd = file_descriptor;
	file->bufsize = BUF_SIZE;
	file->pid = childpid;
	return file;
}
int so_pclose(SO_FILE *stream)
{
	int pid = -1;
	int pstat;
	int waitedPid = stream->pid;

	so_fclose(stream);

	pid = waitpid(waitedPid, &pstat, 0);

	if (pid == -1)
		return SO_EOF;

	return pstat;
}
