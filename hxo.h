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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>

//#include "loader.h"
#include "ini.h"
#include <libgen.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>

// Defined constants
#define MAX_LIBS 100
#define FILE_EXT ".hxo"

#define __ANDROID__

#ifdef __ANDROID__
    //In case of android
    #define EXTRA_UTILS           //Includes extra codes to be used in this project
    #define _DEBUG_LOG            //Comment if you don't wanna store logs in a file
    #define DEFAULT_ANDROID_APPPATH "/storage/emulated/0/Android/media/"        //Parent directory of <id>/scripts
    #define _LOG_DIR DEFAULT_ANDROID_APPPATH                           //Where hxo_log.txt file will be stored
    #define DEFAULT_INI_DIR "/storage/emulated/0/hxo/"                //where HXO.ini will be stored
    #define DEFAULT_LIB_DIR "/storage/emulated/0/hxo/"                //Where hxo_loader.so is placed
    
    int __attribute__((visibility("hidden"))) getAppID(char *_ID);    //Fetches app id eg:<com.example.app>
    int __attribute__((visibility("hidden"))) LogOutput();            //Starts the logging
  
    struct AndroidParam
    {
        char ID[512];                   //App ID
        char AndroidDataPath[1024];     //equvalent to /sdcard/data/<id>/
        char rootDataPath[1024];        //equvalent to /data/data/<id>/
    };
#endif //__ANDROID__

#define DEFAULT_HXO_DIR "./scripts/"
#define CONFIGFILE "HXO.ini"

#ifdef EXTRA_UTILS
int __attribute__((visibility("hidden"))) fileExists(const char *filepath);
int __attribute__((visibility("hidden"))) dirExists(const char *path);
int __attribute__((visibility("hidden"))) CopyFile(char *source_file, char *destination_file);
#endif //EXTRA_UTILS

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
    int out_fd = LogOutput();
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

    //in case of android
    struct AndroidParam *androidParam = malloc(sizeof(struct AndroidParam));

    //Default parameters
    if(getAppID(androidParam->ID))
    {
        fprintf(stderr,"Unable to fetch ID...");
    #ifdef _DEBUG_LOG
        fflush(stdout);
        fflush(stderr);
        close(out_fd);
    #endif
    #ifdef __ANDROID__
        free(androidParam);
    #endif
        free(entParam);
        free(confparam);
        return (void*)1;
    }
    printf("[+] Got ID: %s\n", androidParam->ID);

    //Setup additional android parameters
    dircat(androidParam->rootDataPath, "/data/data/", androidParam->ID);
    dircat(androidParam->AndroidDataPath, DEFAULT_ANDROID_APPPATH, androidParam->ID);
    printf("\nandroidParam->rootDataPath: %s\nandroidParam->AndroidDataPath: %s\n", androidParam->rootDataPath, androidParam->AndroidDataPath);
    //Setting up parameters for loading HXO.ini
    strcpy(entParam->ini_dir, androidParam->AndroidDataPath);
    fixDIR(entParam->ini_dir);
    dircat(entParam->iniFile, entParam->ini_dir, CONFIGFILE);
    
    
    if(!dirExists(androidParam->AndroidDataPath))
    {
        if (mkdir(androidParam->AndroidDataPath, 0770) == -1)
        {
            if(errno != EEXIST)
            {
                fprintf(stderr, "[X] Can't create directory: %s\n", androidParam->AndroidDataPath);
                // free allocated memory           
                free(confparam);
                free(entParam);
                free(androidParam);
            // exit
            #ifdef _DEBUG_LOG
                fflush(stdout);
                fflush(stderr);
                if(!out_fd) {
                    close(out_fd);
                }
            #endif
                return (void*)1;
            }
        } else {
            if(!out_fd)
            {
                out_fd = LogOutput();
                fflush(stdout);
                fflush(stderr);
            }
            printf("[!+] Directory not found, created one: %s\n", androidParam->AndroidDataPath);
        }
    }


    //Parse ini file
    if(ini_parse(entParam->iniFile, fn_ini_handler, confparam) < 0)
    {
        perror("[!] WARNING: unable to parse \'HXO.ini\'");
    }
  after_parsing:

    //exit without ding anythig if config says to
    if(!confparam->Enable)
    {
        free(entParam);
        free(confparam);
      #ifdef __ANDROID__
        free(androidParam);
      #endif
      #ifdef _DEBUG_LOG
        fflush(stdout);
        fflush(stderr);
        close(out_fd);
      #endif
        return (void*)1;
    }
    //setup parameters
    //HXO Priority of searching for 
    dircat(entParam->hxo_dir, androidParam->AndroidDataPath, confparam->hxo_dir);
    char new_hxo_dir[512];
    dircat(new_hxo_dir, androidParam->rootDataPath, "cache/hxo/");
    //Add a slash to avoid directory issues
    fixDIR(entParam->hxo_dir);
    fixDIR(new_hxo_dir);
    

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
    #ifdef __ANDROID__
        free(androidParam);
    #endif
    #ifdef _DEBUG_LOG
        fflush(stdout);
        fflush(stderr);
        close(out_fd);
    #endif
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
  #ifdef __ANDROID__
    //copy files to the tmp folder
    //folder: /data/data/<APP_ID>/tmp/hxo/
    if (mkdir(new_hxo_dir, 0777) == -1)
    {
        if(errno != EEXIST)
        {
            fprintf(stderr, "[X] Can't create directory: %s", new_hxo_dir);
            // free allocated memory
            for (int i = 0; i < count; i++)
            {
                free(files[i]);
            }            
            free(confparam);
            free(entParam);
            free(androidParam);
        // exit
        #ifdef _DEBUG_LOG
            fflush(stdout);
            fflush(stderr);
            close(out_fd);
        #endif
            return (void*)1;
        }
    }

    char new_filename[2048];
    for (int i = 0; i < count; i++)
    {
        dircat(current_filename, entParam->hxo_dir, files[i]);
        dircat(new_filename, new_hxo_dir, files[i]);
        if(CopyFile(current_filename, new_filename))
        {
            // free allocated memory
            for (int i = 0; i < count; i++)
            {
                free(files[i]);
            }            
            free(confparam);
            free(entParam);
            free(androidParam);
        // exit
        #ifdef _DEBUG_LOG
            fflush(stdout);
            fflush(stderr);
            close(out_fd);
        #endif
            return (void*)1;
        }
    }
    strcpy(entParam->hxo_dir, new_hxo_dir);
  #endif //__ANDROID__


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
    for (int i = 0; i < count; i++)
    {
        free(files[i]);
    }
    free(confparam);
    free(entParam);
      #ifdef __ANDROID__
        free(androidParam);
      #endif
    // exit
    //free buffers and handles
      #ifdef _DEBUG_LOG
        fflush(stdout);
        fflush(stderr);
        close(out_fd);
      #endif
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

    char outLogFile[1024];
    strcpy(outLogFile, _LOG_DIR);
    strcat(outLogFile, _debugAppID);
    fixDIR(outLogFile);

    strcat(outLogFile, "hxo_log.txt");
    
    /* Method 1 : Failed at test
    if ( !freopen(outLogFile, "a", stdout) &&
        !freopen(outLogFile, "a", stderr) )
    {
        return 0;
    }
    */

    //Method 2: ????
    int out_fd = open(outLogFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (out_fd == -1) {
        perror("Failed to open file");
        return 0;
    }

    // Duplicate the file descriptor to stdout and stderr
    if (dup2(out_fd, STDOUT_FILENO) == -1) {
        perror("Failed to redirect stdout");
        return 0;
    }

    if (dup2(out_fd, STDERR_FILENO) == -1) {
        perror("Failed to redirect stderr");
        return 0;
    }

    printf("\n\n\n---------->START LOG<----------\n\n");
    // Close the original file descriptors
    //close(out_fd);
    
    return out_fd;
}
#endif

// #include <stdio.h>
// #include <sys/stat.h>
// #include <sys/types.h>
#ifdef EXTRA_UTILS
int __attribute__((visibility("hidden"))) dirExists(const char *path) {
    struct stat info;

    // Use stat to get information about the path
    if (stat(path, &info) != 0) {
        // Error in accessing the path (e.g., it doesn't exist)
        return 0;
    } else if (info.st_mode & S_IFDIR) {
        // S_IFDIR bit is set, meaning it's a directory
        return 1;
    } else {
        // The path exists, but it's not a directory
        return 0;
    }
}
int __attribute__((visibility("hidden"))) fileExists(const char *filepath) {

    // Use fopen to create a handle at path
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        // Error in accessing the file (e.g., it doesn't exist)
        return 0;
    } else {
        // The path exists, but it's not a directory
        fclose(fp);
        return 1;
    }
}

int __attribute__((visibility("hidden"))) CopyFile(char *source_file, char *destination_file) {
    FILE *source = fopen(source_file, "rb");
    if (source == NULL) {
        fprintf(stderr, "[!] CopyFile failed: Could not open source file '%s'\n", source_file);
        return 1;
    }

    FILE *destination = fopen(destination_file, "wb");
    if (destination == NULL) {
        fprintf(stderr, "[!] CopyFile failed: Could not open destination file '%s'\n", destination_file);
        fclose(source);
        return 1;
    }

    char buffer[1024];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        if (fwrite(buffer, 1, bytes_read, destination) != bytes_read) {
            fprintf(stderr, "[!] CopyFile failed: Error writing to destination file '%s'\n", destination_file);
            fclose(source);
            fclose(destination);
            return 1;
        }
    }

    if (ferror(source)) {
        fprintf(stderr, "[!] CopyFile failed: Error reading from source file '%s'\n", source_file);
        fclose(source);
        fclose(destination);
        return 1;
    }

    fclose(source);
    fclose(destination);

    return 0;
}

#endif //EXTRA_UTILS