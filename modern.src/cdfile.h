/*
 * Modernized CDFileClass — searches configured data directories for files.
 * Replaces the original CD-ROM drive scanning with directory-based search.
 */
#ifndef CDFILE_H
#define CDFILE_H

#include "rawfile.h"
#include <string.h>

class CDFileClass : public RawFileClass
{
	public:
		CDFileClass(char const *filename) : RawFileClass(filename) {};
		CDFileClass(void) : RawFileClass() {};
		virtual ~CDFileClass(void) {};

		virtual int Is_Available(int forced=false);
		virtual int Open(int rights=READ);
		virtual int Open(char const *filename, int rights=READ) {
			Set_Name(filename); return Open(rights);
		}

		void Searching(int on) { (void)on; }

		static bool Is_There_Search_Drives(void) { return SearchPath[0][0] != '\0'; }
		static int  Get_Search_Path_Count(void) { return SearchPathCount; }
		static const char* Get_Search_Path(int i) { return (i >= 0 && i < SearchPathCount) ? SearchPath[i] : nullptr; }
		static int Set_Search_Drives(char * pathlist);
		static void Add_Search_Drive(char *path);
		static void Clear_Search_Drives(void);
		static void Refresh_Search_Drives(void) {}
		static void Set_CD_Drive(int drive) { (void)drive; }
		static int  Get_CD_Drive(void) { return 0; }
		static int  Get_Last_CD_Drive(void) { return 0; }

	private:
		static const int MAX_SEARCH_PATHS = 16;
		static const int MAX_PATH_LEN = 512;
		static char SearchPath[MAX_SEARCH_PATHS][MAX_PATH_LEN];
		static int SearchPathCount;
};

#endif
