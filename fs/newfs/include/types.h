#ifndef _TYPES_H_
#define _TYPES_H_

typedef int          boolean;
typedef uint16_t     flag16;

typedef enum NFS_file_type {
    NFS_REG_FILE,
    NFS_DIR,
    NFS_SYM_LINK
} NFS_FILE_TYPE;

#define TRUE                    1
#define FALSE                   0
#define UINT32_BITS             32
#define UINT8_BITS              8

#define NFS_MAGIC_NUM           0x8888
#define NFS_SUPER_OFS           0
#define NFS_ROOT_INO            0


#define NFS_ERROR_NONE          0
#define NFS_ERROR_ACCESS        EACCES
#define NFS_ERROR_SEEK          ESPIPE     
#define NFS_ERROR_ISDIR         EISDIR
#define NFS_ERROR_NOSPACE       ENOSPC
#define NFS_ERROR_EXISTS        EEXIST
#define NFS_ERROR_NOTFOUND      ENOENT
#define NFS_ERROR_UNSUPPORTED   ENXIO
#define NFS_ERROR_IO            EIO     /* Error Input/Output */
#define NFS_ERROR_INVAL         EINVAL  /* Invalid Args */

#define NFS_MAX_FILE_NAME       128
#define NFS_INODE_PER_FILE      1
#define NFS_DATA_PER_FILE       6
#define NFS_DEFAULT_PERM        0777

#define NFS_IOC_MAGIC           'S'
#define NFS_IOC_SEEK            _IO(NFS_IOC_MAGIC, 0)

#define NFS_FLAG_BUF_DIRTY      0x1
#define NFS_FLAG_BUF_OCCUPY     0x2
/******************************************************************************
* SECTION: Macro Function
*******************************************************************************/
#define NFS_IO_SZ()                     (nfs_super.sz_io)       // 512 B
#define NFS_BLK_SZ()                    (nfs_super.sz_blk)      // 1024 B
#define NFS_DISK_SZ()                   (nfs_super.sz_disk)
#define NFS_DRIVER()                    (nfs_super.driver_fd)

#define NFS_ROUND_DOWN(value, round)    (value % round == 0 ? value : (value / round) * round)
#define NFS_ROUND_UP(value, round)      (value % round == 0 ? value : ((value / round) + 1) * round)

#define NFS_BLKS_SZ(blks)               (blks * NFS_BLK_SZ())
#define NFS_ASSIGN_FNAME(pnfs_dentry, _fname) memcpy(pnfs_dentry->fname, _fname, strlen(_fname))
#define NFS_INO_OFS(ino)                (nfs_super.inode_offset + (ino) * NFS_BLK_SZ())
#define NFS_DATA_OFS(data_blk)          (nfs_super.data_offset + (data_blk) * NFS_BLK_SZ())

#define NFS_IS_DIR(pinode)              (pinode->dentry->ftype == NFS_DIR)
#define NFS_IS_REG(pinode)              (pinode->dentry->ftype == NFS_REG_FILE)
#define NFS_IS_SYM_LINK(pinode)         (pinode->dentry->ftype == NFS_SYM_LINK)
/******************************************************************************
* SECTION: FS Specific Structure - In memory structure
*******************************************************************************/
struct nfs_dentry;
struct nfs_inode;
struct nfs_super;

struct custom_options {
	const char* device;
	boolean     show_help;
};

struct nfs_super
{
    int         driver_fd;
    boolean     is_mounted;        
    int         sz_disk;            // ????????????: 4 MB
    int         sz_usage;
    int         sz_io;              // ?????????: 512 B
    int         sz_blk;             // ???????????????: 1024 B

    int         num_ino;            // inode??????
    int         num_data;           // data ?????? = inode??? * NFS_DATA_PER_FILE
    
    uint8_t*    map_inode;          // inode??????
    int         map_inode_blks;     // inode?????????????????????
    int         map_inode_offset;   // inode??????????????????

    uint8_t*    map_data;           // data??????
    int         map_data_blks;      // data?????????????????????
    int         map_data_offset;    // data??????????????????

    int         inode_offset;       // inode???????????????
    int         data_offset;        // ????????????????????????

    struct nfs_dentry* root_dentry; // ?????????
};
struct nfs_inode
{
    int                ino;         // ???inod???????????????
    int                size;        // ?????????????????????
    int                dir_cnt;     // ????????????????????????????????????
    int                p_blk[NFS_DATA_PER_FILE];    //???????????????
    struct nfs_dentry* dentry;      // ?????????inode???dentry
    struct nfs_dentry* dentrys;     // ??????????????? 
    uint8_t*           data[NFS_DATA_PER_FILE];
    char               target_path[NFS_MAX_FILE_NAME];   // store traget path when it is a symlink
};  

struct nfs_dentry
{
    char               fname[NFS_MAX_FILE_NAME];
    struct nfs_dentry* parent;                        /* ??????Inode???dentry */
    struct nfs_dentry* brother;                       /* ?????? */
    int                ino;
    struct nfs_inode*  inode;                         /* ??????inode */
    NFS_FILE_TYPE      ftype;
};

static inline struct nfs_dentry* new_dentry(char * fname, NFS_FILE_TYPE ftype) {
    struct nfs_dentry * dentry = (struct nfs_dentry *)malloc(sizeof(struct nfs_dentry));
    memset(dentry, 0, sizeof(struct nfs_dentry));
    NFS_ASSIGN_FNAME(dentry, fname);
    dentry->ftype   = ftype;
    dentry->ino     = -1;
    dentry->inode   = NULL;
    dentry->parent  = NULL;
    dentry->brother = NULL;   
    return dentry;                                         
}
/******************************************************************************
* SECTION: FS Specific Structure - Disk structure
*******************************************************************************/
struct nfs_super_d
{
    uint32_t magic_num;
    int      sz_usage;
    int      num_ino;            // inode???
    int      map_inode_blks;     // inode??????????????????
    int      map_inode_offset;   // inode???????????????????????????

    int      map_data;           // data??????
    int      map_data_blks;      // data?????????????????????
    int      map_data_offset;    // data???????????????????????????

    int      inode_offset;       // inode???????????????
    int      data_offset;        // ????????????????????????
};

struct nfs_inode_d
{
    int             ino;             // ???inode??????????????????
    int             size;            // ?????????????????????
    int             link;            // ?????????
    int             dir_cnt;         // ???????????????????????????????????????????????????
    int             p_blk[NFS_DATA_PER_FILE];      // ???????????????
    NFS_FILE_TYPE   ftype;
    char            target_path[NFS_MAX_FILE_NAME];// store traget path when it is a symlink   
};  

struct nfs_dentry_d
{
    int             ino;
    boolean         valid;
    NFS_FILE_TYPE   ftype;
    char            fname[NFS_MAX_FILE_NAME];
};  


#endif /* _TYPES_H_ */