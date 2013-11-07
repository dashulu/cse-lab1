#include "inode_manager.h"

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
  blockid_t start = IBLOCK(INODE_NUM, BLOCK_NUM) + 1;
  char buf[BLOCK_SIZE];

  // the start block's bit position may not be BPB-alignment, so we need to handle it differently
  read_block(BBLOCK(start), buf);
  for (uint32_t j = start; j < start / BPB + BPB; j++) {
    uint32_t b_num = j / 8;
    uint8_t b_offset = j % 8;
    bool flag = (buf[b_num] << b_offset) & 0x80;
    if (!flag) {
      buf[b_num] |= (0x1 << (7 - b_offset));
      write_block(BBLOCK(start), buf);
      return j;
    }
  }

  for (blockid_t i = start / BPB + BPB; i < BLOCK_NUM; i += BPB) {
    read_block(BBLOCK(i), buf);
    // byte scanning
    for (uint32_t j = 0; j < BLOCK_SIZE; j++) {
      if ((uint8_t) buf[j] != 0xFF) {
        uint8_t map = (uint8_t) buf[j];
        uint32_t offset = 0;
        while (map & 0x1) {
          map >>= 1;
          offset++;
        }
        buf[j] |= (0x1 << offset);
        write_block(BBLOCK(i), buf);
        return i + j * 8 + offset;
      }
    }
  }

  printf("im: data block exhausted\n");
  return 0;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your lab1 code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  blockid_t start = IBLOCK(INODE_NUM, BLOCK_NUM) + 1;
  if (id < start) {
    printf("bm: try to free non-data blocks\n");
    return;
  }
  char buf[BLOCK_SIZE];
  read_block(BBLOCK(id), buf);
  uint32_t bi = id % BPB / 8; // byte index
  uint32_t bo = id % BPB % 8; // byte offset
  buf[bi] &= (0xFF - (0x1 << (7 - bo)));
  write_block(BBLOCK(id), buf);
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

  printf("bootblock:\t 1\n");
  printf("superblock:\t 1\n");
  printf("BPB:\t %d\n", BPB);
  printf("block bitmap:\t %d\n", BLOCK_NUM / BPB);
  printf("IPB:\t %lu\n", IPB);
  printf("inode table:\t %lu\n", INODE_NUM / IPB + 1);
  printf("data block:\t ....\n");

  write_block(0, (char *)&sb);
  printf("inode 1 is at block %lu\n", IBLOCK(1, BLOCK_NUM));
  printf("inode 2 is at block %lu\n", IBLOCK(2, BLOCK_NUM));
  printf("inode 3 is at block %lu\n", IBLOCK(3, BLOCK_NUM));
  printf("inode %d is at block %lu\n", INODE_NUM, IBLOCK(INODE_NUM, BLOCK_NUM));
  printf("block 0 bit: %d\n", BBLOCK(0));
  printf("block 4095 bit: %d\n", BBLOCK(4095));
  printf("block 4096 bit: %d\n", BBLOCK(4096));
  printf("block %d bit: %d\n", BLOCK_NUM - 1, BBLOCK(BLOCK_NUM - 1));
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
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
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
  char buf[BLOCK_SIZE];
  struct inode *next;
  for (uint32_t i = 1; i <= INODE_NUM - IPB + 1; i += IPB) {
    bm->read_block(IBLOCK(i, bm->sb.nblocks), buf);
    for (uint32_t j = 0; j < IPB; j++) {
      next = (struct inode *) buf + j;
      // Forget about the block bitmap!
      // Use type in inode to decide whether it is free
      if (next->type == 0) {
        next->type = type;
        // flush changes to disk
        bm->write_block(IBLOCK(i, bm->sb.nblocks), buf);
        printf("\tim: alloc_inode: %d\n", i + j);
        return i + j;
      }
    }
  }
  printf("\tim: inode exhausted\n");
  // return an impossible number to indicate an error
  return INODE_NUM + 1;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your lab1 code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */
  struct inode *ino = get_inode(inum);
  if (ino == NULL){
    printf("\tim: inode not exist\n");
    return;
  }

  if (0 < ino->size && ino->size <= NDIRECT * BLOCK_SIZE) {
    for (uint32_t i = 0; i * BLOCK_SIZE < ino->size; i++) {
      assert(ino->blocks[i]);
      bm->free_block(ino->blocks[i]);
      ino->blocks[i] = 0;
    }
  }

  else if (ino->size != 0) {
    for (uint32_t i = 0; i < NDIRECT; i++) {
      assert(ino->blocks[i]);
      bm->free_block(ino->blocks[i]);
      ino->blocks[i] = 0;
    }
    char indbuf[BLOCK_SIZE];
    bm->read_block(ino->blocks[NDIRECT], indbuf);
    uint32_t *indblks = (uint32_t *) indbuf;
    uint32_t j = 0;
    for (; j * BLOCK_SIZE + NDIRECT * BLOCK_SIZE < ino->size; j++) {
      assert(indblks[j]);
      bm->free_block(indblks[j]);
      indblks[j] = 0;
    }
    bm->free_block(ino->blocks[NDIRECT]);
    ino->blocks[NDIRECT] = 0;
  }

  ino->type = 0;
  ino->size = 0;
  put_inode(inum, ino);
  printf("\tim: free_inode: %d\n", inum);
  free(ino);
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum <= 0 || inum > INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);

  ino_disk = (struct inode*)buf + (inum - 1) % IPB;
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
  ino_disk = (struct inode*)buf + (inum - 1) % IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *ssize)
{
  /*
   * your lab1 code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_out
   */
  // dirty hack to depress type conversion warnings
  uint32_t *size = (uint32_t *) ssize;
  struct inode *ino = get_inode(inum);
  if (ino == NULL) {
    printf("\tin: inode not exist\n");
    *size = 0;
    return;
  }

  *size = ino->size;
  char *out = (char *) malloc(sizeof(char) * ino->size);
  *buf_out = out;

  if (*size == 0)
    *size = 0;
  else if (0 < *size && *size <= NDIRECT * BLOCK_SIZE) {

    char buf[BLOCK_SIZE];
    uint32_t i = 0;
    for (; i < *size / BLOCK_SIZE; i++) {
      bm->read_block(ino->blocks[i], buf);
      memcpy(out, buf, BLOCK_SIZE);
      out += BLOCK_SIZE;
    }
    if (*size % BLOCK_SIZE) {
      bm->read_block(ino->blocks[i], buf);
      memcpy(out, buf, *size % BLOCK_SIZE);
    }

  } else {

    char buf[BLOCK_SIZE];
    for (uint32_t i = 0; i < NDIRECT; i++) {
      bm->read_block(ino->blocks[i], buf);
      memcpy(out, buf, BLOCK_SIZE);
      out += BLOCK_SIZE;
    }
    char indbuf[BLOCK_SIZE];
    bm->read_block(ino->blocks[NDIRECT], indbuf);
    uint32_t *indblks = (uint32_t *) indbuf;
    uint32_t j = 0;
    for (; j < (*size - NDIRECT * BLOCK_SIZE) / BLOCK_SIZE; j++) {
      bm->read_block(indblks[j], buf);
      memcpy(out, buf, BLOCK_SIZE);
      out += BLOCK_SIZE;
    }
    if ((*size - NDIRECT * BLOCK_SIZE) % BLOCK_SIZE) {
      bm->read_block(indblks[j], buf);
      memcpy(out, buf, (*size - NDIRECT * BLOCK_SIZE) % BLOCK_SIZE);
    }

  }
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int ssize)
{
  /*
   * your lab1 code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  // dirty hack to depress type conversion warnings
  uint32_t size = (uint32_t) ssize;
  struct inode *ino = get_inode(inum);
  if (ino == NULL) {
    printf("\tim: inode not exist\n");
    return;
  }

  // if size exceeds the limit, just drop the exceeding content
  if (size > MAXFILE * BLOCK_SIZE)
    printf("im: file size exceeds limit! (it will be truncated to meet the limit)\n");

  size = MIN(size, MAXFILE * BLOCK_SIZE);

  if (size <= NDIRECT * BLOCK_SIZE) {
    // copy blocks
    uint32_t i = 0;
    for (; i * BLOCK_SIZE < size; i++) {
      // already copied i * BLOCK_SIZE
      if (ino->blocks[i] == 0 && (ino->blocks[i] = bm->alloc_block()) == 0) {
        printf("\tim: fail to allocate new data block\n");
        free(ino);
        return;
      }
      bm->write_block(ino->blocks[i], buf + i * BLOCK_SIZE);
    }

    // free blocks
    for (; i < NDIRECT && ino->blocks[i]; i++) {
      assert(size < ino->size);
      bm->free_block(ino->blocks[i]);
      ino->blocks[i] = 0;
    }
    if (ino->blocks[NDIRECT]) {
      assert(i == NDIRECT);
      char indbuf[BLOCK_SIZE];
      bm->read_block(ino->blocks[NDIRECT], indbuf);
      uint32_t *indblks = (uint32_t *) indbuf;
      for (uint32_t j = 0; j < NINDIRECT && indblks[j]; j++) {
        bm->free_block(indblks[j]);
        indblks[j] = 0;
      }
      bm->free_block(ino->blocks[NDIRECT]);
      ino->blocks[NDIRECT] = 0;
    }
  }

  else {
    // copy blocks
    for (uint32_t i = 0; i < NDIRECT; i++) {
      if (ino->blocks[i] == 0 && (ino->blocks[i] = bm->alloc_block()) == 0) {
        printf("\tim: fail to allocate new data block\n");
        free(ino);
        return;
      }
      bm->write_block(ino->blocks[i], buf + i * BLOCK_SIZE);
    }

    if (ino->blocks[NDIRECT] == 0 && (ino->blocks[NDIRECT] = bm->alloc_block()) == 0) {
      printf("\tim: fail to allocate new data block\n");
      free(ino);
      return;
    }
    char indbuf[BLOCK_SIZE];
    bm->read_block(ino->blocks[NDIRECT], indbuf);
    uint32_t *indblks = (uint32_t *) indbuf;
    uint32_t j = 0;
    for (; j * BLOCK_SIZE + NDIRECT * BLOCK_SIZE < size; j++) {
      if (indblks[j] == 0 && (indblks[j] = bm->alloc_block()) == 0) {
        printf("\tim: fail to allocate new data block\n");
        free(ino);
        return;
      }
      bm->write_block(indblks[j], buf + j * BLOCK_SIZE + NDIRECT * BLOCK_SIZE);
    }
    bm->write_block(ino->blocks[NDIRECT], indbuf);

    // free blocks
    for (; j < NINDIRECT && indblks[j]; j++) {
      assert(size < ino->size);
      bm->free_block(indblks[j]);
      indblks[j] = 0;
    }
  }

  // update meta data
  ino->mtime = time(0);
  ino->ctime = time(0);
  ino->size = size;
  put_inode(inum, ino);
  free(ino);
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your lab1 code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  struct inode *ino = get_inode(inum);
  // TODO: how to convert reference (a) to pointer to make sure we cat use memcpy ?
  // memcpy((void *)a, ino, sizeof(extent_protocol::attr));
  if (ino != NULL) {
    a.type = ino->type;
    a.atime = ino->atime;
    a.mtime = ino->mtime;
    a.ctime = ino->ctime;
    a.size = ino->size;
    free(ino);
  }
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your lab1 code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  free_inode(inum);
}
