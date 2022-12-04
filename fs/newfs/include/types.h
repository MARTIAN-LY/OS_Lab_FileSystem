#ifndef _TYPES_H_
#define _TYPES_H_

typedef int  boolean;
typedef enum nfs_file_type {
    REG_FILE,
    DIR,
    SYM_LINK
} FILE_TYPE;

#define TRUE            1
#define FALSE           0
#define ERROR_NONE      0
#define UINT32_BITS     32
#define UINT8_BITS      8

#define MAGIC_NUM       8888   // TODO: Define by yourself
#define DEFAULT_PERM    0777   // 全权限打开 
#define SUPER_OFFSET    0
#define ROOT_INO        0

#define MAX_NAME_LEN    128
#define INODE_PER_FILE  1
#define DATA_PER_FILE   16


#define IOC_SEEK         _IO(IOC_MAGIC, 0)

#define FLAG_BUF_DIRTY   0x1
#define FLAG_BUF_OCCUPY  0x2

#define ERROR_SEEK          ESPIPE     
#define ERROR_NOSPACE       ENOSPC
#define ERROR_NOTFOUND      ENOENT
#define ERROR_UNSUPPORTED   ENXIO

/******************************************************************************
* SECTION: Macro Function
*******************************************************************************/
#define IO_SZ()                     (nfs_super.sz_io)
#define DISK_SZ()                   (nfs_super.sz_disk)
#define DRIVER()                    (nfs_super.driver_fd)

#define ROUND_DOWN(value, round)    (value % round == 0 ? value : (value / round) * round)
#define ROUND_UP(value, round)      (value % round == 0 ? value : (value / round + 1) * round)

#define BLKS_SZ(blks)               (blks * NFS_IO_SZ())
#define ASSIGN_FNAME(pNFS_dentry, _fname)\
                                    memcpy(pNFS_dentry->fname, _fname, strlen(_fname))
#define INO_OFS(ino)                (NFS_super.data_offset + ino * NFS_BLKS_SZ((\
                                        NFS_INODE_PER_FILE + NFS_DATA_PER_FILE)))
#define DATA_OFS(ino)               (NFS_INO_OFS(ino) + NFS_BLKS_SZ(NFS_INODE_PER_FILE))

#define IS_DIR(pinode)              (pinode->dentry->ftype == DIR)
#define IS_REG(pinode)              (pinode->dentry->ftype == REG_FILE)
#define IS_SYM_LINK(pinode)         (pinode->dentry->ftype == SYM_LINK)


struct custom_options;
struct nfs_super;
struct nfs_inode;
struct nfs_dentry;
struct disk_spuer;
struct disk_inode;
struct disk_dentry;

struct custom_options
{
	const char* device;      //驱动路径
    boolean     show_help;
};

// 内存中的super block
struct nfs_super 
{
    int         magic;
    int         driver_fd;
    boolean     is_mounted;         
    int         sz_disk;            // 磁盘大小
    int         sz_usage;
    int         sz_io;              // inode的大小

    uint8_t*    map_data;           // data位图
    int         map_data_blocks;    // data位图占用的块数
    int         map_data_offset;    // data位图起始地址

    int         max_ino;            // inode数目
    uint8_t*    map_inode;          // inode位图
    int         map_inode_blks;     // inode位图占用的块数
    int         map_inode_offset;   // inode位图起始地址

    int         data_offset;        // 数据块的起始地址

    struct nfs_dentry* root_dentry; // 根目录
};

// 内存中的inode
struct nfs_inode {
    int                ino;    // 在inod位图的索引
    int                size;   // 文件已占用大小
    int                link;
    int                dir_cnt;           // 如果是目录，其下的目录项
    struct nfs_dentry* dentry;     // 指向该inode的dentry
    struct nfs_dentry* dentrys;    // 所有目录项 
    uint8_t*           data;
    char               target_path[MAX_NAME_LEN];// store traget path when it is a symlink
    FILE_TYPE          ftype;
};

// 内存中的目录项
struct nfs_dentry {
    char               fname[MAX_NAME_LEN];
    FILE_TYPE          ftype;
    int                ino;     // 指向的inode        
    struct nfs_dentry* parent;  // 父亲Inode的dentry 
    struct nfs_dentry* brother; // 兄弟 
    struct nfs_inode*  inode;   // 指向inode 

};

// 磁盘中的super_block
struct disk_super
{
    int     magic_num;
    int     sz_usage;
    int     max_ino;            // inode数
    int     map_inode_blks;     // inode位图占的块数
    int     map_inode_offset;   // inode位图在磁盘上的偏移

    int     map_data;           // data位图
    int     map_data_blocks;    // data位图占用的块数
    int     map_data_offset;    // data位图在磁盘上的偏移

    int     data_offset;        // 数据块的起始地址
};

struct disk_inode
{
    int       ino;             // 在inode位图中的下标
    int       size;            // 文件已占用空间
    int       dir_cnt;         // 如果是目录型文件，下面有几个目录项
    int       p_blk[6];        // 数据块指针
    char      target_path[MAX_NAME_LEN];// store traget path when it is a symlink
    FILE_TYPE ftype;
};

struct disk_dentry
{
    int         ino;
    boolean     valid;
    FILE_TYPE   ftype;
    char        fname[MAX_NAME_LEN];
};


static inline struct nfs_dentry* new_dentry(char * fname, FILE_TYPE ftype) {
    struct nfs_dentry * dentry = (struct nfs_dentry *)malloc(sizeof(struct nfs_dentry));
    memset(dentry, 0, sizeof(struct nfs_dentry));
    ASSIGN_FNAME(dentry, fname);
    dentry->ftype   = ftype;
    dentry->ino     = -1;
    dentry->inode   = NULL;
    dentry->parent  = NULL;
    dentry->brother = NULL;      

    return dentry;                                      
}

#endif /* _TYPES_H_ */