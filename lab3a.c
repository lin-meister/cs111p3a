#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "ext2_fs.h"

#define BUF_SIZE 1024

struct ext2_super_block * superBlock;
int fd;

void freeMemory()
{
  free(superBlock);
}

void error(char* msg)
{
  fprintf(stderr, "%s\n", msg);
  exit(1);
}

void printSuperblock()
{
  long long unsigned int blocksCount = (long long unsigned int) superBlock->s_blocks_count;
  long long unsigned int inodesCount = (long long unsigned int) superBlock->s_inodes_count;
  fprintf(stdout, "SUPERBLOCK,%llu,%llu,%llu\n", blocksCount, inodesCount);
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
  return 0;
}
