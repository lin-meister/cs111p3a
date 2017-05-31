#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
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

int fd;
int numberOfInodes;

void freeMemory()
{
  free(superBlock);
  free(groupBlock);
  free(blockBitmap);
  free(inodeBitmap);
  
}

void error(char* msg)
{
  fprintf(stderr, "%s\n", msg);
  exit(2);
}

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

void printFreeInodes() {
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

void printInodeSummaries(struct ext2_inode * inodes) {
  time_t rawTime = (time_t) inodes->i_atime;
  char timeString[100];
  strftime(timeString, 100, "%m/%d/%y %l:%M:%S %p GMT", gmtime(&rawTime));
  fprintf(stdout, "%s\n", timeString);
}

struct ext2_dir_entry * entry;
struct ext2_inode * inode;

void printDirectoryEntry(struct ext2_dir_entry * entry, unsigned long long int byteOffset) {
  unsigned long long int fileInode = entry->inode;
  unsigned long long int entryLength = entry->rec_len;
  unsigned long long int fileType = entry->file_type;
  char fileName[EXT2_NAME_LEN+1];
  memcpy(fileName, entry->name, strlen(entry->name));
  fileName[strlen(entry->name)] = '\0';
  unsigned long long int fileNameLength = strlen(fileName);
  fprintf(stdout, "DIRENT,%llu,%llu,%llu,%llu,%s\n", byteOffset,fileInode, entryLength, fileNameLength, fileName);
}

void printDirectoryEntries(struct ext2_inode * inodes) {
  unsigned char block[BUF_SIZE];
  struct ext2_dir_entry * entry;
  
  int i, j;
  int size = 0;
  int blockNum = 0;
  unsigned long long int byteOffset = 0;
  for (i = 0; i < numberOfInodes; i++) {
    inode = inodes+i;
    if (getFileType((long long unsigned int) inode->i_mode) == 'd') {
      pread(fd, block, BUF_SIZE, inode->i_block[blockNum]*BUF_SIZE);
      entry = (struct ext2_dir_entry *) block;
      printf("Size: %llu\n", (long long unsigned int) inode->i_size);
      printf("Block count: %llu\n", (long long unsigned int) inode->i_blocks);

      for (size = 0; size < EXT2_NDIR_BLOCKS; size++) {
      	if (entry->inode != 0)
	  printDirectoryEntry(entry, byteOffset);
	byteOffset += entry->rec_len;
	entry = (void*) entry + entry->rec_len;
      }

      size = 0;
      byteOffset = 0;
    }
  }
}

int
main (int argc, char **argv)
{
  // File system image name is in argv
  char * img = argv[1];
  fd = open(img, O_RDONLY);
  if (fd == -1)
    error("Unable to read file system image");

  int blockToRead;
  
  superBlock = (struct ext2_super_block *) malloc (BUF_SIZE);
  if (pread(fd, superBlock, BUF_SIZE, BUF_SIZE) == -1)
    error("Unable to pread from superblock");
  if (superBlock->s_magic != EXT2_SUPER_MAGIC)
    error("Bad file system");
  
  printSuperblock();

  groupBlock = (struct ext2_group_desc *) malloc (BUF_SIZE);
  if (pread(fd, groupBlock, BUF_SIZE, BUF_SIZE*2) == -1)
    error("Unable to pread from group block");

  printGroupBlock();

  blockBitmap = malloc (BUF_SIZE);
  pread(fd, blockBitmap, BUF_SIZE, BUF_SIZE*3);
  printFreeBlocks();

  inodeBitmap = malloc (BUF_SIZE);
  pread(fd, inodeBitmap, BUF_SIZE, BUF_SIZE*4);
  printFreeInodes();

  unsigned int inodesPerBlock = BUF_SIZE/sizeof(struct ext2_inode);
  numberOfInodes = (long long unsigned int) superBlock->s_inodes_per_group;
  unsigned int inodeTableBlocks = numberOfInodes / inodesPerBlock;

  blockToRead = 5;
  struct ext2_inode inodes[numberOfInodes];
  pread(fd, inodes, BUF_SIZE*inodeTableBlocks, BUF_SIZE*blockToRead);
  printInodeSummaries(inodes);
  //  printDirectoryEntries(inodes);
  
  return 0;
}
