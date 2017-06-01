#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "ext2_fs.h"

#define BUF_SIZE 1024

struct ext2_super_block * superBlock;
struct ext2_group_desc * groupBlock;

unsigned char *blockBitmap;
unsigned char *inodeBitmap;

unsigned int numberOfInodes;

int fd;

//Free memory for all allocated blocks
void freeMemory()
{
  free(superBlock);
  free(groupBlock);
  free(blockBitmap);
  free(inodeBitmap);  
}

//Error return function
void error(char* msg)
{
  fprintf(stderr, "%s\n", msg);
  exit(2);
}

//Prints each of the expected superblock fields
void printSuperblock()
{
  long long unsigned int blocksCount = (long long unsigned int) superBlock->s_blocks_count;
  long long unsigned int inodesCount = (long long unsigned int) superBlock->s_inodes_count;
  long long unsigned int blockSize_1 = (long long unsigned int) superBlock->s_log_block_size;
  long long unsigned int blockSize = EXT2_MIN_BLOCK_SIZE << blockSize_1;
  long long unsigned int inodeSize = (long long unsigned int) superBlock->s_inode_size;
  long long unsigned int blocksPerGroup = (long long unsigned int) superBlock->s_blocks_per_group;
  long long unsigned int inodesPerGroup = (long long unsigned int) superBlock->s_inodes_per_group;
  long long unsigned int firstUnreservedInode = (long long unsigned int) superBlock->s_first_ino;
  fprintf(stdout, "SUPERBLOCK,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n", blocksCount, inodesCount, blockSize, inodeSize, blocksPerGroup, inodesPerGroup, firstUnreservedInode);
}

//Print each of expected fields for a group block
void printGroupBlock() {
  int blockNum = 0;
  long long unsigned int totalNumofBlocks = (long long unsigned int) superBlock->s_blocks_count;
  long long unsigned int totalNumofInodes = (long long unsigned int) superBlock->s_inodes_count;
  long long unsigned int freeBlocks = (long long unsigned int) groupBlock->bg_free_blocks_count;
  long long unsigned int freeInodes = (long long unsigned int) groupBlock->bg_free_inodes_count;
  long long unsigned int  freeBlockBitmapNum = (long long unsigned int) groupBlock->bg_block_bitmap;
  long long unsigned int  freeInodeBitmapNum = (long long unsigned int) groupBlock->bg_inode_bitmap;
  long long unsigned int firstInodeBlockNum = (long long unsigned int) groupBlock->bg_inode_table;
  fprintf(stdout, "GROUP,%d,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n", blockNum, totalNumofBlocks, totalNumofInodes, freeBlocks, freeInodes, freeBlockBitmapNum, freeInodeBitmapNum, firstInodeBlockNum);
}

//Takes input of blockNum and prints the message for this free block
void printFreeBlock(int blockNum) {
  fprintf(stdout, "BFREE,%d\n",blockNum);
}

//Prints all of the free blocks
void printFreeBlocks() {

  long long unsigned int totalNumOfBlocks = (long long unsigned int) superBlock->s_blocks_count;
  int byte,bit;

  //Goes through each byte in the bitmap
  for (byte = 0;byte < (totalNumOfBlocks-1)/8+1; byte++)
    {
      //Gets first byte to be read
      unsigned int mapByte = (int) blockBitmap[byte];

      //Goes through bit by bit and starting with the smallest bit on each byte, checks if it is a 0 then prints free block
      int bit = 0;
      for(bit = 0; bit < 8 && ((byte*8)+(bit+1)) < totalNumOfBlocks; bit++)
	{
	  if(!(mapByte & (unsigned int) (pow(2,bit))))
	    printFreeBlock((byte*8)+(bit+1));
	}
    }
}
 
//Prints free inodes
void printFreeInode(int inodeNum) {
  fprintf(stdout, "IFREE,%d\n",inodeNum);
}

//Prints all of the free inode blocks
void printFreeInodes(int* isInodeUsed) {
  long long unsigned int totalNumOfInodes = (long long unsigned int) superBlock->s_inodes_count;
  int byte,bit;

  //Goes through each byte in the bitmap
  for (byte = 0;byte < (totalNumOfInodes-1)/8+1; byte++)
    {
      //Gets first byte to be read
      unsigned int mapByte = (int) inodeBitmap[byte];

      //Goes through bit by bit and starting with the smallest bit in each byte, checks if bit is 1 and if so prints free block
      int bit = 0;
      for(bit = 0; bit < 8 && ((byte*8)+(bit)) < totalNumOfInodes; bit++)
        {
	  if(!(mapByte & (unsigned int) (pow(2,bit))))
	    printFreeInode((byte*8)+(bit+1));
	  //If inode is used, set it as used for printing of inodes
	  else
	    isInodeUsed[(byte*8) + bit] = 1;

        }
        
    }
}

//Helper function that checks the provided mode and returns the char of the file type
char getFileType(long long unsigned int mode)   {
    if(mode & 0xA000 && mode & 0x2000)
        return 's';
    else if(mode & 0x8000)
        return 'f';
    else if(mode & 0x4000)
        return 'd';
    else
        return '?';
}

struct ext2_dir_entry * entry;
struct ext2_inode * inode;

//Prints a single directory entry
void printDirectoryEntry(struct ext2_dir_entry * entry, int inodeNum, unsigned long long int byteOffset) {
  unsigned long long int fileInode = entry->inode;
  unsigned long long int entryLength = entry->rec_len;
  unsigned long long int fileType = entry->file_type;
  char fileName[EXT2_NAME_LEN+1];
  memcpy(fileName, entry->name, strlen(entry->name));
  fileName[strlen(entry->name)] = '\0';
  unsigned long long int fileNameLength = strlen(fileName);
  fprintf(stdout, "DIRENT,%d,%llu,%llu,%llu,%llu,'%s'\n", inodeNum, byteOffset, fileInode, entryLength, fileNameLength, fileName);
}

//Gets an inode that is a directory and prints out the directory entries within this directory 
void printDirectoryEntries(struct ext2_inode * inode, int inodeNum) {  
    struct ext2_dir_entry * entry;
    //Get the amount of blocks to read in that store directory entries. If more than 12, we only want 12 main data blocks.
    unsigned int numBlocks = inode->i_blocks/2;
    if(numBlocks > 12)
      numBlocks = 12;
    unsigned char block[BUF_SIZE*numBlocks];
    
    int i, size = 0, blockNum = 0;

    //Read in all of the data blocks 
    if (pread(fd, block, BUF_SIZE*numBlocks, inode->i_block[blockNum]*BUF_SIZE) == -1)
      error("Error reading directory entries");
    entry = (struct ext2_dir_entry *) block;
    
    //Go through each directory entry, and print, then move to the next by adding the size of the directory to the directory entry we are viewing, leading to next entry
    while (size < inode->i_size) {
      if (entry->inode == 0 || entry->rec_len == 0)
	break;
      printDirectoryEntry(entry, inodeNum, size);
      size += entry->rec_len;      
      entry = (void*) entry + entry->rec_len;
    }
    size = 0;
}

//Print each of the indirect blocks when referenced from a 2nd or 3rd indirect block
void printIndirectBlocks(int owningNum, int level, int logicalOffset, long long unsigned int thisBlockNum ) {
  
  //If this block is not allocated, do nothing
    if (thisBlockNum == 0 || level == 0)
        return;
    
    //Read in the block pointers of this single block into buffer for interpretation
    unsigned int indirectBlockPtrs[BUF_SIZE/sizeof(unsigned int)];
    if (pread(fd, indirectBlockPtrs, BUF_SIZE, thisBlockNum * BUF_SIZE) == -1)
        error("Unable to read indirect block pointers");
    

    //If a double indirect block, each successive block contains blocks that are 256 logical blocks later
    int incrementSize = 1;
    if(level == 2)
        incrementSize = 256;

    //Print each contained blocks info if it is allocated
    int i;
    for (i = 0; i < BUF_SIZE/4; i++) {
        if (indirectBlockPtrs[i] != 0 && indirectBlockPtrs[i] < superBlock->s_blocks_count) {
            fprintf(stdout, "INDIRECT,%d,%d,%d,%llu,%u\n", owningNum, level, logicalOffset+i*incrementSize, thisBlockNum, indirectBlockPtrs[i]);
        }
    }
    
    //If this is a double indirect block, print entries for all single indirect blocks
    if(level == 2) {
        for (i = 0; i < BUF_SIZE/4; i++) {
            printIndirectBlocks(owningNum, level - 1, logicalOffset + i*incrementSize, indirectBlockPtrs[i]);
        }
    }
}

//Print indirect blocks for blocks 13,14, and 15 for inode
void printIndirectBlockInit(struct ext2_inode * inode, int owningNum, int level, int nthBlock) {

  //If nothing here, exit
  if (inode->i_block[nthBlock] == 0 || level == 0)
    return;

  long long unsigned int indirectBlockNum = inode->i_block[nthBlock];

  // Get the pointers stored at the indirect block
  unsigned int indirectBlockPtrs[BUF_SIZE/sizeof(unsigned int)];
  if (pread(fd, indirectBlockPtrs, BUF_SIZE, indirectBlockNum * BUF_SIZE) == -1)
    error("Unable to read indirect block pointers");

  //Set default values for offset (single indirect) and incrementSize (each block is one logical block offset)
  int offset = 12;
  int incrementSize = 1;
  
  //Change values based on if double or triple indirect block
  if(level == 2)
    {
    offset += 256;
    incrementSize = 256;
    }
  else if(level == 3)
    {
      offset += (256 + 65536);
      incrementSize = 65536;
    }

  //Print each contained blocks info
  int i;
  for (i = 0; i < BUF_SIZE/4; i++) {
    if (indirectBlockPtrs[i] != 0 && indirectBlockPtrs[i] < superBlock->s_blocks_count) {
      fprintf(stdout, "INDIRECT,%d,%d,%d,%llu,%u\n", owningNum, level, offset+i*incrementSize, indirectBlockNum, indirectBlockPtrs[i]);
    }
  }
  
  //If double or triple indirect, print out summaries for each contained block
  if(level != 1) {
    for (i = 0; i < BUF_SIZE/4; i++) {
      printIndirectBlocks(owningNum, level - 1, offset + i*incrementSize, indirectBlockPtrs[i]);
    }
  }
  
}

//Print basic info for each inode
void printInodeSummary(struct ext2_inode* thisInode, int inodeNum)   {
    
    long long unsigned int i_mode = (long long unsigned int) thisInode->i_mode;
    char fileType = getFileType(i_mode);
    
    long long unsigned int mode = i_mode & 0xFFF;
    
    //Calculate all neccessary times in correct GMT format
    time_t rawAccessTime = (time_t) thisInode->i_atime;
    char accessTimeString[100];
    strftime(accessTimeString, 100, "%m/%d/%y %H:%M:%S", gmtime(&rawAccessTime));
    
    time_t rawCreationTime = (time_t) thisInode->i_ctime;
    char creationTimeString[100];
    strftime(creationTimeString, 100, "%m/%d/%y %H:%M:%S", gmtime(&rawCreationTime));
    
    time_t rawModTime = (time_t) thisInode->i_mtime;
    char modTimeString[100];
    strftime(modTimeString, 100, "%m/%d/%y %H:%M:%S", gmtime(&rawModTime));
    
    fprintf(stdout, "INODE,%d,%c,%llo,%llu,%llu,%llu,%s,%s,%s,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",inodeNum, fileType, mode, (long long unsigned int) thisInode->i_uid, (long long unsigned int) thisInode->i_gid, (long long unsigned int)thisInode->i_links_count, creationTimeString, modTimeString, accessTimeString, (long long unsigned int) thisInode->i_size, (long long unsigned int) thisInode->i_blocks, (long long unsigned int) thisInode->i_block[0], (long long unsigned int) thisInode->i_block[1], (long long unsigned int) thisInode->i_block[2], (long long unsigned int) thisInode->i_block[3], (long long unsigned int) thisInode->i_block[4], (long long unsigned int) thisInode->i_block[5], (long long unsigned int) thisInode->i_block[6], (long long unsigned int) thisInode->i_block[7], (long long unsigned int) thisInode->i_block[8], (long long unsigned int) thisInode->i_block[9], (long long unsigned int) thisInode->i_block[10], (long long unsigned int) thisInode->i_block[11], (long long unsigned int) thisInode->i_block[12], (long long unsigned int) thisInode->i_block[13], (long long unsigned int) thisInode->i_block[14]);
   
    //Based on inode type and contents, do neccessary prints
    if(fileType == 'd')
      printDirectoryEntries(thisInode, inodeNum);
    if (thisInode->i_block[12] != 0)
      printIndirectBlockInit(thisInode, inodeNum, 1, 12);
    if (thisInode->i_block[13] != 0)
      printIndirectBlockInit(thisInode, inodeNum, 2, 13);
    if (thisInode->i_block[14] != 0)
      printIndirectBlockInit(thisInode, inodeNum, 3, 14);
}

//Go through each inode and print the inodeSummary for this inode if it is in use
void printInodeSummaries(struct ext2_inode* inodes, int* isInodeUsed)    {
    
    int inodeCounter;
    for (inodeCounter = 1;inodeCounter <= numberOfInodes; inodeCounter++) {
      if(isInodeUsed[inodeCounter-1] == 1 && inodes[inodeCounter-1].i_links_count > 0)
	printInodeSummary(&inodes[inodeCounter-1],inodeCounter);       
    }
}

int
main (int argc, char **argv)
{
  // File system image name is in argv
  if (argc != 2) {
    fprintf(stderr, "USAGE: ./lab3a FILESYSTEM.img\n");
    exit(1);
  }

  //Check to see if we can open provided disk image
  char * img = argv[1];
  fd = open(img, O_RDONLY);
  if (fd == -1)
    error("Unable to open disk image");
  
  //Read in superblock and print info
  int blockToRead = 1;
  superBlock = (struct ext2_super_block *) malloc (BUF_SIZE);
  if (pread(fd, superBlock, BUF_SIZE, BUF_SIZE*blockToRead) == -1)
    error("Error reading superblock");

  printSuperblock();

  //Read in groupblock and print info
  blockToRead = 2;
  
  groupBlock = (struct ext2_group_desc *) malloc (BUF_SIZE);
  if (pread(fd, groupBlock, BUF_SIZE, BUF_SIZE*blockToRead) == -1)
    error("Error reading groups");
  printGroupBlock();

  //Read in blockbitmap and print free
  blockToRead = 3;    
    
  blockBitmap = malloc (BUF_SIZE);
  if (pread(fd, blockBitmap, BUF_SIZE, BUF_SIZE*blockToRead) == -1)
    error("Error reading block bitmap");
  printFreeBlocks();
    
  numberOfInodes = (long long unsigned int) superBlock->s_inodes_per_group;
  int isInodeUsed[numberOfInodes];
  memset(isInodeUsed, 0, sizeof(isInodeUsed));
    
  blockToRead = 4;

    //Pread in inodebitmap and print free while setting used as used
  inodeBitmap = malloc (BUF_SIZE);
  if (pread(fd, inodeBitmap, BUF_SIZE, BUF_SIZE*blockToRead) == -1)
    error("Error reading inode bitmap");
    
  printFreeInodes(isInodeUsed);

  blockToRead = 5;
  
  //Prepare to print inodeSummaries and then print
  unsigned int inodesPerBlock = BUF_SIZE/sizeof(struct ext2_inode);
  unsigned int inodeTableBlocks = numberOfInodes / inodesPerBlock;
  
  struct ext2_inode inodes[numberOfInodes];
  if (pread(fd, inodes, BUF_SIZE*inodeTableBlocks, BUF_SIZE*blockToRead) == -1)
    error("Error reading inode table");
  printInodeSummaries(inodes, isInodeUsed);

  blockToRead += inodeTableBlocks;
  
  //Free allocated memory
  freeMemory();

  return 0;
}
