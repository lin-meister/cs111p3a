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

//
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

long long unsigned int freeBlockBitmapNum;
long long unsigned int freeInodeBitmapNum;

void printGroupBlock() {
  int blockNum = 0;
  long long unsigned int totalNumofBlocks = (long long unsigned int) superBlock->s_blocks_count;
  long long unsigned int totalNumofInodes = (long long unsigned int) superBlock->s_inodes_count;
  long long unsigned int freeBlocks = (long long unsigned int) groupBlock->bg_free_blocks_count;
  long long unsigned int freeInodes = (long long unsigned int) groupBlock->bg_free_inodes_count;
  freeBlockBitmapNum = (long long unsigned int) groupBlock->bg_block_bitmap;
  freeInodeBitmapNum = (long long unsigned int) groupBlock->bg_inode_bitmap;
  long long unsigned int firstInodeBlockNum = (long long unsigned int) groupBlock->bg_inode_table;
  fprintf(stdout, "GROUP,%d,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n", blockNum, totalNumofBlocks, totalNumofInodes, freeBlocks, freeInodes, freeBlockBitmapNum, freeInodeBitmapNum, firstInodeBlockNum);
}

void printFreeBlock(int blockNum) {
  fprintf(stdout, "BFREE,%d\n",blockNum);
}

void printFreeBlocks() {

  long long unsigned int totalNumOfBlocks = (long long unsigned int) superBlock->s_blocks_count;
  int byte,bit;

  for (byte = 0;byte < (totalNumOfBlocks-1)/8+1; byte++)
    {
      unsigned int mapByte = (int) blockBitmap[byte];
      int bit = 0;
      for(bit = 0; bit < 8 && ((byte*8)+(bit+1)) < totalNumOfBlocks; bit++)
	{
	  //	  printf("2^Bit is %d,byte is %d, mapByte is %u,is free is %u\n",2^bit,byte,mapByte,(mapByte & (unsigned int)(pow(2,bit))));
	  if(!(mapByte & (unsigned int) (pow(2,bit))))
	    printFreeBlock((byte*8)+(bit+1));
	}
    }
}
 
void printFreeInode(int inodeNum) {
  fprintf(stdout, "IFREE,%d\n",inodeNum);
}

void printFreeInodes(int* isInodeUsed) {
  long long unsigned int totalNumOfInodes = (long long unsigned int) superBlock->s_inodes_count;
  int byte,bit;

  for (byte = 0;byte < (totalNumOfInodes-1)/8+1; byte++)
    {
      unsigned int mapByte = (int) inodeBitmap[byte];
      int bit = 0;
      for(bit = 0; bit < 8 && ((byte*8)+(bit)) < totalNumOfInodes; bit++)
        {
	  //          printf("2^Bit is %d,byte is %d, mapByte is %u,is free is %u\n",2^bit,byte,mapByte,(mapByte & (unsigned int)(pow(2,bit))));
	  if(!(mapByte & (unsigned int) (pow(2,bit))))
	    printFreeInode((byte*8)+(bit+1));
      else
          isInodeUsed[(byte*8) + bit] = 1;

        }
        
    }
}


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

void printDirectoryEntries(struct ext2_inode * inode, int inodeNum) {  
    struct ext2_dir_entry * entry;
    unsigned int numBlocks = inode->i_blocks/2;
    unsigned char block[BUF_SIZE*numBlocks];
    
    int i, size = 0, blockNum = 0;

    if (pread(fd, block, BUF_SIZE*numBlocks, inode->i_block[blockNum]*BUF_SIZE) == -1)
      error("Error reading directory entries");
    entry = (struct ext2_dir_entry *) block;
    
    while (size < inode->i_size) {
      if (entry->inode == 0 || entry->rec_len == 0)
	break;
      printDirectoryEntry(entry, inodeNum, size);
      size += entry->rec_len;      
      entry = (void*) entry + entry->rec_len;
    }
    size = 0;
}

void printIndirectBlocks(int owningNum, int level, int logicalOffset, long long unsigned int thisBlockNum ) {
  
    if (thisBlockNum == 0 || level == 0)
        return;
    
    unsigned int indirectBlockPtrs[BUF_SIZE/sizeof(unsigned int)];
    if (pread(fd, indirectBlockPtrs, BUF_SIZE, thisBlockNum * BUF_SIZE) == -1)
        error("Unable to read indirect block pointers");
    
    int incrementSize = 1;
    if(level == 2)
    {
        incrementSize = 256;
    }

    int i;
    for (i = 0; i < BUF_SIZE/4; i++) {
        if (indirectBlockPtrs[i] != 0 && indirectBlockPtrs[i] < superBlock->s_blocks_count) {
            fprintf(stdout, "INDIRECT,%d,%d,%d,%llu,%u\n", owningNum, level, logicalOffset+i*incrementSize, thisBlockNum, indirectBlockPtrs[i]);
            //printIndirectBlock(inode, inodeNum, level-1, offset/12);
        }
    }
    
    if(level == 2) {
        for (i = 0; i < BUF_SIZE/4; i++) {
            printIndirectBlocks(owningNum, level - 1, logicalOffset + i*incrementSize, indirectBlockPtrs[i]);
        }
    }
}

void printIndirectBlockInit(struct ext2_inode * inode, int owningNum, int level, int nthBlock) {
  if (inode->i_block[nthBlock] == 0 || level == 0)
    return;

  long long unsigned int indirectBlockNum = inode->i_block[nthBlock];

  // Get the pointers stored at the indirect block
  unsigned int indirectBlockPtrs[BUF_SIZE/sizeof(unsigned int)];
  if (pread(fd, indirectBlockPtrs, BUF_SIZE, indirectBlockNum * BUF_SIZE) == -1)
    error("Unable to read indirect block pointers");

  int offset = 12;
  int incrementSize = 1;
  
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

  int i;
  for (i = 0; i < BUF_SIZE/4; i++) {
    if (indirectBlockPtrs[i] != 0 && indirectBlockPtrs[i] < superBlock->s_blocks_count) {
      fprintf(stdout, "INDIRECT,%d,%d,%d,%llu,%u\n", owningNum, level, offset+i*incrementSize, indirectBlockNum, indirectBlockPtrs[i]);
      //printIndirectBlock(inode, inodeNum, level-1, offset/12);
    }
  }

  for (i = 0; i < BUF_SIZE/4; i++) {
      printIndirectBlocks(owningNum, level - 1, offset + i*incrementSize, indirectBlockPtrs[i]);
  }

  
}

void printInodeSummary(struct ext2_inode* thisInode, int inodeNum)   {
    
    long long unsigned int i_mode = (long long unsigned int) thisInode->i_mode;
    char fileType = getFileType(i_mode);
    
    long long unsigned int mode = i_mode & 0xFFF;
    
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
    if(fileType == 'd')
      printDirectoryEntries(thisInode, inodeNum);
    if (thisInode->i_block[12] != 0)
      printIndirectBlockInit(thisInode, inodeNum, 1, 12);
    if (thisInode->i_block[13] != 0)
      printIndirectBlockInit(thisInode, inodeNum, 2, 13);
    if (thisInode->i_block[14] != 0)
      printIndirectBlockInit(thisInode, inodeNum, 3, 14);
}

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
  char * img = argv[1];
  fd = open(img, O_RDONLY);
  if (fd == -1)
    error("Unable to open disk image");
  
  int blockToRead = 1;
  superBlock = (struct ext2_super_block *) malloc (BUF_SIZE);
  if (pread(fd, superBlock, BUF_SIZE, BUF_SIZE*blockToRead) == -1)
    error("Error reading superblock");

  printSuperblock();
  blockToRead = 2;
  
  groupBlock = (struct ext2_group_desc *) malloc (BUF_SIZE);
  if (pread(fd, groupBlock, BUF_SIZE, BUF_SIZE*blockToRead) == -1)
    error("Error reading groups");
  printGroupBlock();

  blockToRead = 3;    
    
  blockBitmap = malloc (BUF_SIZE);
  if (pread(fd, blockBitmap, BUF_SIZE, BUF_SIZE*blockToRead) == -1)
    error("Error reading block bitmap");
  printFreeBlocks();
    
  numberOfInodes = (long long unsigned int) superBlock->s_inodes_per_group;
  int isInodeUsed[numberOfInodes];
  memset(isInodeUsed, 0, sizeof(isInodeUsed));
    
  blockToRead = 4;

  inodeBitmap = malloc (BUF_SIZE);
  if (pread(fd, inodeBitmap, BUF_SIZE, BUF_SIZE*blockToRead) == -1)
    error("Error reading inode bitmap");
    
  printFreeInodes(isInodeUsed);

  blockToRead = 5;
  
  unsigned int inodesPerBlock = BUF_SIZE/sizeof(struct ext2_inode);
  unsigned int inodeTableBlocks = numberOfInodes / inodesPerBlock;
  
  struct ext2_inode inodes[numberOfInodes];
  if (pread(fd, inodes, BUF_SIZE*inodeTableBlocks, BUF_SIZE*blockToRead) == -1)
    error("Error reading inode table");
  printInodeSummaries(inodes, isInodeUsed);

  blockToRead += inodeTableBlocks;
  
  freeMemory();

  return 0;
}
