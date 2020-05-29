//
//  main.c
//  File System Part 2
//
//  Created by Muhammed Okumuş on 23.05.2020.
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
    int address;        // Block index
    int type;           // BlockType enum
    char name[16];      // Block name
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
} sb;


struct Inode {
    int id;                                 // file/directory id
    int direct_blocks;                      // start address of direct blocks that holds file contents
    int num_of_blocks;                      // Total number of blocks that file occupies
    int indirect_blocks[DIRECT_BLOCKS*2];   // indirect block pointers
    int type;                               // 0 = directory, Non-zero positive = file, Negative = not used
    unsigned long size;                     // File size in bytes
    struct tm last_modified;                // Last modification time
} root;

struct Directory{
    char links[16][16];
    int n;
};

unsigned char *bitmap;  //https://stackoverflow.com/questions/44978126/structure-for-an-array-of-bits-in-c
struct Inode** inodes;
char *cdata;
// ==========================================


// Function Prototypes ======================
// Display/Debug Functions
void print_usage();
void print_superblock();
void print_inode(struct Inode i);

// Initiliziation/File IO Functions
void init_superblock(int input_bs, int input_fi, int num_of_inode_blocks);
void format_fs(char* filename);

// Mock function
char* get_name(int i);
//===========================================

int main(int argc, char* argv[]){
    int input_bs,                   // block size
        input_fi,                   // free inodes 
        inodes_per_block,           // # of inodes per data block(calculated)
        num_of_inode_blocks;        // # of data blocks occupied bye inodes
    char input_filepath[MAX_PATH];  // 1MB  Linux file to be created

    unsigned long data_block_size;



    // Parse commands =============================================
    if (argc == 1 || argc > 4) errExit("Parameters are missing or wrong");
    printf("============INPUTS============\n");
    for(int i = 1; i < argc; i++){
        switch(i){
            case 1: input_bs = atoi(argv[i]); printf("Block Size(KB): %d\n", input_bs); break;
            case 2: input_fi = atoi(argv[i]); printf("Free Inodes   : %d\n", input_fi); break;
            case 3: snprintf(input_filepath, MAX_PATH,"%s",argv[i]); printf("File path     : %s\n", input_filepath); break;
            default: errExit("Logic error while parsing commands");
        }
    }
    // Error check
    if((input_bs*1000 - sizeof(struct Block)) <= input_fi )
        errExit("Too many inodes for given block size, bitmap failure");
    
    if(input_bs < 1)    errExit("Block size can't be lower than 1KB");
    if(input_bs > 100)  errExit("Block size can't be larger than 100KB");
    if(input_fi < 1)    errExit("Number of inodes can't be less than 1");
    if(input_fi > 4000) errExit("Inode size can't be lower than 4000");

    // ============================================= Parse commands

    // Initilize data block structure===============================
    data_block_size =  sizeof(char)*(1000*input_bs)-sizeof(struct Block)+1; 
    cdata = malloc(data_block_size); if (cdata == NULL) errExit("Malloc error @cdata");
    for(int i = 0; i < data_block_size-1; i++)
        cdata[i] = 'a';
    cdata[data_block_size-1] = '\0'; 
    // ==============================================================

    // Initlize bitmap structure=====================================
    bitmap = malloc(data_block_size); if (bitmap == NULL) errExit("Malloc error @bitmap");
    for(int i = 0; i < data_block_size-1; i++)
        bitmap[i] = 0x00;
    

    for(int i = 0; i < data_block_size-1; i++)
        bitmap[i/8] |= (0 << (i%8));
    
    bitmap[0/8] |= (1 << (0%8));


    bitmap[data_block_size-1] = '\0';
    // ==============================================================
    

    // Initilize inode block structure===============================
    inodes_per_block = ((1000*input_bs-sizeof(struct Block))/sizeof(struct Inode));
    num_of_inode_blocks = (input_fi/inodes_per_block) + ((input_fi%inodes_per_block) != 0); // (a / b) + ((a % b) != 0), ceil()
    inodes = malloc(num_of_inode_blocks* sizeof(struct Inode *)); if (inodes == NULL) errExit("Malloc error @inodes");
    for(int i = 0; i < num_of_inode_blocks; i++){
        inodes[i] = malloc(inodes_per_block*sizeof(struct Inode)); if (inodes == NULL) errExit("Malloc error @inodes[i]");
    }
    // ==============================================================

    // Initlize superblock 
    init_superblock(input_bs, input_fi, num_of_inode_blocks);

    // Create file system Linux file
    format_fs(input_filepath);

    // Print superblokc
    print_superblock();
    print_inode(root);

    // Free resources ====================================
    printf("==============================\n");
    printf("Freeing resources...\n");
    for(int i = 0; i < num_of_inode_blocks; i++)
        free(inodes[i]);

    free(inodes);
    free(cdata);
    free(bitmap);
    printf("Done!\n");
    //===================================== Free resources 
    exit(EXIT_SUCCESS);
}

void print_usage(void){
    printf("\nUsage:\n./makeFileSystem  [block size] [free inodes] [file path]\n");
}

void init_superblock(int input_bs, int input_fi, int num_of_inode_blocks){
    // Init superblock
    sb.state = 0;            
    sb.total_blocks = ONE_MB/(input_bs*1000);      
    sb.num_free_blocks = ONE_MB/(input_bs*1000)-num_of_inode_blocks-2;   
    sb.first_block = 0 ;        
    sb.block_size = input_bs;   
    sb.inodes_per_block = (int) ((1000*input_bs-sizeof(struct Block))/sizeof(struct Inode));
    sb.total_inode_blocks = num_of_inode_blocks;
    sb.num_free_inodes = input_fi-1; //Reserve one to root
    sb.first_inode = 0;      
    sb.inode_size = sizeof(struct Inode);
    sb.superblock_size = sizeof(struct Superblock);

    // Init root
    time_t t = time(NULL);
    root.id = 0;

    root.direct_blocks = 2+num_of_inode_blocks;
    
    root.num_of_blocks = 1;
    root.type = 0;
    root.size = sb.block_size*root.num_of_blocks;
    root.last_modified = *localtime(&t);
    for(int i = 0; i < DIRECT_BLOCKS*2; i++)
        root.indirect_blocks[i] = -1;
    inodes[0][0] = root;
    
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
    printf("============INODE=============\n");
    printf("type            : %s\n"
           "name            : %s\n"
           "direct block ad : %d (db index)\n"
           "num of blocks   : %d\n"
           "size(KB)        : %lu\n"
           "last modified   : %s",
           (i.type == 0) ? "directory" : "file",
           get_name(i.id),
           i.direct_blocks,
           i.num_of_blocks,
           i.size,
           asctime(&(i.last_modified)));
}

char* get_name(int i){
    // Code this in part 3
    return "root";
}

void format_fs(char* filename){
    int status = 0;
    unsigned long bytes_left_in_block;
    // Open file for writing
    FILE *outfile;
    outfile = fopen (filename, "w"); 
    if (outfile == NULL) errExit("Error opening file");

    
    // Initilize first block with super block ================
    // Write block information
    struct Block block = {0,BLOCK_SUPER};
    snprintf(block.name, 16, "sb_%d", 0); 
    status = fwrite (&block, sizeof(struct Block), 1, outfile); if(status <= 0) errExit("fwrite @format_fs1");
    // Write the superblock into the block
    status = fwrite (&sb, sizeof(struct Superblock), 1, outfile);  if(status <= 0) errExit("fwrite @format_fs2");
    // Fill block upto block size
    bytes_left_in_block = (1000*sb.block_size)-sizeof(struct Block)-sizeof(struct Superblock);
    status = fwrite (cdata, bytes_left_in_block, 1, outfile); if(status <= 0) errExit("fwrite @format_fs3");

    /*  To read superblock:
        1) Read block information: struct Block
        2) Then read superblock: struct Superblock
        3) rest of the bits in the block are junk bits
    */

    //========================================================

    // Initilize inode blocks ================================
    time_t t = time(NULL);
    for(int i = 0; i < sb.total_inode_blocks; i++){
        // Write block information
        struct Block block = {i+1,BLOCK_INODE};
        snprintf(block.name, 16, "t_%d", i+1); 
        status = fwrite (&block, sizeof(struct Block), 1, outfile); if(status <= 0) errExit("fwrite @format_fs4");
        // Write empty inodes to the block
        for(int j = 0; j < sb.inodes_per_block; j++){
            if(i == 0 && j == 0){
                // Root special case
                status = fwrite (&inodes[i][j], sizeof(struct Inode), 1, outfile); if(status <= 0) errExit("fwrite @format_fs5");
            }
            else{
                inodes[i][j].id = 0;
                inodes[i][j].direct_blocks = -1;
                inodes[i][j].num_of_blocks = 0;
                inodes[i][j].type = -1;
                inodes[i][j].size = 0;
                inodes[i][j].last_modified = *localtime(&t);
                for(int k = 0; k < DIRECT_BLOCKS*2; k++){
                    inodes[i][j].indirect_blocks[k] = -1;
                }
                status = fwrite (&inodes[i][j], sizeof(struct Inode), 1, outfile); if(status <= 0) errExit("fwrite @format_fs6");
            }

        }
        // Fill block upto block size
        bytes_left_in_block  = (1000*sb.block_size)-sizeof(struct Block)-(sb.inodes_per_block*sizeof(struct Inode));
        status = fwrite (cdata, bytes_left_in_block, 1, outfile); if(status <= 0) errExit("fwrite @format_fs7");
    }

    /*  To read inode block:
        0) Find the block that the inode searched for is in
        1) Read block information: struct Block
        2) Read sizeof(int) from file 
        3) if int_buffer = inode key > read int[DIRECT_BLOCKS] + int(num_of_blocks) + int(indirect_blocks) + int(type)
        4) else skip sizeof(int)*16 bytes and go to step 2
    */
    //========================================================

    //Initilize inode bitmap==================================
    // Write block information
    struct Block block_bitmap = {1+sb.inodes_per_block,BLOCK_INODE_MAP};
    snprintf(block_bitmap.name, 6, "bmap"); 
    fwrite (&block_bitmap, sizeof(struct Block), 1, outfile);
    // Write the bitmap into the block
    fwrite (bitmap, sb.block_size*1000-sizeof(struct Block), 1, outfile); 
    // No bits to fill as bitmap is big enough to cover whole block


    // Initilize file data blocks ================================
    // Data block for root



    // Calculate number of block left for file content
    int num_of_blocks = ONE_MB/(sb.block_size*1000) - 2 - sb.total_inode_blocks;
    for(int i = 0; i < num_of_blocks; i++){
        if(i == 0){
            struct Block block = {i+1+sb.total_inode_blocks,BLOCK_DIRECTORY_CONTENT};
            snprintf(block.name, 16, "root"); 
            fwrite (&block, sizeof(struct Block), 1, outfile); 
            struct Directory d = {};
            d.n = 0;
            snprintf(d.links[0], 16, ".");
            d.n++; 
            snprintf(d.links[1], 16, ".."); 
            d.n++; 
            fwrite(&d, sizeof(struct Directory), 1, outfile); 
            fwrite (cdata, strlen(cdata)-sizeof(struct Directory), 1, outfile);  
        }
        else{
            struct Block block = {i+1+sb.total_inode_blocks,BLOCK_FREE};
            snprintf(block.name, 16, "free_%d", i); 
            fwrite (&block, sizeof(struct Block), 1, outfile); 
            fwrite (cdata, strlen(cdata), 1, outfile);
        }

    }

    fclose (outfile); 
}