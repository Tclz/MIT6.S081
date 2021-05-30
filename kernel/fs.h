// On-disk file system format.
// Both the kernel and user programs use this header file.


#define ROOTINO  1   // root i-number
#define BSIZE 1024  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint magic;        // Must be FSMAGIC
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

#define FSMAGIC 0x10203040

#define NDIRECT 11  //11个直接块
#define NINDIRECT (BSIZE / sizeof(uint))  //1个singly-indirect block==>指向256个直接块
#define MAXFILE (NDIRECT + NINDIRECT + NINDIRECT * NINDIRECT) //加了1个doubly-indirect block后能支持的最大文件大小

// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+2];   // Data block addresses  //指向的数据块的地址
};

// Inodes per block. // 每个block有多少inodes
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i //确定给定的inode号在哪个block
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block  //每个block能装下多少bitmap的bit位数
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b // 给定bit序号确定在哪个bitmap block
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

//目录是一种inode类型为T_DIR的文件。该种文件包含的数据是由各个目录项构成的一个序列。
//每个目录项是一个dirent结构体(内部有一个名称和inode号,名称最多包含14个字符)
struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

