#include "../include/newfs.h"

extern struct nfs_super      nfs_super; 
extern struct custom_options nfs_options;

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
int nfs_mount(struct custom_options options){
    int                 ret = ERROR_NONE;
    int                 driver_fd;
    struct disk_super   disk_super; 
    struct nfs_dentry*  root_dentry;
    struct nfs_inode*   root_inode;

    int                 inode_num;
    int                 map_inode_blks;
    int                 map_data_blks;
    int                 super_blks;

    boolean is_init = FALSE;
    nfs_super.is_mounted = FALSE;

    // driver_fd = open(options.device, O_RDWR);
    driver_fd = ddriver_open(options.device);
    if (driver_fd < 0) {
        return driver_fd;
    }

    nfs_super.driver_fd = driver_fd;
    ddriver_ioctl(DRIVER(), IOC_REQ_DEVICE_SIZE,  &nfs_super.sz_disk);
    ddriver_ioctl(DRIVER(), IOC_REQ_DEVICE_IO_SZ, &nfs_super.sz_io);
    
    root_dentry = new_dentry("/", DIR);

    if (ERROR_NONE != driver_read(SUPER_OFFSET, (uint8_t *)(&disk_super), sizeof(struct disk_super))) {
        return -EIO;
    }   
    // 首次读取super，估算各部分大小                                               
    if (disk_super.magic_num != MAGIC_NUM) {          
                                                
        super_blks = ROUND_UP(sizeof(struct disk_super), IO_SZ()) / IO_SZ();
        // 一个inode对应一个文件，一个文件最多 DATA_PER_FILE 块
        inode_num  =  DISK_SZ() / ((DATA_PER_FILE + INODE_PER_FILE) * IO_SZ());

        map_inode_blks = ROUND_UP(ROUND_UP(inode_num, UINT32_BITS), IO_SZ()) / IO_SZ();

        map_data_blks = ROUND_UP(ROUND_UP( DATA_PER_FILE * inode_num, UINT8_BITS), IO_SZ()) / IO_SZ();
        
        disk_super.map_inode_blks  = map_inode_blks;
        disk_super.map_data_blocks = map_data_blks;
        disk_super.sz_usage  = 0;

        // layout
        nfs_super.max_ino = (inode_num - super_blks - map_inode_blks - map_data_blks); 
        disk_super.max_ino = nfs_super.max_ino;
        disk_super.map_inode_offset = SUPER_OFFSET + BLKS_SZ(super_blks);
        disk_super.map_data_offset = disk_super.map_inode_offset + BLKS_SZ(map_inode_blks);
        disk_super.data_offset = disk_super.map_data_offset + BLKS_SZ(map_data_blks);
        
        DBG("inode map blocks: %d\n", map_inode_blks);
        DBG(" data map blocks: %d\n", map_data_blks);
        
        is_init = TRUE;
    }
    nfs_super.sz_usage   = disk_super.sz_usage;      /* 建立 in-memory 结构 */
    
    nfs_super.map_inode = (uint8_t *)malloc(BLKS_SZ(disk_super.map_inode_blks));
    nfs_super.map_inode_blks = disk_super.map_inode_blks;
    nfs_super.map_inode_offset = disk_super.map_inode_offset;
    nfs_super.data_offset = disk_super.data_offset;

    if (driver_read(disk_super.map_inode_offset, (uint8_t *)(nfs_super.map_inode), 
                        BLKS_SZ(disk_super.map_inode_blks)) != ERROR_NONE) {
        return -EIO;
    }

    if (is_init) {                                    /* 分配根节点 */
        root_inode = alloc_inode(root_dentry);
        sync_inode(root_inode);
    }
    
    root_inode            = read_inode(root_dentry, ROOT_INO);
    root_dentry->inode    = root_inode;
    nfs_super.root_dentry = root_dentry;
    nfs_super.is_mounted  = TRUE;

    dump_map();
    return ret;
}

// 解除挂载
int nfs_umount() {
    struct disk_super  disk_super; 

    if (!nfs_super.is_mounted) {
        return ERROR_NONE;
    }

    nfs_sync_inode(nfs_super.root_dentry->inode);     /* 从根节点向下刷写节点 */
                                                    
    disk_super.magic_num           = MAGIC_NUM;
    disk_super.map_inode_blks      = nfs_super.map_inode_blks;
    disk_super.map_inode_offset    = nfs_super.map_inode_offset;
    disk_super.data_offset         = nfs_super.data_offset;
    disk_super.sz_usage            = nfs_super.sz_usage;

    if (ERROR_NONE != nfs_driver_write(SUPER_OFFSET, (uint8_t *)&disk_super, sizeof(struct disk_super))) {
        return -EIO;
    }
    if (nfs_driver_write(disk_super.map_inode_offset, (uint8_t *)(nfs_super.map_inode), 
                         nfs_BLKS_SZ(disk_super.map_inode_blks)) != ERROR_NONE) {
        return -EIO;
    }

    free(nfs_super.map_inode);
    ddriver_close(DRIVER());
    return ERROR_NONE;
}

// 驱动读
int nfs_driver_read(int start, uint8_t *out_content, int size) {
    int      start_aligned  = ROUND_DOWN(start, IO_SZ());
    int      bias           = start - start_aligned;
    int      size_aligned   = ROUND_UP((size + bias), IO_SZ());
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    // lseek(nfs_driver(), start_aligned, SEEK_SET);
    ddriver_seek(nfs_driver(), start_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        // read(nfs_driver(), cur, IO_SZ());
        ddriver_read(nfs_driver(), cur, IO_SZ());
        cur          += IO_SZ();
        size_aligned -= IO_SZ();   
    }
    memcpy(out_content, temp_content + bias, size);
    free(temp_content);
    return ERROR_NONE;
}

// 驱动写
int nfs_driver_write(int dst, uint8_t *in_content, int size) {
    int      dst_aligned = ROUND_DOWN(dst, IO_SZ());
    int      bias           = dst - dst_aligned;
    int      size_aligned   = ROUND_UP((size + bias), IO_SZ());
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    nfs_driver_read(dst_aligned, temp_content, size_aligned);
    memcpy(temp_content + bias, in_content, size);
    
    // lseek(nfs_driver(), start_aligned, SEEK_SET);
    ddriver_seek(nfs_driver(), dst_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        // write(nfs_driver(), cur, IO_SZ());
        ddriver_write(nfs_driver(), cur, IO_SZ());
        cur          += IO_SZ();
        size_aligned -= IO_SZ();   
    }

    free(temp_content);
    return ERROR_NONE;
}

// 将内存inode及其下方结构全部刷回磁盘
int nfs_sync_inode(struct nfs_inode * inode) {
    struct disk_inode   disk_inode;
    struct nfs_dentry*  dentry_cursor;
    struct disk_dentry  disk_dentry;
    int ino          = inode->ino;
    disk_inode.ino   = ino;
    disk_inode.size  = inode->size;
    memcpy(disk_inode.target_path, inode->target_path, MAX_NAME_LEN);
    disk_inode.ftype = inode->dentry->ftype;
    disk_inode.dir_cnt = inode->dir_cnt;
    int offset;
    
    if (nfs_driver_write(nfs_INO_OFS(ino), (uint8_t *)&disk_inode, 
                     sizeof(struct disk_inode)) != ERROR_NONE) {
        nfs_DBG("[%s] io error\n", __func__);
        return -EIO;
    }
                                                      /* Cycle 1: 写 INODE */
                                                      /* Cycle 2: 写 数据 */
    if (nfs_IS_DIR(inode)) {                          
        dentry_cursor = inode->dentrys;
        offset        = nfs_DATA_OFS(ino);
        while (dentry_cursor != NULL)
        {
            memcpy(disk_dentry.fname, dentry_cursor->fname, MAX_NAME_LEN);
            disk_dentry.ftype = dentry_cursor->ftype;
            disk_dentry.ino = dentry_cursor->ino;
            if (nfs_driver_write(offset, (uint8_t *)&disk_dentry, 
                                 sizeof(struct disk_dentry)) != ERROR_NONE) {
                nfs_DBG("[%s] io error\n", __func__);
                return -EIO;                     
            }
            
            if (dentry_cursor->inode != NULL) {
                nfs_sync_inode(dentry_cursor->inode);
            }

            dentry_cursor = dentry_cursor->brother;
            offset += sizeof(struct disk_dentry);
        }
    }
    else if (nfs_IS_REG(inode)) {
        if (nfs_driver_write(nfs_DATA_OFS(ino), inode->data, 
                             nfs_BLKS_SZ(DATA_PER_FILE)) != ERROR_NONE) {
            nfs_DBG("[%s] io error\n", __func__);
            return -EIO;
        }
    }
    return ERROR_NONE;
}

// 获取文件名
char* nfs_get_fname(const char* path) {
    char ch = '/';
    char *q = strrchr(path, ch) + 1;
    return q;
}

/**
 * @brief 分配一个inode，占用位图
 * 
 * @param dentry 该dentry指向分配的inode
 */
struct nfs_inode* nfs_alloc_inode(struct nfs_dentry * dentry) {
    struct nfs_inode* inode;
    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int ino_cursor  = 0;
    boolean is_find_free_entry = FALSE;

    for (byte_cursor = 0; byte_cursor < nfs_BLKS_SZ(nfs_super.map_inode_blks); 
         byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            if((nfs_super.map_inode[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                                                      /* 当前ino_cursor位置空闲 */
                nfs_super.map_inode[byte_cursor] |= (0x1 << bit_cursor);
                is_find_free_entry = TRUE;           
                break;
            }
            ino_cursor++;
        }
        if (is_find_free_entry) {
            break;
        }
    }

    if (!is_find_free_entry || ino_cursor == nfs_super.max_ino)
        return -ENOSPC;

    inode = (struct nfs_inode*)malloc(sizeof(struct nfs_inode));
    inode->ino  = ino_cursor; 
    inode->size = 0;
                                                      /* dentry指向inode */
    dentry->inode = inode;
    dentry->ino   = inode->ino;
                                                      /* inode指回dentry */
    inode->dentry = dentry;
    
    inode->dir_cnt = 0;
    inode->dentrys = NULL;
    
    if (nfs_IS_REG(inode)) {
        inode->data = (uint8_t *)malloc(nfs_BLKS_SZ(DATA_PER_FILE));
    }

    return inode;
}


/**
 * @brief 计算路径的层级
 * exm: /av/c/d/f
 * -> lvl = 4
 * @param path 
 * @return int 
 */
int nfs_calc_lvl(const char * path) {
    // char* path_cpy = (char *)malloc(strlen(path));
    // strcpy(path_cpy, path);
    char* str = path;
    int   lvl = 0;
    if (strcmp(path, "/") == 0) {
        return lvl;
    }
    while (*str != NULL) {
        if (*str == '/') {
            lvl++;
        }
        str++;
    }
    return lvl;
}

/**
 * @brief 为一个inode分配dentry，采用头插法
 * 
 * @param inode 
 * @param dentry 
 * @return int 
 */
int nfs_alloc_dentry(struct nfs_inode* inode, struct nfs_dentry* dentry) {
    if (inode->dentrys == NULL) {
        inode->dentrys = dentry;
    }
    else {
        dentry->brother = inode->dentrys;
        inode->dentrys = dentry;
    }
    inode->dir_cnt++;
    return inode->dir_cnt;
}
/**
 * @brief 将dentry从inode的dentrys中取出
 * 
 * @param inode 
 * @param dentry 
 * @return int 
 */
int nfs_drop_dentry(struct nfs_inode * inode, struct nfs_dentry * dentry) {
    boolean is_find = FALSE;
    struct nfs_dentry* dentry_cursor;
    dentry_cursor = inode->dentrys;
    
    if (dentry_cursor == dentry) {
        inode->dentrys = dentry->brother;
        is_find = TRUE;
    }
    else {
        while (dentry_cursor)
        {
            if (dentry_cursor->brother == dentry) {
                dentry_cursor->brother = dentry->brother;
                is_find = TRUE;
                break;
            }
            dentry_cursor = dentry_cursor->brother;
        }
    }
    if (!is_find) {
        return -ENOENT;
    }
    inode->dir_cnt--;
    return inode->dir_cnt;
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
int nfs_drop_inode(struct nfs_inode * inode) {
    struct nfs_dentry*  dentry_cursor;
    struct nfs_dentry*  dentry_to_free;
    struct nfs_inode*   inode_cursor;

    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int ino_cursor  = 0;
    boolean is_find = FALSE;

    if (inode == nfs_super.root_dentry->inode) {
        return EINVAL;
    }

    if (nfs_IS_DIR(inode)) {
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
    else if (nfs_IS_REG(inode) || nfs_IS_SYM_LINK(inode)) {
        for (byte_cursor = 0; byte_cursor < nfs_BLKS_SZ(nfs_super.map_inode_blks); 
            byte_cursor++)                            /* 调整inodemap */
        {
            for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
                if (ino_cursor == inode->ino) {
                     nfs_super.map_inode[byte_cursor] &= (uint8_t)(~(0x1 << bit_cursor));
                     is_find = TRUE;
                     break;
                }
                ino_cursor++;
            }
            if (is_find == TRUE) {
                break;
            }
        }
        if (inode->data)
            free(inode->data);
        free(inode);
    }
    return ERROR_NONE;
}
/**
 * @brief 
 * 
 * @param dentry dentry指向ino，读取该inode
 * @param ino inode唯一编号
 * @return struct nfs_inode* 
 */
struct nfs_inode* nfs_read_inode(struct nfs_dentry * dentry, int ino) {
    struct nfs_inode* inode = (struct nfs_inode*)malloc(sizeof(struct nfs_inode));
    struct disk_inode inode_d;
    struct nfs_dentry* sub_dentry;
    struct disk_dentry dentry_d;
    int    dir_cnt = 0, i;
    if (nfs_driver_read(nfs_INO_OFS(ino), (uint8_t *)&inode_d, 
                        sizeof(struct disk_inode)) != ERROR_NONE) {
        DBG("[%s] io error\n", __func__);
        return NULL;                    
    }
    inode->dir_cnt = 0;
    inode->ino = inode_d.ino;
    inode->size = inode_d.size;
    memcpy(inode->target_path, inode_d.target_path, MAX_NAME_LEN);
    inode->dentry = dentry;
    inode->dentrys = NULL;
    if (nfs_IS_DIR(inode)) {
        dir_cnt = inode_d.dir_cnt;
        for (i = 0; i < dir_cnt; i++)
        {
            if (nfs_driver_read(nfs_DATA_OFS(ino) + i * sizeof(struct disk_dentry), 
                                (uint8_t *)&dentry_d, 
                                sizeof(struct disk_dentry)) != ERROR_NONE) {
                nfs_DBG("[%s] io error\n", __func__);
                return NULL;                    
            }
            sub_dentry = new_dentry(dentry_d.fname, dentry_d.ftype);
            sub_dentry->parent = inode->dentry;
            sub_dentry->ino    = dentry_d.ino; 
            nfs_alloc_dentry(inode, sub_dentry);
        }
    }
    else if (nfs_IS_REG(inode)) {
        inode->data = (uint8_t *)malloc(nfs_BLKS_SZ(DATA_PER_FILE));
        if (nfs_driver_read(nfs_DATA_OFS(ino), (uint8_t *)inode->data, 
                            nfs_BLKS_SZ(DATA_PER_FILE)) != ERROR_NONE) {
            nfs_DBG("[%s] io error\n", __func__);
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
struct nfs_dentry* nfs_get_dentry(struct nfs_inode * inode, int dir) {
    struct nfs_dentry* dentry_cursor = inode->dentrys;
    int    cnt = 0;
    while (dentry_cursor)
    {
        if (dir == cnt) {
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
struct nfs_dentry* nfs_lookup(const char * path, boolean* is_find, boolean* is_root) {
    struct nfs_dentry* dentry_cursor = nfs_super.root_dentry;
    struct nfs_dentry* dentry_ret = NULL;
    struct nfs_inode*  inode; 
    int   total_lvl = nfs_calc_lvl(path);
    int   lvl = 0;
    boolean is_hit;
    char* fname = NULL;
    char* path_cpy = (char*)malloc(sizeof(path));
    *is_root = FALSE;
    strcpy(path_cpy, path);

    if (total_lvl == 0) {                           /* 根目录 */
        *is_find = TRUE;
        *is_root = TRUE;
        dentry_ret = nfs_super.root_dentry;
    }
    fname = strtok(path_cpy, "/");       
    while (fname)
    {   
        lvl++;
        if (dentry_cursor->inode == NULL) {           /* Cache机制 */
            nfs_read_inode(dentry_cursor, dentry_cursor->ino);
        }

        inode = dentry_cursor->inode;

        if (nfs_IS_REG(inode) && lvl < total_lvl) {
            nfs_DBG("[%s] not a dir\n", __func__);
            dentry_ret = inode->dentry;
            break;
        }
        if (nfs_IS_DIR(inode)) {
            dentry_cursor = inode->dentrys;
            is_hit        = FALSE;

            while (dentry_cursor)
            {
                if (memcmp(dentry_cursor->fname, fname, strlen(fname)) == 0) {
                    is_hit = TRUE;
                    break;
                }
                dentry_cursor = dentry_cursor->brother;
            }
            
            if (!is_hit) {
                *is_find = FALSE;
                nfs_DBG("[%s] not found %s\n", __func__, fname);
                dentry_ret = inode->dentry;
                break;
            }

            if (is_hit && lvl == total_lvl) {
                *is_find = TRUE;
                dentry_ret = dentry_cursor;
                break;
            }
        }
        fname = strtok(NULL, "/"); 
    }

    if (dentry_ret->inode == NULL) {
        dentry_ret->inode = nfs_read_inode(dentry_ret, dentry_ret->ino);
    }
    
    return dentry_ret;
}