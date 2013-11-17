#include "inode_manager.h"
#include <time.h>

// Bitmap ------------------------------------
Bitmap::Bitmap() {
  numBits = 0;
  numChar = 0;
}

Bitmap::Bitmap(uint32_t nBits) {
  numBits = nBits;
  numChar = (nBits + 7)/8;
  map = (char*) malloc(numChar);
  bzero(map, numChar);
}

Bitmap::~Bitmap() {
  delete map;
}

void Bitmap::mark(uint32_t which) {
  if(which < 0 || which >= numBits)
    return;

  map[which/8] |= (1 << (which % 8));
}

void Bitmap::clear(uint32_t which) {
  if(which < 0 || which >= numBits)
    return;

  map[which/8] &= (~(1 << (which % 8)));
}

bool Bitmap::test(uint32_t which) {
  if(which < 0 || which >= numBits)
    return false;

  return map[which/8] & (1 << (which % 8));
}

uint32_t Bitmap::find() {
  uint32_t i,j;

  for(i = 0;i < numChar;i++) {
    for(j = 0;j < 8;j++) {
      if((map[i] & (1 << j)) == 0) {
        if(i*8+j < numBits)
          return i*8+j;
        else
          return 0;
      }
    }
  }
  return 0;
}

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your lab1 code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */

  uint32_t i = block_bitmap->find(); 
  block_bitmap->mark(i);  
  return i; 
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your lab1 code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  
  block_bitmap->clear(id);
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();
  block_bitmap = new Bitmap(BLOCK_NUM);
  
  block_bitmap->mark(0);// for boot sector
  block_bitmap->mark(1);// for super block

  // for block bitmap;
  for(int i = 0;i < (BLOCK_NUM + BPB - 1)/BPB;i++) {
    block_bitmap->mark(2+i);
  }

  // for inodetable
  // the macro IBLOCK is so confusing 
  for(uint32_t i = 0;i <= (INODE_NUM + IPB - 1)/IPB;i++) {
    block_bitmap->mark( 2 + (BLOCK_NUM + BPB - 1) / BPB + i);
  }

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
//  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  struct inode *ino = (struct inode *) malloc(sizeof(struct inode));
  pthread_mutex_init(&mp, NULL);
  bzero(ino, sizeof(struct inode));
  ino->size = 0;
  ino->type = extent_protocol::T_DIR;
  ino->ctime = time(NULL);
  ino->mtime = 0;
  ino->atime = 0;
  put_inode(1, ino);
  free(ino);
/*  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }*/
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your lab1 code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  int i;
  struct inode *ino;
  struct inode *tmp;

  // too slow. A better way is add a inode bitmap.
/*  for(i = 1;i < INODE_NUM;i++) {
    if( (tmp = get_inode(i)) == NULL) {
      break;
    } else {
      free(tmp);
    }
  }*/

  pthread_mutex_lock(&mp); 
 /* for(i = 1;i < INODE_NUM;i++) {
    if( (tmp = get_inode(i)) == NULL) {
      break;
    } else {
      free(tmp);
    }
  }*/

  while(1) {
    //i = time(NULL) % 1022 + 2;
    i=1+(int)(1023.0*rand()/(RAND_MAX+1.0));
    if( (tmp = get_inode(i)) == NULL) {
      printf("alloc inode num:%d\n",i);
      break;
    } else {
      free(tmp);
    }
  }
  
  if(i < INODE_NUM) {
    ino = (struct inode *) malloc(sizeof(struct inode));
    bzero(ino, sizeof(struct inode));
    ino->size = 0;
    ino->type = type;
    ino->ctime = time(NULL);
    ino->mtime = 0;
    ino->atime = 0;
    put_inode(i, ino);
    pthread_mutex_unlock(&mp);
    free(ino);
    return i;
  } else {
    pthread_mutex_unlock(&mp);
    return 0;
  }
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your lab1 code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */

  struct inode *ino;


  
  ino = get_inode(inum);

  if(ino != NULL) {
    bzero(ino, sizeof(struct inode));
    ino->type = 0;
    ino->ctime = 0;
    ino->mtime = 0;
    ino->atime = 0;
    pthread_mutex_lock(&mp);
    put_inode(inum, ino);
    pthread_mutex_unlock(&mp);
    free(ino);
  }

  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}



/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your lab1 code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */
  
  struct inode *ino;
  char *indirect_block;
  uint32_t direct;
  unsigned int read_size = 0;
  unsigned int blocks_num;
  char buf[BLOCK_SIZE];
  int flag = 0;

  if(buf_out == NULL)
    return;

  ino = get_inode(inum);
  if(ino == NULL) {
    buf_out = NULL;
    *size = 0;
    return;
  }


  *size = ino->size;
  *buf_out =(char*) malloc(ino->size + 1);

  //direct block
  for(blocks_num = 0;blocks_num < NDIRECT;blocks_num++) {
    if(read_size >= ino->size) {
      flag = 1;
      break;
    }
    bm->read_block(ino->blocks[blocks_num], buf);
    memcpy(*buf_out + read_size, buf, 
      MIN(BLOCK_SIZE, ino->size - read_size));
    read_size += BLOCK_SIZE;
  }


  //indrect block
  if(flag == 0 && read_size < ino->size) {
    indirect_block = (char*) malloc(BLOCK_SIZE);
    bm->read_block(ino->blocks[NDIRECT], indirect_block);
    blocks_num = 0;
    do{
      direct = *(((uint32_t*)indirect_block) + blocks_num);
      bm->read_block(direct, buf); 
      memcpy(*buf_out + read_size, buf,
        MIN(BLOCK_SIZE, ino->size - read_size));
      blocks_num++;
      read_size += BLOCK_SIZE;
    } while(read_size < ino->size);
    free(indirect_block);
  }

  *(*buf_out + ino->size) = '\0'; 
  ino->atime = time(NULL);
  ino->ctime = time(NULL);
  put_inode(inum, ino);
  free(ino);
  return;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your lab1 code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  
  struct inode *ino;
  uint32_t origin_block_num, cur_block_num;
  uint32_t i;

  ino = get_inode(inum);
  if(ino == NULL)
    return;
  origin_block_num = BLOCK_COUNT(ino->size);
  cur_block_num = BLOCK_COUNT(size);

  // direct
  for(i = 0;i < NDIRECT;i++) {
    if(i < cur_block_num && i < origin_block_num) {
      bm->write_block(ino->blocks[i], buf+i*BLOCK_SIZE);
    } else if(i < cur_block_num) {
      ino->blocks[i] = bm->alloc_block();
      bm->write_block(ino->blocks[i], buf+i*BLOCK_SIZE);
    } else {
      break;
    }
  }

  // indirect
  if(cur_block_num > NDIRECT) {
    uint32_t indirect_block[NINDIRECT];
    if(origin_block_num > NDIRECT) {
      bm->read_block(ino->blocks[NDIRECT], (char*) indirect_block);
    } else {
      ino->blocks[NDIRECT] = bm->alloc_block();
      bzero((char*)indirect_block, sizeof(uint32_t)*NINDIRECT);
    }

    for(i = NDIRECT;i < NINDIRECT + NDIRECT;i++) {
      if(i < cur_block_num && i < origin_block_num) {
        bm->write_block(indirect_block[i - NDIRECT], buf+i*BLOCK_SIZE);
      } else if(i < cur_block_num) {
        indirect_block[i - NDIRECT] = bm->alloc_block();
        bm->write_block(indirect_block[i - NDIRECT], buf+i*BLOCK_SIZE);
      } else {
        bm->write_block(ino->blocks[NDIRECT], (char*) indirect_block);
        break;
      }
    }
  }

/*
  // block free when new file is smaller than the origin one
  if( origin_block_num > cur_block_num) {
    for(i = cur_block_num;i < NDIRECT && i < origin_block_num;i++) {
      bm->free_block(ino->blocks[i]);
    }
    if(origin_block_num > NDIRECT) {
      uint32_t indirect_block[NINDIRECT];
      bm->read_block(ino->blocks[NDIRECT], (char*) indirect_block);
      for(i = 0;i < origin_block_num - NDIRECT;i++) {
        bm->free_block(indirect_block[i]);
      }
      bm->free_block(ino->blocks[NDIRECT]);
    }
  }
*/
  ino->size = size;
  ino->mtime = time(NULL);
  ino->ctime = time(NULL);
  put_inode(inum, ino);

  free(ino);
  return;
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your lab1 code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  
  struct inode* ino;

  ino = get_inode(inum);
  if(ino == NULL)
    return;

  a.type = ino->type;
  a.atime = ino->atime;
  a.ctime = ino->ctime;
  a.mtime = ino->mtime;
  a.size = ino->size;

  free(ino);   
  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your lab1 code goes here
   * note: you need to consider about both the data block and inode of the file
   */

  struct inode *ino;
  uint32_t block_num;
  
  ino = get_inode(inum);
  if(ino == NULL) 
    return;

  block_num = BLOCK_COUNT(ino->size);

  for(uint32_t i = 0;i < NDIRECT && i < block_num;i++) {
    bm->free_block(ino->blocks[i]);
  }

  if(block_num > NDIRECT) {
    uint32_t indirect_block[NINDIRECT];
    bm->read_block(ino->blocks[NDIRECT], (char*) indirect_block);
    for(uint32_t i = 0;i < block_num - NDIRECT && i < NINDIRECT;i++) {
      bm->free_block(indirect_block[i]);
    }
    bm->free_block(ino->blocks[NDIRECT]);
  }
  
  free_inode(inum);
  free(ino);
  return;
}
