/*
 * cdfile.cpp — Directory-based file search for the modernized TD engine.
 * Replaces the original CD-ROM drive scanning.
 */
#include "function.h"
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

char CDFileClass::SearchPath[MAX_SEARCH_PATHS][MAX_PATH_LEN] = {};
int CDFileClass::SearchPathCount = 0;


int CDFileClass::Set_Search_Drives(char* pathlist) {
    Clear_Search_Drives();
    if (!pathlist) return 0;

    /* Parse semicolon or colon-separated paths (like PATH) */
    char buf[2048];
    strncpy(buf, pathlist, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';

    char* token = strtok(buf, ";");
    while (token && SearchPathCount < MAX_SEARCH_PATHS) {
        /* Skip whitespace */
        while (*token == ' ') token++;
        if (*token) {
            Add_Search_Drive(token);
        }
        token = strtok(nullptr, ";");
    }
    return 0;
}


void CDFileClass::Add_Search_Drive(char* path) {
    if (SearchPathCount >= MAX_SEARCH_PATHS || !path || !*path) return;

    strncpy(SearchPath[SearchPathCount], path, MAX_PATH_LEN-2);
    SearchPath[SearchPathCount][MAX_PATH_LEN-2] = '\0';

    /* Ensure trailing slash */
    int len = strlen(SearchPath[SearchPathCount]);
    if (len > 0 && SearchPath[SearchPathCount][len-1] != '/') {
        SearchPath[SearchPathCount][len] = '/';
        SearchPath[SearchPathCount][len+1] = '\0';
    }
    SearchPathCount++;
}


void CDFileClass::Clear_Search_Drives(void) {
    SearchPathCount = 0;
    memset(SearchPath, 0, sizeof(SearchPath));
}


int CDFileClass::Is_Available(int forced) {
    /* First try the filename as-is */
    if (RawFileClass::Is_Available(forced)) return true;

    /* Search each registered path */
    char const* name = File_Name();
    if (!name) return false;

    char fullpath[1024];
    for (int i = 0; i < SearchPathCount; i++) {
        snprintf(fullpath, sizeof(fullpath), "%s%s", SearchPath[i], name);
        struct stat st;
        if (stat(fullpath, &st) == 0) {
            return true;
        }
    }
    return false;
}


int CDFileClass::Open(int rights) {
    /* First try the filename as-is */
    if (RawFileClass::Open(rights)) return true;

    /* For read operations, search the registered paths */
    if (rights == READ) {
        char const* name = File_Name();
        if (!name) return false;

        char fullpath[1024];
        for (int i = 0; i < SearchPathCount; i++) {
            snprintf(fullpath, sizeof(fullpath), "%s%s", SearchPath[i], name);
            struct stat st;
            if (stat(fullpath, &st) == 0) {
                Set_Name(fullpath);
                return RawFileClass::Open(rights);
            }
        }
    }
    return false;
}
