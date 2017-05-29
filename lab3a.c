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
unsigned char * bitmap;

int fd;

void freeMemory()
{
  free(superBlock);
  free(groupBlock);
  free(bitmap);
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

void printFreeBlockEntries()
{
  bitmap = (unsigned char *) malloc(BUF_SIZE);
  if (pread(fd, bitmap, BUF_SIZE, freeBlockBitmapNum) == -1)
    error("Unable to pread bitmap");
  
  printf("%s\n", bitmap[0]);
}

int
main (int argc, char **argv)
{
  // File system image name is in argv
  char * img = argv[1];
  fd = open(img, O_RDONLY);
  if (fd == -1)
    error("Unable to read file system image");
  
  superBlock = (struct ext2_super_block *) malloc (BUF_SIZE);
  if (pread(fd, superBlock, BUF_SIZE, BUF_SIZE) == -1)
    error("Unable to pread from superblock");

  printSuperblock();

  groupBlock = (struct ext2_group_desc *) malloc (BUF_SIZE);
  if (pread(fd, groupBlock, BUF_SIZE, BUF_SIZE*2) == -1)
    error("Unable to pread from group block");

  printGroupBlock();

  printFreeBlockEntries();

  return 0;
}
