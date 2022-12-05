#include "../include/newfs.h"

extern struct nfs_super nfs_super;
extern struct custom_options nfs_options;

/**
 * @brief 获取文件名
 */
char *nfs_get_fname(const char *path)
{
    char ch = '/';
    char *q = strrchr(path, ch) + 1;
    return q;
}
/**
 * @brief 计算路径的层级
 * exm: /av/c/d/f
 * -> lvl = 4
 */
int nfs_calc_lvl(const char *path)
{
    // char* path_cpy = (char *)malloc(strlen(path));
    // strcpy(path_cpy, path);
    char *str = path;
    int lvl = 0;
    if (strcmp(path, "/") == 0)
    {
        return lvl;
    }
    while (*str != NULL)
    {
        if (*str == '/')
        {
            lvl++;
        }
        str++;
    }
    return lvl;
}

/**
 * 驱动读
 *
 * BLK_SZ = 2 * IO_SIZE
 */
int nfs_driver_read(int start, uint8_t *out_content, int size)
{
    // 对齐的时候按 BLK_SZ
    int start_aligned = NFS_ROUND_DOWN(start, NFS_BLK_SZ());
    int bias = start - start_aligned;
    int size_aligned = NFS_ROUND_UP((size + bias), NFS_BLK_SZ());
    uint8_t *temp_content = (uint8_t *)malloc(size_aligned);
    uint8_t *cur = temp_content;

    // 驱动读的时候按 IO_SZ
    ddriver_seek(NFS_DRIVER(), start_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        ddriver_read(NFS_DRIVER(), cur, NFS_IO_SZ());
        cur += NFS_IO_SZ();
        size_aligned -= NFS_IO_SZ();
    }
    memcpy(out_content, temp_content + bias, size);
    free(temp_content);
    return NFS_ERROR_NONE;
}

// 驱动写
int nfs_driver_write(int dst, uint8_t *in_content, int size)
{
    // 对齐的时候按 BLK_SZ
    int dst_aligned = NFS_ROUND_DOWN(dst, NFS_BLK_SZ());
    int bias = dst - dst_aligned;
    int size_aligned = NFS_ROUND_UP((size + bias), NFS_BLK_SZ());
    uint8_t *temp_content = (uint8_t *)malloc(size_aligned);
    uint8_t *cur = temp_content;
    nfs_driver_read(dst_aligned, temp_content, size_aligned);
    memcpy(temp_content + bias, in_content, size);

    // 驱动写的时候按 IO_SZ
    ddriver_seek(NFS_DRIVER(), dst_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        ddriver_write(NFS_DRIVER(), cur, NFS_IO_SZ());
        cur += NFS_IO_SZ();
        size_aligned -= NFS_IO_SZ();
    }

    free(temp_content);
    return NFS_ERROR_NONE;
}

// 为一个inode分配dentry，采用头插法
int nfs_alloc_dentry(struct nfs_inode *inode, struct nfs_dentry *dentry)
{
    if (inode->dentrys == NULL)
    {
        inode->dentrys = dentry;
    }
    else
    {
        dentry->brother = inode->dentrys;
        inode->dentrys = dentry;
    }
    inode->dir_cnt++;
    return inode->dir_cnt;
}

// 将dentry从inode的dentrys中取出
int nfs_drop_dentry(struct nfs_inode *inode, struct nfs_dentry *dentry)
{
    boolean is_find = FALSE;
    struct nfs_dentry *dentry_cursor;
    dentry_cursor = inode->dentrys;

    if (dentry_cursor == dentry)
    {
        inode->dentrys = dentry->brother;
        is_find = TRUE;
    }
    else
    {
        while (dentry_cursor)
        {
            if (dentry_cursor->brother == dentry)
            {
                dentry_cursor->brother = dentry->brother;
                is_find = TRUE;
                break;
            }
            dentry_cursor = dentry_cursor->brother;
        }
    }
    if (!is_find)
    {
        return -NFS_ERROR_NOTFOUND;
    }
    inode->dir_cnt--;
    return inode->dir_cnt;
}
/**
 * @brief 分配一个inode，占用位图
 *
 * @param dentry 该dentry指向分配的inode
 * @return nfs_inode
 */
struct nfs_inode *nfs_alloc_inode(struct nfs_dentry *dentry)
{
    struct nfs_inode *inode;
    int byte_cursor = 0;
    int bit_cursor = 0;
    int ino_cursor = 0;
    boolean is_find_free_entry = FALSE;

    // 从 inode 位图里找空闲
    for (byte_cursor = 0; byte_cursor < NFS_BLKS_SZ(nfs_super.map_inode_blks); byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
        {
            if ((nfs_super.map_inode[byte_cursor] & (0x1 << bit_cursor)) == 0)
            {
                /* 当前ino_cursor位置空闲 */
                nfs_super.map_inode[byte_cursor] |= (0x1 << bit_cursor);
                is_find_free_entry = TRUE;
                break;
            }
            ino_cursor++;
        }
        if (is_find_free_entry)
            break;
    }
    if (!is_find_free_entry || ino_cursor == nfs_super.num_ino)
        return -NFS_ERROR_NOSPACE;

    // 从 data 位图里找空闲
    int data_cursor = 0;
    int free_data = 0;
    int mark_blk[NFS_DATA_PER_FILE] = { 0 };
    for (byte_cursor = 0; byte_cursor < NFS_BLKS_SZ(nfs_super.map_data_blks); byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
        {
            if ((nfs_super.map_inode[byte_cursor] & (0x1 << bit_cursor)) == 0)
            {
                /* 当前data_cursor位置空闲 */
                nfs_super.map_data[byte_cursor] |= (0x1 << bit_cursor);
                mark_blk[free_data] = data_cursor;
                ++free_data;
            }
            // 找到所需 data 块
            if (free_data == NFS_DATA_PER_FILE)
            {
                is_find_free_entry = TRUE;
                break;
            }
            data_cursor++;
        }
        if (is_find_free_entry)
            break;
    }
    if (!is_find_free_entry || data_cursor == nfs_super.num_data)
        return -NFS_ERROR_NOSPACE;

    inode = (struct nfs_inode *)malloc(sizeof(struct nfs_inode));
    inode->ino = ino_cursor;
    inode->size = 0;
    for (int i = 0; i < NFS_DATA_PER_FILE; i++)
        inode->p_blk[i] = mark_blk[i];
    
    /* dentry指向inode */
    dentry->inode = inode;
    dentry->ino = inode->ino;
    /* inode指回dentry */
    inode->dentry = dentry;

    inode->dir_cnt = 0;
    inode->dentrys = NULL;

    if (NFS_IS_REG(inode))
    {
        inode->data = (uint8_t *)malloc(NFS_BLKS_SZ(NFS_DATA_PER_FILE));
    }

    return inode;
}
/**
 * @brief 将内存inode及其下方结构全部刷回磁盘
 */
int nfs_sync_inode(struct nfs_inode *inode)
{
    struct nfs_inode_d inode_d;
    struct nfs_dentry *dentry_cursor;
    struct nfs_dentry_d dentry_d;
    int ino = inode->ino;
    inode_d.ino = ino;
    inode_d.size = inode->size;
    memcpy(inode_d.target_path, inode->target_path, NFS_MAX_FILE_NAME);
    inode_d.ftype = inode->dentry->ftype;
    inode_d.dir_cnt = inode->dir_cnt;
    int offset;

    if (nfs_driver_write(NFS_INO_OFS(ino), (uint8_t *)&inode_d,
                         sizeof(struct nfs_inode_d)) != NFS_ERROR_NONE)
    {
        NFS_DBG("[%s] io error\n", __func__);
        return -NFS_ERROR_IO;
    }
    /* Cycle 1: 写 INODE */
    /* Cycle 2: 写 数据 */
    if (NFS_IS_DIR(inode))
    {
        dentry_cursor = inode->dentrys;
        offset = NFS_DATA_OFS(ino);
        while (dentry_cursor != NULL)
        {
            memcpy(dentry_d.fname, dentry_cursor->fname, NFS_MAX_FILE_NAME);
            dentry_d.ftype = dentry_cursor->ftype;
            dentry_d.ino = dentry_cursor->ino;
            if (nfs_driver_write(offset, (uint8_t *)&dentry_d,
                                 sizeof(struct nfs_dentry_d)) != NFS_ERROR_NONE)
            {
                NFS_DBG("[%s] io error\n", __func__);
                return -NFS_ERROR_IO;
            }

            if (dentry_cursor->inode != NULL)
            {
                nfs_sync_inode(dentry_cursor->inode);
            }

            dentry_cursor = dentry_cursor->brother;
            offset += sizeof(struct nfs_dentry_d);
        }
    }
    else if (NFS_IS_REG(inode))
    {
        if (nfs_driver_write(NFS_DATA_OFS(ino), inode->data,
                             NFS_BLKS_SZ(NFS_DATA_PER_FILE)) != NFS_ERROR_NONE)
        {
            NFS_DBG("[%s] io error\n", __func__);
            return -NFS_ERROR_IO;
        }
    }
    return NFS_ERROR_NONE;
}
/**
 * @brief 删除内存中的一个inode， 暂时不释放
 * Case 1: Reg File
 *
 *                  Inode
 *                /      \
 *            Dentry -> Dentry (Reg Dentry)
 *                       |
 *                      Inode  (Reg File)
 *
 *  1) Step 1. Erase Bitmap
 *  2) Step 2. Free Inode                      (Function of nfs_drop_inode)
 * ------------------------------------------------------------------------
 *  3) *Setp 3. Free Dentry belonging to Inode (Outsider)
 * ========================================================================
 * Case 2: Dir
 *                  Inode
 *                /      \
 *            Dentry -> Dentry (Dir Dentry)
 *                       |
 *                      Inode  (Dir)
 *                    /     \
 *                Dentry -> Dentry
 *
 *   Recursive
 * @param inode
 * @return int
 */
int nfs_drop_inode(struct nfs_inode *inode)
{
    struct nfs_dentry *dentry_cursor;
    struct nfs_dentry *dentry_to_free;
    struct nfs_inode *inode_cursor;

    int byte_cursor = 0;
    int bit_cursor = 0;
    int ino_cursor = 0;
    boolean is_find = FALSE;

    if (inode == nfs_super.root_dentry->inode)
    {
        return NFS_ERROR_INVAL;
    }

    if (NFS_IS_DIR(inode))
    {
        dentry_cursor = inode->dentrys;
        /* 递归向下drop */
        while (dentry_cursor)
        {
            inode_cursor = dentry_cursor->inode;
            nfs_drop_inode(inode_cursor);
            nfs_drop_dentry(inode, dentry_cursor);
            dentry_to_free = dentry_cursor;
            dentry_cursor = dentry_cursor->brother;
            free(dentry_to_free);
        }
    }
    else if (NFS_IS_REG(inode) || NFS_IS_SYM_LINK(inode))
    {
        for (byte_cursor = 0; byte_cursor < NFS_BLKS_SZ(nfs_super.map_inode_blks);
             byte_cursor++) /* 调整inodemap */
        {
            for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
            {
                if (ino_cursor == inode->ino)
                {
                    nfs_super.map_inode[byte_cursor] &= (uint8_t)(~(0x1 << bit_cursor));
                    is_find = TRUE;
                    break;
                }
                ino_cursor++;
            }
            if (is_find == TRUE)
            {
                break;
            }
        }
        if (inode->data)
            free(inode->data);
        free(inode);
    }
    return NFS_ERROR_NONE;
}
/**
 * @brief
 *
 * @param dentry dentry指向ino，读取该inode
 * @param ino inode唯一编号
 * @return struct nfs_inode*
 */
struct nfs_inode *nfs_read_inode(struct nfs_dentry *dentry, int ino)
{
    struct nfs_inode *inode = (struct nfs_inode *)malloc(sizeof(struct nfs_inode));
    struct nfs_inode_d inode_d;
    struct nfs_dentry *sub_dentry;
    struct nfs_dentry_d dentry_d;
    int dir_cnt = 0, i;
    if (nfs_driver_read(NFS_INO_OFS(ino), (uint8_t *)&inode_d,
                        sizeof(struct nfs_inode_d)) != NFS_ERROR_NONE)
    {
        NFS_DBG("[%s] io error\n", __func__);
        return NULL;
    }
    inode->dir_cnt = 0;
    inode->ino = inode_d.ino;
    inode->size = inode_d.size;
    memcpy(inode->target_path, inode_d.target_path, NFS_MAX_FILE_NAME);
    inode->dentry = dentry;
    inode->dentrys = NULL;
    if (NFS_IS_DIR(inode))
    {
        dir_cnt = inode_d.dir_cnt;
        for (i = 0; i < dir_cnt; i++)
        {
            if (nfs_driver_read(NFS_DATA_OFS(ino) + i * sizeof(struct nfs_dentry_d),
                                (uint8_t *)&dentry_d,
                                sizeof(struct nfs_dentry_d)) != NFS_ERROR_NONE)
            {
                NFS_DBG("[%s] io error\n", __func__);
                return NULL;
            }
            sub_dentry = new_dentry(dentry_d.fname, dentry_d.ftype);
            sub_dentry->parent = inode->dentry;
            sub_dentry->ino = dentry_d.ino;
            nfs_alloc_dentry(inode, sub_dentry);
        }
    }
    else if (NFS_IS_REG(inode))
    {
        inode->data = (uint8_t *)malloc(NFS_BLKS_SZ(NFS_DATA_PER_FILE));
        if (nfs_driver_read(NFS_DATA_OFS(ino), (uint8_t *)inode->data,
                            NFS_BLKS_SZ(NFS_DATA_PER_FILE)) != NFS_ERROR_NONE)
        {
            NFS_DBG("[%s] io error\n", __func__);
            return NULL;
        }
    }
    return inode;
}
/**
 * @brief
 *
 * @param inode
 * @param dir [0...]
 * @return struct nfs_dentry*
 */
struct nfs_dentry *nfs_get_dentry(struct nfs_inode *inode, int dir)
{
    struct nfs_dentry *dentry_cursor = inode->dentrys;
    int cnt = 0;
    while (dentry_cursor)
    {
        if (dir == cnt)
        {
            return dentry_cursor;
        }
        cnt++;
        dentry_cursor = dentry_cursor->brother;
    }
    return NULL;
}
/**
 * @brief
 * path: /qwe/ad  total_lvl = 2,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry
 *      3) find qwe's inode     lvl = 2
 *      4) find ad's dentry
 *
 * path: /qwe     total_lvl = 1,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry
 *
 * @param path
 * @return struct nfs_inode*
 */
struct nfs_dentry *nfs_lookup(const char *path, boolean *is_find, boolean *is_root)
{
    struct nfs_dentry *dentry_cursor = nfs_super.root_dentry;
    struct nfs_dentry *dentry_ret = NULL;
    struct nfs_inode *inode;
    int total_lvl = nfs_calc_lvl(path);
    int lvl = 0;
    boolean is_hit;
    char *fname = NULL;
    char *path_cpy = (char *)malloc(sizeof(path));
    *is_root = FALSE;
    strcpy(path_cpy, path);

    if (total_lvl == 0)
    { /* 根目录 */
        *is_find = TRUE;
        *is_root = TRUE;
        dentry_ret = nfs_super.root_dentry;
    }
    fname = strtok(path_cpy, "/");
    while (fname)
    {
        lvl++;
        if (dentry_cursor->inode == NULL)
        { /* Cache机制 */
            nfs_read_inode(dentry_cursor, dentry_cursor->ino);
        }

        inode = dentry_cursor->inode;

        if (NFS_IS_REG(inode) && lvl < total_lvl)
        {
            NFS_DBG("[%s] not a dir\n", __func__);
            dentry_ret = inode->dentry;
            break;
        }
        if (NFS_IS_DIR(inode))
        {
            dentry_cursor = inode->dentrys;
            is_hit = FALSE;

            while (dentry_cursor)
            {
                if (memcmp(dentry_cursor->fname, fname, strlen(fname)) == 0)
                {
                    is_hit = TRUE;
                    break;
                }
                dentry_cursor = dentry_cursor->brother;
            }

            if (!is_hit)
            {
                *is_find = FALSE;
                NFS_DBG("[%s] not found %s\n", __func__, fname);
                dentry_ret = inode->dentry;
                break;
            }

            if (is_hit && lvl == total_lvl)
            {
                *is_find = TRUE;
                dentry_ret = dentry_cursor;
                break;
            }
        }
        fname = strtok(NULL, "/");
    }

    if (dentry_ret->inode == NULL)
    {
        dentry_ret->inode = nfs_read_inode(dentry_ret, dentry_ret->ino);
    }

    return dentry_ret;
}
/**
 * @brief 挂载newfs, Layout 如下
 *
 * Layout
 * | Super | Inode Map | Data Map | Data |
 *
 * IO_SZ = BLK_SZ
 *
 * 每个Inode占用一个Blk
 */
int nfs_mount(struct custom_options options)
{
    int ret = NFS_ERROR_NONE;
    int driver_fd;
    struct nfs_super_d nfs_super_d;
    struct nfs_dentry *root_dentry;
    struct nfs_inode *root_inode;

    int super_blks;
    int inode_num;
    int map_inode_blks;
    int map_data_blks;

    boolean is_init = FALSE;
    nfs_super.is_mounted = FALSE;

    // driver_fd = open(options.device, O_RDWR);
    driver_fd = ddriver_open(options.device);
    if (driver_fd < 0)
        return driver_fd;

    nfs_super.driver_fd = driver_fd;
    ddriver_ioctl(NFS_DRIVER(), IOC_REQ_DEVICE_SIZE, &nfs_super.sz_disk);
    ddriver_ioctl(NFS_DRIVER(), IOC_REQ_DEVICE_IO_SZ, &nfs_super.sz_io);
    nfs_super.sz_blk = 2 * nfs_super.sz_io; // nfs块大小 1024 B

    root_dentry = new_dentry("/", NFS_DIR);

    if (NFS_ERROR_NONE != nfs_driver_read(NFS_SUPER_OFS,
                                          (uint8_t *)(&nfs_super_d), sizeof(struct nfs_super_d)))
        return -NFS_ERROR_IO;

    // 首次启动磁盘，估算各部分大小，建立super block布局
    if (nfs_super_d.magic_num != NFS_MAGIC_NUM)
    {
        super_blks = NFS_ROUND_UP(sizeof(struct nfs_super_d), NFS_IO_SZ()) / NFS_IO_SZ();
        // 一个inode对应一个文件，一个文件最多 DATA_PER_FILE 块
        inode_num = NFS_DISK_SZ() / ((NFS_DATA_PER_FILE + NFS_INODE_PER_FILE) * NFS_IO_SZ());

        map_inode_blks = NFS_ROUND_UP(NFS_ROUND_UP(inode_num, UINT8_BITS), NFS_IO_SZ()) / NFS_IO_SZ();

        map_data_blks = NFS_ROUND_UP(NFS_ROUND_UP(NFS_DATA_PER_FILE * inode_num, UINT8_BITS), NFS_IO_SZ()) / NFS_IO_SZ();

        // layout
        nfs_super_d.magic_num = NFS_MAGIC_NUM;
        nfs_super_d.sz_usage = 0;
        nfs_super_d.num_ino = (inode_num - super_blks - map_inode_blks - map_data_blks);
        nfs_super_d.map_inode_blks = map_inode_blks;
        nfs_super_d.map_data_blks = map_data_blks;

        nfs_super_d.map_inode_offset = NFS_SUPER_OFS + NFS_BLKS_SZ(super_blks);
        nfs_super_d.map_data_offset = nfs_super_d.map_inode_offset + NFS_BLKS_SZ(map_inode_blks);
        nfs_super_d.data_offset = nfs_super_d.map_data_offset + NFS_BLKS_SZ(map_data_blks);

        NFS_DBG("inode map blocks: %d\n", map_inode_blks);
        NFS_DBG(" data map blocks: %d\n", map_data_blks);
        is_init = TRUE;
    }
    // 建立 super block 的 in-memory 结构
    nfs_super.sz_usage = nfs_super_d.sz_usage;
    nfs_super.num_ino = nfs_super_d.num_ino;
    nfs_super.num_data = NFS_DATA_PER_FILE * nfs_super_d.num_ino;

    nfs_super.map_inode = (uint8_t *)malloc(NFS_BLKS_SZ(nfs_super_d.map_inode_blks));
    nfs_super.map_inode_blks = nfs_super_d.map_inode_blks;
    nfs_super.map_inode_offset = nfs_super_d.map_inode_offset;

    nfs_super.map_data = (uint8_t *)malloc(NFS_BLKS_SZ(nfs_super_d.map_data_blks));
    nfs_super.map_data_blks = nfs_super_d.map_data_blks;
    nfs_super.map_data_offset = nfs_super_d.map_data_offset;

    nfs_super.data_offset = nfs_super_d.data_offset;

    // 读inode位图
    if (nfs_driver_read(nfs_super_d.map_inode_offset, (uint8_t *)(nfs_super.map_inode),
                        NFS_BLKS_SZ(nfs_super_d.map_inode_blks)) != NFS_ERROR_NONE)
        return -NFS_ERROR_IO;
    // 读data位图
    if (nfs_driver_read(nfs_super_d.map_data_offset, (uint8_t *)(nfs_super.map_data),
                        NFS_BLKS_SZ(nfs_super_d.map_data_blks)) != NFS_ERROR_NONE)
        return -NFS_ERROR_IO;
    // 分配根节点
    if (is_init)
    {
        root_inode = nfs_alloc_inode(root_dentry);
        nfs_sync_inode(root_inode);
    }
    root_inode = nfs_read_inode(root_dentry, NFS_ROOT_INO);
    root_dentry->inode = root_inode;
    nfs_super.root_dentry = root_dentry;
    nfs_super.is_mounted = TRUE;

    nfs_dump_map_inode();
    return ret;
}
// 解除挂载
int nfs_umount()
{
    struct nfs_super_d nfs_super_d;

    if (!nfs_super.is_mounted)
        return NFS_ERROR_NONE;

    // 从根节点向下刷写节点
    nfs_sync_inode(nfs_super.root_dentry->inode);

    nfs_super_d.magic_num = NFS_MAGIC_NUM;
    nfs_super_d.sz_usage = nfs_super.sz_usage;
    nfs_super_d.num_ino = nfs_super.num_ino;

    nfs_super_d.map_inode_blks = nfs_super.map_inode_blks;
    nfs_super_d.map_inode_offset = nfs_super.map_inode_offset;

    nfs_super_d.map_data_blks = nfs_super.map_data_blks;
    nfs_super_d.map_data_offset = nfs_super.map_inode_offset;

    nfs_super_d.data_offset = nfs_super.data_offset;

    // 回写super block
    if (nfs_driver_write(NFS_SUPER_OFS, (uint8_t *)&nfs_super_d,
                         sizeof(struct nfs_super_d)) != NFS_ERROR_NONE)
        return -NFS_ERROR_IO;
    // 回写inode map
    if (nfs_driver_write(nfs_super_d.map_inode_offset, (uint8_t *)(nfs_super.map_inode),
                         NFS_BLKS_SZ(nfs_super_d.map_inode_blks)) != NFS_ERROR_NONE)
        return -NFS_ERROR_IO;
    free(nfs_super.map_inode);
    // 回写data map
    if (nfs_driver_write(nfs_super_d.map_data_offset, (uint8_t *)(nfs_super.map_data),
                         NFS_BLKS_SZ(nfs_super_d.map_data_blks)) != NFS_ERROR_NONE)
        return -NFS_ERROR_IO;
    free(nfs_super.map_data);

    ddriver_close(NFS_DRIVER());

    return NFS_ERROR_NONE;
}
