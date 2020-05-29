//
//  main.c
//  File System Part 3
//
//  Created by Muhammed Okumuş on 26.05.2020.
//  Copyright 2020 Muhammed Okumuş. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>  
#include <string.h> 
#include <time.h>

#define ONE_MB 1000000
#define MAX_PATH 255
#define DIRECT_BLOCKS 12
#define errExit(msg) do{ perror(msg); print_usage(); exit(EXIT_FAILURE); } while(0)

#define CMD_LIST "list"
#define CMD_MKDIR "mkdir"
#define CMD_RMDIR "rmdir"
#define CMD_DUMPE2FS "dumpe2fs"
#define CMD_WRITE "write"
#define CMD_READ "read"
#define CMD_DEL "del"
#define CMD_LN "ln"
#define CMD_LNSYM "lnsym"
#define CMD_FSCK "fsck"
  
// File system data structures =============
enum BlockType {
    BLOCK_INODE,
    BLOCK_FILE_CONTENT,
    BLOCK_DIRECTORY_CONTENT,
    BLOCK_SUPER,
    BLOCK_INODE_MAP,
    BLOCK_FREE
};


struct Block {
    int address;    // Block index
    int type;       // BlockType enum
    char name[16];  // Block name
};

struct Superblock {
    int state;              // State of the system, 0 = No error
    int total_blocks;       // # of blocks
    int num_free_blocks;    // # of free blocks
    int first_block;        // Start index of  blocks
    int block_size;         // block size in kilobytes
    int inodes_per_block;   // # of inodes per block
    int total_inode_blocks; // # of inode blocks
    int num_free_inodes;    // # of inodes without a associeted file
    int first_inode;        // Start index of inodes(root inode)
    int inode_size;         // Size of inodes
    int superblock_size;    // Size of the super block
}sb;

struct Inode {
    int id;                                 // file/directory id
    int direct_blocks;                      // start address of direct blocks that holds file contents
    int num_of_blocks;                      // Total number of blocks that file occupies
    int indirect_blocks[DIRECT_BLOCKS*2];   // indirect block pointers
    int type;                               // 0 = directory, Non-zero positive = file, Negative = not used
    unsigned long size;                     // File size in bytes
    struct tm last_modified;                // Last modification time
};

struct Directory{
    char links[16][16];
    int n;
};


struct Inode** inodes;
struct Block *blocks;
unsigned char *bitmap;  //https://stackoverflow.com/questions/44978126/structure-for-an-array-of-bits-in-c


// ==========================================

  
// Function Prototypes ======================
// Display/Debug Functions
void print_usage();
void print_superblock();
void print_inode(struct Inode i);
void print_inode_n(int i);
void print_bitmap();
char* get_name(int address);
char* get_block_type(struct Block b);


// Command Processing Functions =============
int cmd_list(char* path);
int cmd_mkdir(char* path);
int cmd_rmdir(char* path);
int cmd_dumpe2fs();
int cmd_write(char* path, char* linuxFile);
int cmd_read(char* path, char* linuxFile);
int cmd_del(char* path);
int cmd_ln(char* path1, char* path2);
int cmd_lnsym(char* path1, char* path2);
int cmd_fsck();


// File system retrievel/booting
int read_filesystem();

// Bitmap access/modification
int read_bit(int n);
int set_bit(int b, int n);

// File pointer to be shared with functions
FILE *fp; 

// Driver program 
int main (int argc, char* argv[]) 
{ 
    char input_filepath[MAX_PATH];

    // Parse commands =============================================
    if(argv[2] == NULL) errExit("Missing arguments");
    snprintf(input_filepath, MAX_PATH,"%s",argv[1]); printf("File path : %s\n", input_filepath);

    fp = fopen (input_filepath, "r+"); 
    if (fp == NULL) errExit("Error opening file");
    read_filesystem();
    
    printf("Command   : %s\n",argv[2]);

    if (strncmp(argv[2],CMD_LIST,strlen(CMD_LIST)) == 0){
        //fileSystemOper fileSystem.data list “/”

    }
    else if (strncmp(argv[2],CMD_MKDIR,strlen(CMD_MKDIR))== 0){
        //fileSystemOper fileSystem.data mkdir “/usr/ysa”
    }
    else if (strncmp(argv[2],CMD_RMDIR,strlen(CMD_RMDIR))== 0){
        //fileSystemOper fileSystem.data rmdir “/usr/ysa”
    }
    else if (strncmp(argv[2],CMD_DUMPE2FS,strlen(CMD_DUMPE2FS))== 0){
        cmd_dumpe2fs();
    }
    else if (strncmp(argv[2],CMD_WRITE,strlen(CMD_WRITE))== 0){
        // fileSystemOper fileSystem.data write “/usr/ysa/file” linuxFile
    }
    else if (strncmp(argv[2],CMD_READ,strlen(CMD_READ))== 0){
        // fileSystemOper fileSystem.data read “/usr/ysa/file” linuxFile
    }
    else if (strncmp(argv[2],CMD_DEL,strlen(CMD_DEL))== 0){
        // fileSystemOper fileSystem.data del “/usr/ysa/file”
    }
    else if (strncmp(argv[2],CMD_LN,strlen(CMD_LN))== 0){
        // fileSystemOper fileSystem.data ln “/usr/ysa/file1” “/usr/ysa/file2”
    }
    else if (strncmp(argv[2],CMD_LNSYM,strlen(CMD_LNSYM))== 0){
        // fileSystemOper fileSystem.data lnsym “/usr/ysa/file1” “/usr/ysa/file2”
    }
    else if (strncmp(argv[2],CMD_FSCK,strlen(CMD_FSCK))== 0){
        cmd_fsck();
    }  


    // Free resources ====================================
    // close file 
    fclose (fp); 
    printf("==============================\n");
    printf("Freeing resources...\n");
    for(int i = 0; i < sb.total_inode_blocks; i++)
        free(inodes[i]);

    free(inodes);
    free(bitmap);
    free(blocks);

    printf("Done!\n");
    //===================================== Free resources 

    exit(EXIT_SUCCESS);
} 

void print_usage(){
    printf("\nUsage:\n./fileSystemOper  [fileSystem.data file] [operation] [parameters]\n");
}

void print_superblock(){
    printf("==========SUPERBLOCK==========\n");
    printf("state               : %d\n"
           "total blocks        : %d\n"
           "free blocks         : %d\n"
           "first block         : %d\n" 
           "block size(byte)    : %d\n"
           "# inodes per block  : %d\n" 
           "# inode blocks      : %d\n" 
           "# free inode block  : %d\n" 
           "first inode         : %d\n"
           "inode size(byte)    : %d\n"
           "sb size(byte)       : %d\n",
           sb.state,
           sb.total_blocks,
           sb.num_free_blocks,
           sb.first_block,
           sb.block_size*1000,
           sb.inodes_per_block,
           sb.total_inode_blocks,
           sb.num_free_inodes,
           sb.first_inode,
           sb.inode_size,
           sb.superblock_size);
}

void print_inode(struct Inode i){
    printf("==========INODE %d===========\n",i.id);

    printf("type            : ");
    switch(i.type){
        case 0: printf("directory\n"); break;
        case 1: printf("file\n"); break;
        default: printf("free\n"); break;
    }

    printf("name            : %s\n"
           "direct_block_ad : %d\n"
           "num of blocks   : %d\n"
           "size            : %lu\n"
           "last modified   : %s",
           get_name(i.direct_blocks),
           i.direct_blocks,
           i.num_of_blocks,
           i.size,
           asctime(&(i.last_modified)));
}

void print_inode_n(int i){
    int x, y;
    x = i/sb.total_inode_blocks;
    y = i%sb.inodes_per_block;

    print_inode(inodes[x][y]);
}




int read_bit(int n){
    return (bitmap[n/8] & (1 << (n%8))) != 0;
}

int set_bit(int b, int n){
    int status;
    if(n == 0 || n == 1){
        bitmap[b/8] |= (n << (b%8));
        fseek(fp, 1000*sb.block_size*(sb.total_inode_blocks+1) + sizeof(struct Block), SEEK_SET);
        status = fwrite (bitmap,  1000*sb.block_size-sizeof(struct Block), 1, fp); if(status <= 0) errExit("fread @read_filesystem6");
        return 0;
    }
    else
        errExit("Trying to set bit to non-binary value @set_bit");
    return 2;
}


void print_bitmap(){
    printf("============BITMAP============\n");

    for(int i = 0; i < sb.total_inode_blocks*sb.inodes_per_block; i++){
        int value = read_bit(i);
        printf("%d",value);
        if(i > 0 && i % 80 == 79)
            printf("\n");

    }
    printf("\n");
}

char* get_name(int address){
    if(address >= 0)
        return blocks[address].name;
    return "";
}

char* get_block_type(struct Block b){
    switch(b.type){
        case BLOCK_DIRECTORY_CONTENT: return "DIRECTORY CONTENT";
        case BLOCK_INODE_MAP: return "INODE MAP";
        case BLOCK_FILE_CONTENT: return "FILE CONTENT";
        case BLOCK_FREE: return "FREE BLOCK";
        case BLOCK_SUPER: return "SUPER BLOCK";
        case BLOCK_INODE: return "INODES BLOCK";
        default: return "CORRUPTED BLOCK";
    }
}


int read_filesystem(){
    int status = 0;
    struct Block block_sb;

    status = fread(&block_sb, sizeof(struct Block), 1, fp); if(status <= 0) errExit("fread @read_filesystem1");
    status = fread(&sb, sizeof(struct Superblock), 1, fp); if(status <= 0) errExit("fread @read_filesystem2");

    // Initilize data block info structure===========================
    blocks = malloc(sb.total_blocks* sizeof(struct Block));
    blocks[0] = block_sb;

    // ==============================================================

    // Initilize inode block structure===============================
    inodes = malloc(sb.total_inode_blocks* sizeof(struct Inode *)); if (inodes == NULL) errExit("Malloc error @inodes");
    for(int i = 0; i < sb.total_inode_blocks; i++){
        inodes[i] = malloc(sb.inodes_per_block*sizeof(struct Inode)); if (inodes == NULL) errExit("Malloc error @inodes[i]");
    }
    // ==============================================================

    // Allocate bitmap structure=====================================
    bitmap = malloc(sizeof(char)*(1000*sb.block_size)-sizeof(struct Block)+1); if (bitmap == NULL) errExit("Malloc error @bitmap");

    // ==============================================================

    fseek(fp, 1000*sb.block_size, SEEK_SET);


    for(int i = 0; i < sb.total_inode_blocks; i++){
        // Write block information
        status = fread(&blocks[i+1], sizeof(struct Block), 1, fp); if(status <= 0) errExit("fread @read_filesystem3"); 
        
        // Write empty inodes to the block
        for(int j = 0; j < sb.inodes_per_block; j++){
            status = fread (&inodes[i][j], sizeof(struct Inode), 1, fp); if(status <= 0) errExit("fread @read_filesystem4");
            //printf("indirect1: %d\n", inodes[i][j].indirect_blocks[0]);
        }

        fseek(fp, 1000*sb.block_size*(i+2), SEEK_SET);
    }

    //Read inode bitmap==================================
    status = fread(&blocks[1+sb.total_inode_blocks], sizeof(struct Block), 1, fp); if(status <= 0) errExit("fread @read_filesystem5");
    status = fread (bitmap,  1000*sb.block_size-sizeof(struct Block), 1, fp); if(status <= 0) errExit("fread @read_filesystem6");
    fseek(fp, 1000*sb.block_size*(2+sb.total_inode_blocks), SEEK_SET);
    for(int i = 0; i < sb.num_free_blocks; i++){
        status = fread(&blocks[i+2+sb.total_inode_blocks], sizeof(struct Block), 1, fp);  if(status <= 0) errExit("fread @read_filesystem7");
        
        if( blocks[i+2+sb.total_inode_blocks].type == BLOCK_DIRECTORY_CONTENT){
            struct Directory d;
            status = fread(&d, sizeof(struct Directory), 1, fp); if(status <= 0) errExit("fread @read_filesystem8");
            //printf("links: %s\n", d.links[0]);
            //printf("links: %s\n", d.links[1]);
            //printf("n: %d\n", d.n);

        }

        fseek(fp, 1000*sb.block_size*(i+3+sb.total_inode_blocks), SEEK_SET);
    }

    return 0;
}


int cmd_dumpe2fs(){
    unsigned long bitmap_n = sizeof(char)*(1000*sb.block_size)-sizeof(struct Block)+1;
    print_superblock();
    
    print_bitmap();
    printf("=======OCCUPIED INODES========\n");
    for(int i = 0; i < bitmap_n; i++){
        if(read_bit(i) == 1){
            print_inode_n(i);
        }
    }
    printf("============BLOCKS============\n");
    printf("%-13s%-20s%s\n","Address", "Type", "Name");
    for(int i = 0; i < sb.total_blocks; i++){
        printf("%-13d",i);
        printf("%-20s%s\n",get_block_type(blocks[i]), blocks[i].name);
    }

    return 0;
}

int cmd_fsck(){

  
    printf("\nIN USE:\n");
    for(int i = 0; i < sb.total_blocks; i++){
        printf("%d", (blocks[i].type != BLOCK_FREE) ? 1 : 0);
    }
    printf("\nFREE  :\n");
    for(int i = 0; i < sb.total_blocks; i++){
            printf("%d", (blocks[i].type == BLOCK_FREE) ? 1 : 0);
    }
    printf("\n");
    

    return 0;
}