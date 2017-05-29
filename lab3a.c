#include <math.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "ext2_fs.h"

#define BUF_SIZE 1024

struct ext2_super_block * superBlock;
struct ext2_group_desc * groupBlock;
unsigned char *blockBitmap;
unsigned char *inodeBitmap;

int fd;

void freeMemory()
{
  free(superBlock);
  free(groupBlock);
  free(blockBitmap);
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

void printGroupBlock() {
  int blockNum = 0;
  long long unsigned int totalNumofBlocks = (long long unsigned int) superBlock->s_blocks_count;
  long long unsigned int totalNumofInodes = (long long unsigned int) superBlock->s_inodes_count;
  long long unsigned int freeBlocks = (long long unsigned int) groupBlock->bg_free_blocks_count;
  long long unsigned int freeInodes = (long long unsigned int) groupBlock->bg_free_inodes_count;
  long long unsigned int freeBlockBitmapNum = (long long unsigned int) groupBlock->bg_block_bitmap;
  long long unsigned int freeInodeBitmapNum = (long long unsigned int) groupBlock->bg_inode_bitmap;
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

int
main (int argc, char **argv)
{
  // File system image name is in argv
  char * img = argv[1];
  fd = open(img, O_RDONLY);
  if (fd == -1)
    error("Unable to open disk image");
  
  superBlock = (struct ext2_super_block *) malloc (BUF_SIZE);
  pread(fd, superBlock, BUF_SIZE, BUF_SIZE);

  printSuperblock();

  groupBlock = (struct ext2_group_desc *) malloc (BUF_SIZE);
  pread(fd, groupBlock, BUF_SIZE, BUF_SIZE*2);
  printGroupBlock();

  blockBitmap = malloc (BUF_SIZE);
  pread(fd, blockBitmap, BUF_SIZE, BUF_SIZE*3);
  printFreeBlocks();

  inodeBitmap = malloc (BUF_SIZE);
  pread(fd, inodeBitmap, BUF_SIZE, BUF_SIZE*4);
  printFreeInodes();
  
  return 0;
}
