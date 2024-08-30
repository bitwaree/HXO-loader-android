/*-----------(START OF LICENSE NOTICE)-----------*/
/*
 * This file is part of HXO-loader.
 *
 * HXO-loader is licensed under the GNU General Public License v3.0
 * (GPL-3.0). You may copy, modify, and distribute it under the terms of the
 * GPL-3.0. This software is distributed WITHOUT ANY WARRANTY; see the GPL-3.0
 * for more details. You should have received a copy of the GPL-3.0 along
 * with this software. If not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2024 bitware
*/
/*-----------(END OF LICENSE NOTICE)-----------*/

//dynamic shared object loader by bitware
//Read README.md for config and function api defination and potential tutorial
//       _
// |_|\// \ | _  _. _| _ ._
// | |/\\_/ |(_)(_|(_|(/_|
//
//patchelf --add-needed hxo_loader.so <executable_target.elf>

#ifndef VER_STR
  #define VER_STR "1.0"
#endif

#ifndef LIC_STR
  #define LIC_STR "HXO-loader Copyright (C) 2024 bitware.\n\n"
#endif

#ifndef BANNER_STR
  #define BANNER_STR "\n\n" \
                     "          _                   \n" \
                     "    |_|\\// \\ | _  _. _| _ ._\n" \
                     "    | |/\\\\_/ |(_)(_|(_|(/_| \n" \
                     "                      --v%s  \n"
#endif //BANNER_STR
#define CPRS_SHOW_ALWAYS

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

void __attribute__((visibility("hidden"))) *hxo_loader();
int __attribute__((visibility("hidden"))) GetExePath(char *directory);
void __attribute__((visibility("hidden"))) fixDIR(char *Dir);
void __attribute__((visibility("hidden"))) dircat(char *absolute, char *parent, char *child);

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>
//#include "loader.h"
#include "ini.h"
#include <libgen.h>

// Defined constants
#define MAX_LIBS 100
#define FILE_EXT ".hxo"

#define __ANDROID__

#ifdef __ANDROID__
  //In case of android
  #define _DEBUG_LOG

  int __attribute__((visibility("hidden"))) getAppID(char *_ID);
  int __attribute__((visibility("hidden"))) LogOutput();
  #define DEFAULT_LIB_DIR "/storage/emulated/0/hxo/"

#else
  //if not android
  #define DEFAULT_LIB_DIR "/usr/lib"

#endif //__ANDROID__
#define DEFAULT_HXO_DIR "./scripts/"
#define CONFIGFILE "HXO.ini"

struct iniParam
{
    bool Enable;          //Enables hxo_loader
    char hxo_dir[2048];   //Child directory name (where .hxo files stays)
    int sleep;            //Sleep timer in seconds
    bool autoUnload;      //Unloads hxo files after execution
    //extras
    char ep[256];         //Entrypoint name
    char dl_dir[2048];    //where hxo_loader stays
    //misc
    bool hideBanner;      //Hide banner text
    bool hideCPRstring;   //Hide Copyright String
};

struct internalParam
{
    char exedir[2048];    //(Absolute path) Where elf executable stays
    char cwd[2048];       //(Absolute path) Where elf was executed
    char ini_dir[2048];   //(Absolute path) Where INI file was loaded (parent directory for hxo_dir) (Absolute Path)
    char iniFile[2048];   //(Absolute path) Path of iniFile 
    char hxo_dir[2048];   //(Absolute path) Where .hxo files lives 
};
/*
;INI struct
[HXO]
hxo=1
hxo_dir=./scripts
sleep=0
UnloadAfterExecution=0

[1337]
lib=/usr/lib/hxo
EP=_init_hxo
*/

//hook snippet
/*
    //loader code start
    static int callnum = 0;
    if(!callnum)
    {
        callnum = ~callnum;
        pthread_attr_t ptattr;
        size_t _stacksize = 4 * 1024 * 1024; // 4MB stack size

        pthread_attr_init(&ptattr);
        pthread_attr_setstacksize(&ptattr, _stacksize);
        pthread_t loader_thread;
        pthread_create(&loader_thread, &ptattr, (void*(*)(void*))hxo_loader, 0);
    }
*/
int __attribute__((visibility("hidden"))) fn_ini_handler(void *user, const char *section, const char *name, const char *value)
{

    struct iniParam *cf = (struct iniParam*) user;
    if(!strcmp(section, "HXO"))
    {
        //HXO configs
        if(!strcmp(name, "hxo"))
            cf->Enable = atoi(value);
        if(!strcmp(name, "hxo_dir"))
            strcpy(cf->hxo_dir, value);
        if(!strcmp(name, "sleep"))
            cf->sleep = atoi(value);
        if(!strcmp(name, "UnloadAfterExecution"))
            cf->autoUnload = atoi(value);
    }
    else if(!strcmp(section, "1337"))
    {
        //extra hacks
        if(!strcmp(name, "EP"))
            strcpy(cf->ep, value);
        if(!strcmp(name, "lib"))
            strcpy(cf->dl_dir, value);
    }
    else if(!strcmp(section, "MISC"))
    {
        //misc
        if(!strcmp(name, "HideBanner"))
            cf->hideBanner = (bool) atoi(value);
        if(!strcmp(name, "HideCPRS"))
            cf->hideCPRstring = (bool) atoi(value);
    }
    else
    {
        return 0;
    }

    return 1;
}


void __attribute__((visibility("hidden"))) *hxo_loader()
{
  #ifdef _DEBUG_LOG
    LogOutput();
  #endif

  #ifdef CPRS_SHOW_ALWAYS
    fprintf(stdout, BANNER_STR, VER_STR);
    fprintf(stdout, LIC_STR);
  #endif
    //Read Config
    struct internalParam *entParam = malloc(sizeof(struct internalParam));
    
    struct iniParam *confparam = malloc(sizeof(struct iniParam));
    confparam->Enable = 1;
    confparam->sleep = 0;
    strcpy(confparam->dl_dir, DEFAULT_LIB_DIR);
    strcpy(confparam->hxo_dir, DEFAULT_HXO_DIR);
    strcpy(confparam->ep, "_init_hxo");
    confparam->autoUnload = 0;
    confparam->hideBanner = 0;
    confparam->hideCPRstring = 0;

  #ifndef __ANDROID__
    // fetch current working directory
    if (getcwd(entParam->cwd, 2048) == NULL) 
    {
        perror("[!] WARNING: Can't retrive current working directory!");
        *entParam->cwd = (unsigned char) 0;
    }
    //fetch exe path
    if(!GetExePath(entParam->exedir))
    {
        perror("[X] ERROR: Can't retrive current executable path! \n");
        *entParam->exedir = (unsigned char) 0;
    }

    //INI persing priority:
    //FIRST: look for current woking directory
    //SEC:   look for Executable path
    if(*entParam->cwd == 0 && *entParam->exedir==0)
    {
      _exit_at_init:
        //If nothing found... (atleast one is needed to continue)
        free(entParam);
        free(confparam);
        return (void*)1;
    }

    if(*entParam->cwd != 0)
    {
        dircat(entParam->iniFile, entParam->cwd, CONFIGFILE);
        if(!(ini_parse(entParam->iniFile, fn_ini_handler, confparam) < 0))
        {
            //Successfully parsed ini from current working directory
            strcpy(entParam->ini_dir, entParam->cwd);
            fixDIR(entParam->ini_dir);
            goto after_parsing;
        }
    }
    
    if (*entParam->exedir != 0) 
    {
        dircat(entParam->iniFile, entParam->exedir, CONFIGFILE);
        if(!(ini_parse(entParam->iniFile, fn_ini_handler, confparam) < 0))
        {
            //Successfully parsed ini from executable directory
            strcpy(entParam->ini_dir, entParam->exedir);
            fixDIR(entParam->ini_dir);
            goto after_parsing;
        }
        else
        {
            //Assume all parameter as per default values
            perror("[!] WARNING: unable to parse \'HXO.ini\'");
            strcpy(entParam->ini_dir, entParam->exedir);
            fixDIR(entParam->ini_dir);
            goto after_parsing;
        }
    }
    else
    {
        goto _exit_at_init;
    }
  #else
    //in case of android
    strcpy(entParam->ini_dir, DEFAULT_LIB_DIR);
    fixDIR(entParam->ini_dir);
    dircat(entParam->iniFile, entParam->ini_dir, CONFIGFILE);
    if(ini_parse(entParam->iniFile, fn_ini_handler, confparam) < 0)
    {
        perror("[!] WARNING: unable to parse \'HXO.ini\'");
    }
    if(!getAppID(confparam->hxo_dir))
    {
        return (void*)1;
    }
  #endif //__ANDROID__

  after_parsing:

    //exit without ding anythig if config says to
    if(!confparam->Enable)
    {
        free(entParam);
        free(confparam);
        return (void*)1;
    }
    //setup parameters
    dircat(entParam->hxo_dir, entParam->ini_dir, confparam->hxo_dir);
    //Add a slash to avoid directory issues
    fixDIR(entParam->hxo_dir);

  #ifndef CPRS_SHOW_ALWAYS
    if(!confparam->hideBanner)
        fprintf(stdout, BANNER_STR, VER_STR);
    if(!confparam->hideCPRstring)
        fprintf(stdout, LIC_STR);
  #endif //CPRS_SHOW_ALWAYS

    // search for lib in the directory as per config
    DIR *dir;
    struct dirent *entry;
    char *files[MAX_LIBS]; // Assuming a maximum number of files
    int count = 0;

    dir = opendir(entParam->hxo_dir);
    if (dir == NULL)
    {
        fprintf(stderr, "[X] ERROR: Can't open hxo directory \"%s\".\n", confparam->hxo_dir);
        free(entParam);
        free(confparam);
        return (void*)1;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_REG)
        { // Check if it is a regular file
            char *dot = strrchr(entry->d_name, '.');
            if (dot && strcmp(dot, FILE_EXT) == 0)
            {
                files[count] = strdup(entry->d_name);
                count++;
            }
        }
    }

    closedir(dir);


    if(confparam->sleep > 0)
    {
        //Sleep before loading
        sleep(confparam->sleep);
    }
    // load one by one and call their perticular entrypoint void* _init_hxo(void*)
    char current_filename[2048];
    void *dlhandle;
    void *(*init_func)(void *);

    for (int i = 0; i < count; i++)
    {
        dlhandle = NULL;
        // Load the shared object file
        dircat(current_filename, entParam->hxo_dir, files[i]);
        dlhandle = dlopen(current_filename, RTLD_LAZY);
        if (!dlhandle)
        {
            fprintf(stderr, "[!] Error while loading %s: %s\n", current_filename, dlerror());
            continue;
        }

        // Get a pointer to the _init_hxo function
        init_func = dlsym(dlhandle, confparam->ep);
        if (!init_func)
        {
            fprintf(stderr, "[!] Entrypoint not found in: %s\n", files[i]);
            dlclose(dlhandle);
            continue;
        }

        // Call the _init_hxo function
        void *result = init_func(NULL);
        //If a non-zero value returned...
        if((intptr_t)result != 0)
        {
            //Print about the error value
            fprintf(stderr, "[*] \"%s\" returned %ld (0x%lX).\n", files[i], (int64_t) result, (uintptr_t) result);
        }
        if((intptr_t)result == -1)
        {
            //Error in library!!!
            dlclose(dlhandle);
            continue;
        }
        if(confparam->autoUnload)
        {
            dlclose(dlhandle);
        }
    }

    
    // free allocated memory

    // strings
    for (int i = 0; i < count; i++)
    {
        free(files[i]);
    }
    free(confparam);
    free(entParam);
    // exit
    return (void*)0;
}

int __attribute__((visibility("hidden"))) GetExePath(char *directory)
{
    static const uint MAX_LENGTH = 1024;
    char *exepath = (char *)malloc(MAX_LENGTH);
    char *dir;
    ssize_t len = readlink("/proc/self/exe", exepath, MAX_LENGTH - 1);
    if (len != -1)
    {
        exepath[len] = '\0';
        // printf("exe path: %s\n", exepath);
        dir = dirname(exepath);
        // printf("Current directory: %s\n", dir);
        strcpy(directory, dir);
        size_t dirlen = strlen(dir);
        directory[dirlen] = '/';
        directory[dirlen + 1] = '\0';
        dirlen++;
    }
    else
    {
        perror("readlink() error: can't fetch exe path");
        free(exepath);
        return 0;
    }
    free(exepath);
    return 1;
}

void __attribute__((visibility("hidden"))) fixDIR(char *Dir)
{
    size_t tmp_length = strlen(Dir);
    if(Dir[tmp_length-1] != '/')
    {
        Dir[tmp_length] = '/';
        Dir[tmp_length+1] = '\0';
    }
}

void __attribute__((visibility("hidden"))) dircat(char *absolute, char *parent, char *child)
{
    //If child starts with a "/" take it as a absolute directory
    if(child[0] == '/') {
        //treat child as an absolute path
        strcpy(absolute, child);
    }
    else {
        //copy the parent directory first
        strcpy(absolute, parent);
        fixDIR(absolute);           //add a slash (/) is not already exists
        strcat(absolute, child);    //concat child
    }
}

#ifdef __ANDROID__
int __attribute__((visibility("hidden"))) getAppID(char *_ID)
{
    // Open the /proc/self/cmdline file
    FILE *f = fopen("/proc/self/cmdline", "r");
    if (f == NULL) {
        perror("Failed to open /proc/self/cmdline");
        return 1;
    }

    // Read the content of the file
    if (fgets(_ID, 512, f) != NULL) {
        // The package name is stored at the start of the file
    } else {
        perror("Failed to read package name");
        fclose(f);
        return 1;
    }

    // Close the file
    fclose(f);
    return 0;
}
int __attribute__((visibility("hidden"))) LogOutput()
{
    char _debugAppID[512];
    getAppID(_debugAppID);

    char LogFile[2048];
    strcpy(LogFile, "/data/data/");
    strcat(LogFile, _debugAppID);
    fixDIR(LogFile);
    strcat(LogFile, "hxo_log.txt");
    if ( !freopen(LogFile, "a", stderr) &&
        !freopen(LogFile, "a", stdout) )
    {
        return 1;
    }

    // Get the current time
    time_t current_time;
    time(&current_time);

    // Convert to local time format
    struct tm *local_time = localtime(&current_time);
    
    fprintf(stdout, "\n\n\n------->START LOG (%s)<----------\n\n", asctime(local_time));
    perror("lol lol");
    fclose(stderr);
    fclose(stdout);
    return 0;
}
#endif