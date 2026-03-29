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

/* $Header:   F:\projects\c&c\vcs\code\mixfile.h_v   2.18   16 Oct 1995 16:47:22   JOE_BOSTIC  $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               *** 
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : MIXFILE.H                                                    *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : October 18, 1994                                             *
 *                                                                                             *
 *                  Last Update : October 18, 1994   [JLB]                                     *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#ifndef MIXFILE_H
#define MIXFILE_H

#include "link.h"

class MixFileClass : public LinkClass 
{
	public:
		char const *Filename;			// Filename of mixfile.

		MixFileClass(char const *filename);
		~MixFileClass(void);

		static bool Free(char const *filename);
		static void Free_All(void);
		void Free(void);
		bool Cache(void);
		static bool Cache(char const *filename);
		static bool Offset(char const *filename, void ** realptr = 0, MixFileClass ** mixfile = 0, long * offset = 0, long * size = 0);
		static void const * Retrieve(char const *filename);

		/* MIX entry: 3x 4-byte ints = 12 bytes on disk */
		#pragma pack(push, 1)
		struct SubBlock {
			int CRC;				// CRC code for embedded file.
			int Offset;			// Offset from start of data section.
			int Size;				// Size of data subfile.

			int operator < (SubBlock & two) const {return (CRC < two.CRC);};
			int operator > (SubBlock & two) const {return (CRC > two.CRC);};
			int operator == (SubBlock & two) const {return (CRC == two.CRC);};
		};
		#pragma pack(pop)

		/* Non-member accessors for tool code */
		friend MixFileClass* MixFileClass_First(void);
		friend MixFileClass* MixFileClass_Next(MixFileClass*);
		friend int MixFileClass_GetCount(MixFileClass*);
		friend long MixFileClass_GetDataSize(MixFileClass*);
		friend SubBlock* MixFileClass_GetEntries(MixFileClass*);
		friend void* MixFileClass_GetData(MixFileClass*);

	private:
		static MixFileClass * Finder(char const *filename);
		long Offset(long crc, long *size = 0);

		/* MIX file header: 2 bytes count + 4 bytes size = 6 bytes
		   On 64-bit, 'long' is 8 bytes — must use int32_t for file format. */
		#pragma pack(push, 1)
		typedef struct {
			short	count;
			int	size;
		} FileHeader;
		#pragma pack(pop)

		int Count;							// Number of sub-blocks.
		long DataSize;						// Size of raw data.
		SubBlock * Buffer;				// Array of sub blocks (could be in EMS).
		void *Data;							// Pointer to raw data.

		static MixFileClass * First;
};

#endif
