/*
Program to emulate ls command using file descriptor 
rather than C standard FILE* to be more UNIX-like

This program assumes directory not larger than 1 block,
this program seems to be way more complicated than it needs
to be. I used this page as a reference: http://man7.org/linux/man-pages/man2/getdents.2.html

developed by John Gaboriault-Whitcomb
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <dirent.h> // included to use file type constants

#define BLOCK_SIZE 4096 //assuming ext4 system has 4 kb block size
// some size helpers for things that appear multiple times
#define ULONG_SIZE sizeof(unsigned long)
#define USHORT_SIZE sizeof(unsigned short)
#define D_ULONG_SIZE ULONG_SIZE*2 // double the size of an unsigned long
#define ADD_DULONG_USHORT D_ULONG_SIZE + USHORT_SIZE // double ulong size and add ushort size  

typedef struct
{
    unsigned long inode_num;
    unsigned long offset;
    unsigned short record_length;
    char* file_name;
    char pad; // just a zero padding byte
    char file_type; 
} DirEntry;

typedef struct 
{
    int more_info, normal, help;
} Args;

Args* parseArgs(int argc, char** argv);
void cleanup(Args* args, int file_descriptor);
void fatal(char* message);
char* lookupFiletype(char type);
void help();
//int read_dir_entry(dir_entry* d, uint8_t* bytes);

int main(int argc, char** argv)
{
    Args* args = parseArgs(argc, argv);
    //printf("args: help: %d, more_info: %d, normal: %d\n", args->help, args->more_info, args->normal);
    if(args == NULL)
    {
        fatal("could not parse command line parameters. Use -h option for help.");
    }
    if(args->help)
    {
        //printf("help value: %d", args->help);
        help();
        exit(EXIT_SUCCESS);
    }
    int f; // file descriptor
    f = open(".", O_RDONLY | __O_DIRECTORY);
    if(f == -1)
    {
        free(args);
        fatal("could not open current directory");
    } 

    int numread;

    //char* bl = (char*)calloc(4096, sizeof(char));
    uint8_t full_block[4096];
    memset(full_block, 0, 4096);
    numread = syscall(SYS_getdents, f, full_block, 4096);
    if(numread < 0)
    {
        cleanup(args, f);
        fatal("could not read directory data");
    }
    if(args->more_info)
        printf("%8s         %8s         %8s         %8s         %8s\n", "inode number", "entry number", "record length", "file type", "file name");

    int index = 0;
    DirEntry dir_ent;
    //parse directory data until number of bytes actually read is reached
    while (index < numread)
    {
        memcpy(&dir_ent.inode_num, full_block + index, ULONG_SIZE);
        memcpy(&dir_ent.offset, full_block + ULONG_SIZE + index, ULONG_SIZE);
        memcpy(&dir_ent.record_length, full_block + D_ULONG_SIZE + index, USHORT_SIZE);

        int name_length = dir_ent.record_length - 2 - D_ULONG_SIZE - USHORT_SIZE;
        
        dir_ent.file_name = (uint8_t*)calloc(name_length, sizeof(uint8_t));

        memcpy(dir_ent.file_name, full_block + ADD_DULONG_USHORT + index, name_length);
        memcpy(&dir_ent.file_type, full_block + index + dir_ent.record_length - 1, sizeof(char));

        index += dir_ent.record_length;

        char* filetype = lookupFiletype(dir_ent.file_type);
        if(filetype == NULL)
        {
           // printf("filetype: %d", dir_ent.file_type);
            cleanup(args, f);
            fatal("Encountered unknown filetype, exiting program");
        }

        if(args->normal)
        {
            printf("%s\n", dir_ent.file_name);
        }
        else if(args->more_info)
        {
            printf("%8ld\t%8ld\t%8d\t%8s\t%8s\n", dir_ent.inode_num, dir_ent.offset, 
                                    dir_ent.record_length, filetype, dir_ent.file_name);
        }
        free(dir_ent.file_name);
    }
    cleanup(args, f);


    int f2;

    f2 = open("/home/crisc/c_files/test_block.bin", O_WRONLY | O_CREAT);
    write(f2, full_block, 4096);
    exit(EXIT_SUCCESS);
}

Args* parseArgs(int argc, char** argv)
{
    Args* args = (Args*)malloc(sizeof(Args));
    if(argc == 1)
    {
        args->help = 0;
        args->more_info = 0;
        args->normal = 1;
        return args;
    }
    if(argc == 2)
    {
        if(strcmp(argv[1], "-l") == 0)
        {
            args->more_info = 1;
            args->help = 0;
            args->normal = 0;
            return args;
        }
        else if(strcmp(argv[1], "-h") == 0)
        {
            args->more_info = 0;
            args->help = 1;
            args->normal = 0;
            return args;
        }
        else
        {
            free(args);
            return NULL;
        }
    }
    else
    {
        free(args);
        return NULL;
    }
    
}

void help()
{
    printf("Usage: \"program_name [-h][-l]\". Run this program with \n \
            either 1 argument or 0. Running without arguments results\n \
            in all filenames being sent to stdout. Running with -h prints \n \
            help. Running with -l will print more information from the current directory\n \
            similar to \"ls -l\". \n");
}

char* lookupFiletype(char type)
{
    char* retval;
    if(type == DT_UNKNOWN)
        return "Unknown";
    else if(type == DT_REG)
        return "Regular";
    else if(type == DT_DIR)
        return "Directory";
    else if(type == DT_CHR)
        return "Character Device";
    else if(type == DT_BLK)
        return "Block Device";
    else if(type == DT_FIFO)
        return "Buffer";
    else if(type == DT_SOCK)
        return "Socket";
    else if(type == DT_LNK)
        return "Symbolic Link";
    else
        return NULL;   
}

void fatal(char* message)
{
    printf("%s\n", message);
    exit(EXIT_FAILURE);
}

void cleanup(Args* args, int file_descriptor)
{
    free(args);
    close(file_descriptor);
}