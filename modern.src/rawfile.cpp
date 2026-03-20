/*
**	Command & Conquer(tm)
**	Copyright 2025 Electronic Arts Inc.
**	Modernized for Android/Linux port — POSIX file I/O replacing DOS _dos_* calls.
*/

#include "function.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "compat.h"
#include "rawfile.h"


/***********************************************************************************************
 * RawFileClass::Error -- Handles displaying a file error message.                             *
 *   Simplified for Android — logs to stderr instead of graphics-mode error display.           *
 *=============================================================================================*/
void RawFileClass::Error(int error, int canretry, char const * filename)
{
	(void)canretry;
	fprintf(stderr, "File error %d (%s)", error, strerror(error));
	if (filename) {
		fprintf(stderr, " on '%s'", filename);
	}
	fprintf(stderr, "\n");
}


/***********************************************************************************************
 * RawFileClass::RawFileClass -- Simple constructor for a file object.                         *
 *=============================================================================================*/
RawFileClass::RawFileClass(char const *filename) : Rights(0), Handle(-1), Filename(0), Allocated(false)
{
	Set_Name(filename);
}


/***********************************************************************************************
 * RawFileClass::Set_Name -- Manually sets the name for a file object.                         *
 *=============================================================================================*/
char const * RawFileClass::Set_Name(char const *filename)
{
	if (Filename && Allocated) {
		free((char *)Filename);
		Filename = 0;
		Allocated = false;
	}

	if (filename) {
		Filename = strdup(filename);
		if (Filename) {
			Allocated = true;
		}
	}
	return(Filename);
}


/***********************************************************************************************
 * RawFileClass::~RawFileClass -- Default destructor for a file object.                        *
 *=============================================================================================*/
RawFileClass::~RawFileClass(void)
{
	Close();
	if (Allocated && Filename) {
		free((char *)Filename);
		Filename = 0;
		Allocated = false;
	}
}


/***********************************************************************************************
 * RawFileClass::RawFileClass -- Copy constructor.                                             *
 *=============================================================================================*/
RawFileClass::RawFileClass(RawFileClass const & f) : Rights(0), Handle(-1), Filename(0), Allocated(false)
{
	Set_Name(f.Filename);
}


/***********************************************************************************************
 * RawFileClass::operator = -- Assignment operator.                                            *
 *=============================================================================================*/
RawFileClass & RawFileClass::operator = (RawFileClass const & f)
{
	if (this != &f) {
		Close();
		Set_Name(f.Filename);
	}
	return *this;
}


/***********************************************************************************************
 * RawFileClass::Create -- Creates an empty file.                                              *
 *=============================================================================================*/
int RawFileClass::Create(void)
{
	Close();
	if (Open(WRITE)) {
		Close();
		return(true);
	}
	return(false);
}


/***********************************************************************************************
 * RawFileClass::Delete -- Deletes the file from disk.                                         *
 *=============================================================================================*/
int RawFileClass::Delete(void)
{
	Close();
	if (Filename) {
		return(unlink(Filename) == 0);
	}
	return(false);
}


/***********************************************************************************************
 * RawFileClass::Is_Available -- Checks if the file exists and can be opened.                  *
 *=============================================================================================*/
int RawFileClass::Is_Available(int forced)
{
	(void)forced;
	if (Is_Open()) return(true);
	if (!Filename) return(false);

	int fd = open(Filename, O_RDONLY);
	if (fd >= 0) {
		close(fd);
		return(true);
	}
	return(false);
}


/***********************************************************************************************
 * RawFileClass::Open -- Opens the file with specified access rights.                          *
 *=============================================================================================*/
int RawFileClass::Open(int rights)
{
	Close();
	Rights = rights;

	int flags = 0;
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	switch (rights) {
		case READ:
			flags = O_RDONLY;
			break;

		case WRITE:
			flags = O_WRONLY | O_CREAT | O_TRUNC;
			break;

		case READ|WRITE:
			flags = O_RDWR | O_CREAT;
			break;

		default:
			errno = EINVAL;
			Error(errno, false, Filename);
			return(false);
	}

	Handle = open(Filename, flags, mode);
	if (Handle < 0) {
		Error(errno, false, Filename);
		return(false);
	}
	return(true);
}

int RawFileClass::Open(char const *filename, int rights)
{
	Set_Name(filename);
	return Open(rights);
}


/***********************************************************************************************
 * RawFileClass::Close -- Closes the file.                                                     *
 *=============================================================================================*/
void RawFileClass::Close(void)
{
	if (Is_Open()) {
		close(Handle);
		Handle = -1;
	}
}


/***********************************************************************************************
 * RawFileClass::Read -- Reads bytes into buffer.                                              *
 *=============================================================================================*/
long RawFileClass::Read(void *buffer, long size)
{
	int opened = false;

	if (!buffer || !size) return(0);

	if (!Is_Open()) {
		if (!Open(READ)) return(0);
		opened = true;
	}

	long total = 0;
	while (size > 0) {
		ssize_t got = read(Handle, buffer, size);
		if (got <= 0) break;
		total += got;
		size -= got;
		buffer = (char*)buffer + got;
	}

	if (opened) Close();
	return(total);
}


/***********************************************************************************************
 * RawFileClass::Write -- Writes data to file.                                                 *
 *=============================================================================================*/
long RawFileClass::Write(void const *buffer, long size)
{
	int opened = false;

	if (!buffer || !size) return(0);

	if (!Is_Open()) {
		if (!Open(WRITE)) return(0);
		opened = true;
	}

	long total = 0;
	while (size > 0) {
		ssize_t wrote = write(Handle, buffer, size);
		if (wrote <= 0) break;
		total += wrote;
		size -= wrote;
		buffer = (const char*)buffer + wrote;
	}

	if (opened) Close();
	return(total);
}


/***********************************************************************************************
 * RawFileClass::Seek -- Repositions the file pointer.                                         *
 *=============================================================================================*/
long RawFileClass::Seek(long pos, int dir)
{
	if (Is_Open()) {
		long result = lseek(Handle, pos, dir);
		if (result == -1) {
			Error(errno, false, Filename);
			return(0);
		}
		return(result);
	}
	return(0);
}


/***********************************************************************************************
 * RawFileClass::Size -- Returns file size in bytes.                                           *
 *=============================================================================================*/
long RawFileClass::Size(void)
{
	int opened = false;

	if (!Is_Open()) {
		if (!Open(READ)) return(0);
		opened = true;
	}

	long oldpos = Seek(0, SEEK_CUR);
	long size = Seek(0, SEEK_END);
	Seek(oldpos, SEEK_SET);

	if (opened) Close();
	return(size);
}
