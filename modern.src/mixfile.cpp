/*
**	Command & Conquer(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* $Header:   F:\projects\c&c\vcs\code\mixfile.cpv   2.18   16 Oct 1995 16:48:46   JOE_BOSTIC  $ */
/***********************************************************************************************
 ***             C O N F I D E N T I A L  ---  W E S T W O O D   S T U D I O S               *** 
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : MIXFILE.CPP                                                  *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : August 8, 1994                                               *
 *                                                                                             *
 *                  Last Update : January 23, 1995 [JLB]                                       *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   MixFileClass::Cache -- Caches the named mixfile into RAM.                                 *
 *   MixFileClass::Cache -- Loads this particular mixfile's data into RAM.                     *
 *   MixFileClass::Finder -- Finds the mixfile object that matches the name specified.         *
 *   MixFileClass::Free -- Frees the allocated raw data block (not the index block).           *
 *   MixFileClass::MixFileClass -- Constructor for mixfile object.                             *
 *   MixFileClass::Offset -- Determines if the file is in a mixfile and where its sublocation is.*
 *   MixFileClass::Offset -- Searches in mixfile for matching file and returns offset if found.*
 *   MixFileClass::Retrieve -- Retrieves a pointer to the specified data file.                 *
 *   MixFileClass::~MixFileClass -- Destructor for the mixfile object.                         *
 *   MixFileClass::Offset -- Determines the offset of the requested file from the mixfile system.*
 *   MixFileClass::Free -- Uncaches a cached mixfile.                                          * 
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


#include "function.h"
#include	<fcntl.h>
#include	"io.h"
#include	<errno.h>
#include	"share.h"
#include "mixfile.h"


template<class T> int Compare(T const *obj1, T const *obj2) {
	if (*obj1 < *obj2) return(-1);
	if (*obj1 > *obj2) return(1);
	return(0);
};


/*
**	This is the pointer to the first mixfile in the list of mixfiles registered
**	with the mixfile system.
*/
MixFileClass * MixFileClass::First = 0;


/*********************************************************************************************** 
 * MixFileClass::Free -- Uncaches a cached mixfile.                                            * 
 *                                                                                             * 
 *    Use this routine to uncache a mixfile that has been cached.                              * 
 *                                                                                             * 
 * INPUT:   filename -- Pointer to the filename of the mixfile that is to be uncached.         * 
 *                                                                                             * 
 * OUTPUT:  bool; Was the mixfile found and freed?                                             * 
 *                                                                                             * 
 * WARNINGS:   none                                                                            * 
 *                                                                                             * 
 * HISTORY:                                                                                    * 
 *   01/23/1995 JLB : Created.                                                                 * 
 *=============================================================================================*/
bool MixFileClass::Free(char const *filename)
{
	MixFileClass * ptr = Finder(filename);

	if (ptr) {
		ptr->Free();
		return(true);
	}
	return(false);
}


#ifndef NOMEMCHECK
void MixFileClass::Free_All(void)
{
	while (First) {
		delete First;
	}
}
#endif


/***********************************************************************************************
 * MixFileClass::~MixFileClass -- Destructor for the mixfile object.                           *
 *                                                                                             *
 *    This destructor will free all memory allocated by this mixfile and will remove it from   *
 *    the system. A mixfile removed in this fashion must be created anew in order to be        *
 *    subsequent used.                                                                         *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/08/1994 JLB : Created.                                                                 *
 *   01/06/1995 JLB : Puts mixfile header table into EMS.                                      * 
 *=============================================================================================*/
MixFileClass::~MixFileClass(void)
{

	/*
	**	Deallocate any allocated memory.
	*/
	if (Filename) {
		free((char *)Filename);
	}
	if (Data) {
		delete [] (char*)Data;
	}
	if (Buffer) {
		delete [] Buffer;
	}

	/*
	**	Unlink this mixfile object from the chain.
	*/
	if (this == First) {
		First = (MixFileClass *)Get_Next();
	} else {
		Remove();
	}
	Zap();
}


/***********************************************************************************************
 * MixFileClass::MixFileClass -- Constructor for mixfile object.                               *
 *                                                                                             *
 *    This is the constructor for the mixfile object. It takes a filename and a memory         *
 *    handler object and registers the mixfile object with the system. The index block is      *
 *    allocated and loaded from disk by this routine.                                          *
 *                                                                                             *
 * INPUT:   filename -- Pointer to the filename of the mixfile object.                         *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/08/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
MixFileClass::MixFileClass(char const *filename)
{
	CCFileClass file;		// Working file object.

	/*
	**	Load in the control block. It always remains resident.
	*/
	Data = 0;
	Count = 0;
	Buffer = 0;
	file.Set_Name(filename);
	Filename = strdup(file.File_Name());

	if (!Force_CD_Available(RequiredCD)) {
		Prog_End();
		exit(EXIT_FAILURE);
	}

	if (file.Is_Available(true)) {
		FileHeader fileheader;

		file.Open();
		file.Read(&fileheader, sizeof(fileheader));
		Count = fileheader.count;
		DataSize = fileheader.size;
		fprintf(stderr, "MIX: %s — count=%d, size=%ld [%s]\n", filename, Count, DataSize, file.File_Name());

		/*
		**	Load up the offset control array. This could be located in
		**	EMS if possible.
		*/
		Buffer = new SubBlock [Count];
		if (Buffer) {
			file.Read(Buffer, Count * sizeof(SubBlock));
		}
		file.Close();
	} else {
//		delete this;
		return;
	}

	/*
	**	Raw data block starts uncached.
	*/
	Data = 0;

	/*
	**	Attach to list of mixfiles.
	*/
	Zap();
	if (!First) {
		First = this;
	} else {
		Add_Tail(*First);
	}
}


/***********************************************************************************************
 * MixFileClass::Retrieve -- Retrieves a pointer to the specified data file.                   *
 *                                                                                             *
 *    This routine will return with a pointer to the specified data file if the file resides   *
 *    in memory. Otherwise, this routine returns NULL. Use this routine to access a resident   *
 *    file directly rather than going through the process of pseudo disk access. This will     *
 *    save both time and RAM.                                                                  *
 *                                                                                             *
 * INPUT:   filename -- Pointer to the filename of the data file to retrieve a pointer to.     *
 *                                                                                             *
 * OUTPUT:  Returns with a pointer to the data file's data. If the file is not in RAM, then    *
 *          NULL is returned.                                                                  *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/23/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
void const * MixFileClass::Retrieve(char const *filename) {
	void *ptr = 0;
	Offset(filename, &ptr);
	return(ptr);
};


/***********************************************************************************************
 * MixFileClass::Finder -- Finds the mixfile object that matches the name specified.           *
 *                                                                                             *
 *    This routine will scan through all registered mixfiles and return with a pointer to      *
 *    the matching mixfile. If no mixfile could be found that matches the name specified,      *
 *    then NULL is returned.                                                                   *
 *                                                                                             *
 * INPUT:   filename -- Pointer to the filename to search for.                                 *
 *                                                                                             *
 * OUTPUT:  Returns with a pointer to the matching mixfile -- if found.                        *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/08/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
MixFileClass * MixFileClass::Finder(char const *filename)
{
	MixFileClass * ptr;

	ptr = First;
	while (ptr) {
		int flen = strlen(ptr->Filename);
		int qlen = strlen(filename);
		if (qlen <= flen && stricmp(&ptr->Filename[flen - qlen], filename) == 0) {
			return(ptr);
		}
		ptr = (MixFileClass *)ptr->Get_Next();
	}
	return(0);
}


/***********************************************************************************************
 * MixFileClass::Cache -- Caches the named mixfile into RAM.                                   *
 *                                                                                             *
 *    This routine will cache the mixfile, specified by name, into RAM.                        *
 *                                                                                             *
 * INPUT:   filename -- The name of the mixfile that should be cached.                         *
 *                                                                                             *
 * OUTPUT:  bool; Was the cache successful?                                                    *
 *                                                                                             *
 * WARNINGS:   This routine could go to disk for a very long time.                             *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/08/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
bool MixFileClass::Cache(char const *filename)
{
	MixFileClass * mixer = Finder(filename);

	if (mixer) {
		return(mixer->Cache());
	}
	return(false);
}


/***********************************************************************************************
 * MixFileClass::Cache -- Loads this particular mixfile's data into RAM.                       *
 *                                                                                             *
 *    This load the mixfile data into ram for this mixfile object. This is the counterpart     *
 *    to the Free() function.                                                                  *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  bool; Was the file load successful?  It could fail if there wasn't enough room     *
 *                to allocate the raw data block.                                              *
 *                                                                                             *
 * WARNINGS:   This routine goes to disk for a potentially very long time.                     *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/08/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
bool MixFileClass::Cache(void)
{
	if (Data) return(true);

	Data = new char [DataSize];
	if (Data) {
		CCFileClass file(Filename);

		file.Open();
		file.Seek(sizeof(SubBlock) * Count + sizeof(FileHeader));
		long actual = file.Read(Data, DataSize);
		if (actual != DataSize) {
#ifdef GERMAN
			Fatal("Korrupte .MIX-Datei \"%s\". Beim Versuch, %ld zu lesen, nur %ld gefunden.", Filename, DataSize, actual);
#else
#ifdef FRENCH
			Fatal("Fichier .MIX corrumpu \"%s\". Essai de lecture de %ld, mais %ld obtenu.", Filename, DataSize, actual);
#else
			Fatal("Corrupt .MIX file \"%s\". Tried to read %ld, but got %ld.", Filename, DataSize, actual);
#endif
#endif
		}
		file.Close();
		return(true);
	}
#ifdef GERMAN
	Fatal("Kann Datei \"%s\" nicht laden.", Filename);
#else
#ifdef FRENCH
	Fatal("Impossible de charger \"%s\".", Filename);
#else
	Fatal("Unable to load \"%s\".", Filename);
#endif
#endif
	return(false);
}


/***********************************************************************************************
 * MixFileClass::Free -- Frees the allocated raw data block (not the index block).             *
 *                                                                                             *
 *    This routine will free the (presumably large) raw data block, but leave the index        *
 *    block intact. By using this in conjunction with the Cache() function, one can maintain   *
 *    tight control of memory usage. If the index block is desired to be freed, then the       *
 *    mixfile object must be deleted.                                                          *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/08/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
void MixFileClass::Free(void)
{
	if (Data) {
		delete [] (char*)Data;
		Data = 0;
	}
}


/***********************************************************************************************
 * MixFileClass::Offset -- Determines if the file is in a mixfile and where its sublocation is.*
 *                                                                                             *
 *    This routine will scan through all registers mixfiles in an attempt to locate the file   *
 *    in question. Whether the file is located in RAM or on disk does not restrict this        *
 *    search operation.                                                                        *
 *                                                                                             *
 * INPUT:   filename    -- The filename of the file to search within the mixfile system.       *
 *                                                                                             *
 *          realptr     -- Pointer to pointer that will be filled with the RAM pointer to      *
 *                         the file if it happens to be in RAM. NULL if otherwise.             *
 *                                                                                             *
 *          mixfile     -- Pointer to the mixfile object that contains the file in question.   *
 *                                                                                             *
 *          offset      -- The offset to the start of the data of the file. If the file is     *
 *                         on disk then this is the offset from the start of the file, if      *
 *                         it is located in RAM, then it is the offset from the start of the   *
 *                         raw data block.                                                     *
 *                                                                                             *
 *          size        -- Pointer to where the size of the file will be stored.               *
 *                                                                                             *
 * OUTPUT:  bool; Was the file found in one of the mixfiles?  The value stored in "realptr"    *
 *                will be NULL if it is on disk, otherwise it is in RAM.                       *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   08/08/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
//int _USERENTRY Compare(MixFileClass::SubBlock const *, MixFileClass::SubBlock const *);

// int _USERENTRY compfunc(void const *ptr1, void const *ptr2) 
int compfunc(void const *ptr1, void const *ptr2) 
{
//	long diff = *(long const *)ptr1 - *(long const *)ptr2;
//	return FP_SEG(diff);

	if (*(long const *)ptr1 < *(long const *)ptr2) return(-1);
	if (*(long const *)ptr1 > *(long const *)ptr2) return(1);
	return(0);
}


/***********************************************************************************************
 * MixFileClass::Offset -- Determines the offset of the requested file from the mixfile system.*
 *                                                                                             *
 *    This routine will take the falename specified and search through the mixfile system      *
 *    looking for it. If the file was found, then the mixfile it was found int, the offset     *
 *    from the start of the mixfile, and the size of the embedded file will be returned.       *
 *    Using this method it is possible for the CCFileClass system to process it as a normal    *
 *    file.                                                                                    *
 *                                                                                             *
 * INPUT:   filename    -- The filename to search for.                                         *
 *                                                                                             *
 *          realptr     -- Stores a pointer to the start of the file in memory here. If the    *
 *                         file is not in memory, then NULL is stored here.                    *
 *                                                                                             *
 *          mixfile     -- The pointer to the corresponding mixfile is placed here. If no      *
 *                         mixfile was found that contains the file, then NULL is stored here. *
 *                                                                                             *
 *          offset      -- The starting offset from the beginning of the parent mixfile is     *
 *                         stored here.                                                        *
 *                                                                                             *
 *          size        -- The size of the embedded file is stored here.                       *
 *                                                                                             *
 * OUTPUT:  bool; Was the file found? The file may or may not be resident, but it does exist   *
 *                and can be opened.                                                           *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   10/17/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
bool MixFileClass::Offset(char const *filename, void ** realptr, MixFileClass ** mixfile, long * offset, long * size)
{
	MixFileClass * ptr;

	if (!filename) return(false);

	/*
	**	Create the key block that will be used to binary search for the file.
	*/
	/* Copy filename to writable buffer before strupr (original may be a string literal) */
	char fname_buf[256];
	strncpy(fname_buf, filename, sizeof(fname_buf)-1);
	fname_buf[sizeof(fname_buf)-1] = '\0';
	strupr(fname_buf);
	int slen = strlen(fname_buf);
	long crc = Calculate_CRC(fname_buf, slen);

	/* Old C&C95 MIX files use a simpler byte-by-byte CRC */
	unsigned int old_crc = 0;
	for (int i = 0; i < slen; i++) {
		old_crc = ((old_crc << 1) | (old_crc >> 31)) + (unsigned char)fname_buf[i];
	}

	SubBlock key;
	key.CRC = crc;

	/*
	**	Sweep through all registered mixfiles, trying to find the file in question.
	*/
	ptr = First;
	while (ptr) {
		SubBlock * block;

		if (!ptr->Buffer || ptr->Count <= 0 || ptr->Count > 10000) {
			ptr = (MixFileClass *)ptr->Get_Next();
			continue;
		}

		/* Linear search — try both remastered (4-byte chunk) and old (byte-by-byte) CRCs */
		block = nullptr;
		for (int i = 0; i < ptr->Count; i++) {
			if (ptr->Buffer[i].CRC == key.CRC || (unsigned int)ptr->Buffer[i].CRC == old_crc) {
				block = &ptr->Buffer[i];
				break;
			}
		}
		if (block) {
			if (mixfile) *mixfile = ptr;
			if (size) *size = block->Size;
			if (realptr) *realptr = 0;
			if (offset) *offset = block->Offset;
			if (realptr && ptr->Data) {
				*realptr = Add_Long_To_Pointer(ptr->Data, block->Offset);
			}
			if (!ptr->Data && offset) {
				*offset += sizeof(SubBlock) * ptr->Count + sizeof(FileHeader);
			}
			return(true);
		}

		/*
		**	Advance to next mixfile.
		*/
		ptr = (MixFileClass *)ptr->Get_Next();
	}

	/*
	**	All the mixfiles have been examined but no match was found. Return with the non success flag.
	*/
	return(false);
}

/* ---- Accessors for asset browser (tools) ---- */

MixFileClass* MixFileClass_First(void) {
	return MixFileClass::First;
}

MixFileClass* MixFileClass_Next(MixFileClass* ptr) {
	if (!ptr) return nullptr;
	return (MixFileClass*)ptr->Get_Next();
}

int MixFileClass_GetCount(MixFileClass* ptr) {
	return ptr ? ptr->Count : 0;
}

long MixFileClass_GetDataSize(MixFileClass* ptr) {
	return ptr ? ptr->DataSize : 0;
}

MixFileClass::SubBlock* MixFileClass_GetEntries(MixFileClass* ptr) {
	return ptr ? ptr->Buffer : nullptr;
}

void* MixFileClass_GetData(MixFileClass* ptr) {
	return ptr ? ptr->Data : nullptr;
}

