#include "../include/newfs.h"

#define OPTION(t, p)                             \
	{                                            \
		t, offsetof(struct custom_options, p), 1 \
	}
static const struct fuse_opt option_spec[] = {/* 用于FUSE文件系统解析参数 */
											  OPTION("--device=%s", device),
											  FUSE_OPT_END};

struct nfs_super nfs_super;
struct custom_options nfs_options; /* 全局选项 */

/******************************************************************************
 * SECTION: FUSE操作定义
 *******************************************************************************/
static struct fuse_operations operations = {
	.init = newfs_init,			/* mount文件系统 */
	.destroy = newfs_destroy,	/* umount文件系统 */
	.mkdir = newfs_mkdir,		/* 建目录，mkdir */
	.getattr = newfs_getattr,	/* 获取文件属性，类似stat，必须完成 */
	.readdir = newfs_readdir,	/* 填充dentrys */
	.mknod = newfs_mknod,		/* 创建文件，touch相关 */
	.write = newfs_write,		/* 写入文件 */
	.read = newfs_read,			/* 读文件 */
	.utimens = newfs_utimens,	/* 修改时间，忽略，避免touch报错 */
	.truncate = newfs_truncate, /* 改变文件大小 */
	.unlink = newfs_unlink,		/* 删除文件 */
	.rmdir = newfs_rmdir,		/* 删除目录， rm -r */
	.rename = newfs_rename,		/* 重命名，mv */

	.open = newfs_open,
	.opendir = newfs_opendir,
	.access = newfs_access};
/******************************************************************************
 * SECTION: 必做函数实现
 *******************************************************************************/
/**
 * @brief 挂载（mount）文件系统
 * @param conn_info 可忽略，一些建立连接相关的信息
 */
void *newfs_init(struct fuse_conn_info *conn_info)
{
	if (nfs_mount(nfs_options) != NFS_ERROR_NONE)
	{
		NFS_DBG("[%s] mount error\n", __func__);
		fuse_exit(fuse_get_context()->fuse);
		return NULL;
	}
	return NULL;
}

/**
 * @brief 卸载（umount）文件系统
 * @param p 可忽略
 */
void newfs_destroy(void *p)
{
	if (nfs_umount() != NFS_ERROR_NONE)
	{
		NFS_DBG("[%s] unmount error\n", __func__);
		fuse_exit(fuse_get_context()->fuse);
		return;
	}
	return;
}

/**
 * @brief 创建目录
 *
 * @param path 相对于挂载点的路径
 * @param mode 创建模式（只读？只写？），可忽略
 * @return int 0成功，否则失败
 */
int newfs_mkdir(const char *path, mode_t mode)
{
	(void)mode;
	boolean is_find, is_root;
	char *fname;
	struct nfs_dentry *last_dentry = nfs_lookup(path, &is_find, &is_root);
	struct nfs_dentry *dentry;
	struct nfs_inode *inode;

	if (is_find)
		return -NFS_ERROR_EXISTS;

	if (NFS_IS_REG(last_dentry->inode))
		return -NFS_ERROR_UNSUPPORTED;

	fname = nfs_get_fname(path);
	dentry = new_dentry(fname, NFS_DIR);
	dentry->parent = last_dentry;
			
	inode = nfs_alloc_inode(dentry);
	nfs_sync_inode(inode);

	nfs_alloc_dentry(last_dentry->inode, dentry);

	return NFS_ERROR_NONE;
}

/**
 * @brief 获取文件或目录的属性，该函数非常重要
 *
 * @param path 相对于挂载点的路径
 * @param nfs_stat 返回状态
 * @return int 0成功，否则失败
 */
int newfs_getattr(const char *path, struct stat *nfs_stat)
{
	boolean is_find, is_root;
	struct nfs_dentry *dentry = nfs_lookup(path, &is_find, &is_root);
	if (is_find == FALSE)
	{
		return -NFS_ERROR_NOTFOUND;
	}

	if (NFS_IS_DIR(dentry->inode))
	{
		nfs_stat->st_mode = S_IFDIR | NFS_DEFAULT_PERM;
		nfs_stat->st_size = dentry->inode->dir_cnt * sizeof(struct nfs_dentry_d);
	}
	else if (NFS_IS_REG(dentry->inode))
	{
		nfs_stat->st_mode = S_IFREG | NFS_DEFAULT_PERM;
		nfs_stat->st_size = dentry->inode->size;
	}
	else if (NFS_IS_SYM_LINK(dentry->inode))
	{
		nfs_stat->st_mode = S_IFLNK | NFS_DEFAULT_PERM;
		nfs_stat->st_size = dentry->inode->size;
	}

	nfs_stat->st_nlink = 1;
	nfs_stat->st_uid = getuid();
	nfs_stat->st_gid = getgid();
	nfs_stat->st_atime = time(NULL);
	nfs_stat->st_mtime = time(NULL);
	nfs_stat->st_blksize = NFS_IO_SZ();

	if (is_root)
	{
		nfs_stat->st_size = nfs_super.sz_usage;
		nfs_stat->st_blocks = NFS_DISK_SZ() / NFS_IO_SZ();
		nfs_stat->st_nlink = 2; /* !特殊，根目录link数为2 */
	}
	return NFS_ERROR_NONE;
}

/**
 * @brief 遍历目录项，填充至buf，并交给FUSE输出
 *
 * @param path 相对于挂载点的路径
 * @param buf 输出buffer
 * @param filler 参数讲解:
 *
 * typedef int (*fuse_fill_dir_t) (void *buf, const char *name,
 *				const struct stat *stbuf, off_t off)
 * buf: name会被复制到buf中
 * name: dentry名字
 * stbuf: 文件状态，可忽略
 * off: 下一次offset从哪里开始，这里可以理解为第几个dentry
 *
 * @param offset 第几个目录项？
 * @param fi 可忽略
 * @return int 0成功，否则失败
 */
int newfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
				  struct fuse_file_info *fi)
{
	boolean is_find, is_root;
	int cur_dir = offset;

	struct nfs_dentry *dentry = nfs_lookup(path, &is_find, &is_root);
	struct nfs_dentry *sub_dentry;
	struct nfs_inode *inode;
	if (is_find)
	{
		inode = dentry->inode;
		sub_dentry = nfs_get_dentry(inode, cur_dir);
		if (sub_dentry)
		{
			filler(buf, sub_dentry->fname, NULL, ++offset);
		}
		return NFS_ERROR_NONE;
	}
	return -NFS_ERROR_NOTFOUND;
}

/**
 * @brief 创建文件
 *
 * @param path 相对于挂载点的路径
 * @param mode 创建文件的模式，可忽略
 * @param dev 设备类型，可忽略
 * @return int 0成功，否则失败
 */
int newfs_mknod(const char *path, mode_t mode, dev_t dev)
{
	boolean is_find, is_root;

	struct nfs_dentry *last_dentry = nfs_lookup(path, &is_find, &is_root);
	struct nfs_dentry *dentry;
	struct nfs_inode *inode;
	char *fname;

	if (is_find == TRUE)
	{
		return -EEXIST;
	}

	fname = nfs_get_fname(path);

	if (S_ISREG(mode))
	{
		dentry = new_dentry(fname, NFS_REG_FILE);
	}
	else if (S_ISDIR(mode))
	{
		dentry = new_dentry(fname, NFS_DIR);
	}
	else
	{
		dentry = new_dentry(fname, NFS_REG_FILE);
	}
	dentry->parent = last_dentry;
	// ??? 
	inode = nfs_alloc_inode(dentry);
	nfs_sync_inode(inode);
	nfs_alloc_dentry(last_dentry->inode, dentry);

	return NFS_ERROR_NONE;
}

/**
 * @brief 修改时间，为了不让touch报错
 *
 * @param path 相对于挂载点的路径
 * @param tv 实践
 * @return int 0成功，否则失败
 */
int newfs_utimens(const char *path, const struct timespec tv[2])
{
	(void)path;
	return NFS_ERROR_NONE;
}
/******************************************************************************
 * SECTION: 选做函数实现
 *******************************************************************************/
/**
 * @brief 写入文件
 *
 * @param path 相对于挂载点的路径
 * @param buf 写入的内容
 * @param size 写入的字节数
 * @param offset 相对文件的偏移
 * @param fi 可忽略
 * @return int 写入大小
 */
int newfs_write(const char *path, const char *buf, size_t size, off_t offset,
				struct fuse_file_info *fi)
{
	boolean is_find, is_root;
	struct nfs_dentry *dentry = nfs_lookup(path, &is_find, &is_root);
	struct nfs_inode *inode;

	if (is_find == FALSE)
	{
		return -NFS_ERROR_NOTFOUND;
	}

	inode = dentry->inode;

	if (NFS_IS_DIR(inode))
	{
		return -NFS_ERROR_ISDIR;
	}

	if (inode->size < offset)
	{
		return -NFS_ERROR_SEEK;
	}

	memcpy(inode->data + offset, buf, size);
	inode->size = offset + size > inode->size ? offset + size : inode->size;

	return size;
}

/**
 * @brief 读取文件
 *
 * @param path 相对于挂载点的路径
 * @param buf 读取的内容
 * @param size 读取的字节数
 * @param offset 相对文件的偏移
 * @param fi 可忽略
 * @return int 读取大小
 */
int newfs_read(const char *path, char *buf, size_t size, off_t offset,
			   struct fuse_file_info *fi)
{
	boolean is_find, is_root;
	struct nfs_dentry *dentry = nfs_lookup(path, &is_find, &is_root);
	struct nfs_inode *inode;

	if (is_find == FALSE)
	{
		return -NFS_ERROR_NOTFOUND;
	}

	inode = dentry->inode;

	if (NFS_IS_DIR(inode))
	{
		return -NFS_ERROR_ISDIR;
	}

	if (inode->size < offset)
	{
		return -NFS_ERROR_SEEK;
	}

	memcpy(buf, inode->data + offset, size);

	return size;
}

/**
 * @brief 删除文件
 *
 * @param path 相对于挂载点的路径
 * @return int 0成功，否则失败
 */
int newfs_unlink(const char *path)
{
	boolean is_find, is_root;
	struct nfs_dentry *dentry = nfs_lookup(path, &is_find, &is_root);
	struct nfs_inode *inode;

	if (is_find == FALSE)
	{
		return -NFS_ERROR_NOTFOUND;
	}

	inode = dentry->inode;

	nfs_drop_inode(inode);
	nfs_drop_dentry(dentry->parent->inode, dentry);
	return NFS_ERROR_NONE;
}

/**
 * @brief 删除目录
 *
 * 一个可能的删除目录操作如下：
 * rm ./tests/mnt/j/ -r
 *  1) Step 1. rm ./tests/mnt/j/j
 *  2) Step 2. rm ./tests/mnt/j
 * 即，先删除最深层的文件，再删除目录文件本身
 *
 * @param path 相对于挂载点的路径
 * @return int 0成功，否则失败
 */
int newfs_rmdir(const char *path)
{
	return newfs_unlink(path);
}

/**
 * @brief 重命名文件
 *
 * @param from 源文件路径
 * @param to 目标文件路径
 * @return int 0成功，否则失败
 */
int newfs_rename(const char *from, const char *to)
{
	int ret = NFS_ERROR_NONE;
	boolean is_find, is_root;
	struct nfs_dentry *from_dentry = nfs_lookup(from, &is_find, &is_root);
	struct nfs_inode *from_inode;
	struct nfs_dentry *to_dentry;
	mode_t mode = 0;
	if (is_find == FALSE)
	{
		return -NFS_ERROR_NOTFOUND;
	}

	if (strcmp(from, to) == 0)
	{
		return NFS_ERROR_NONE;
	}

	from_inode = from_dentry->inode;

	if (NFS_IS_DIR(from_inode))
	{
		mode = S_IFDIR;
	}
	else if (NFS_IS_REG(from_inode))
	{
		mode = S_IFREG;
	}

	ret = newfs_mknod(to, mode, 0);
	if (ret != NFS_ERROR_NONE)
	{ /* 保证目的文件不存在 */
		return ret;
	}

	to_dentry = nfs_lookup(to, &is_find, &is_root);
	nfs_drop_inode(to_dentry->inode); /* 保证生成的inode被释放 */
	to_dentry->ino = from_inode->ino; /* 指向新的inode */
	to_dentry->inode = from_inode;

	nfs_drop_dentry(from_dentry->parent->inode, from_dentry);
	return ret;
}

/**
 * @brief 打开文件，可以在这里维护fi的信息，例如，fi->fh可以理解为一个64位指针，可以把自己想保存的数据结构
 * 保存在fh中
 *
 * @param path 相对于挂载点的路径
 * @param fi 文件信息
 * @return int 0成功，否则失败
 */
int newfs_open(const char *path, struct fuse_file_info *fi)
{
	return NFS_ERROR_NONE;
}

/**
 * @brief 打开目录文件
 *
 * @param path 相对于挂载点的路径
 * @param fi 文件信息
 * @return int 0成功，否则失败
 */
int newfs_opendir(const char *path, struct fuse_file_info *fi)
{
	return NFS_ERROR_NONE;
}

/**
 * @brief 改变文件大小
 *
 * @param path 相对于挂载点的路径
 * @param offset 改变后文件大小
 * @return int 0成功，否则失败
 */
int newfs_truncate(const char *path, off_t offset)
{
	boolean is_find, is_root;
	struct nfs_dentry *dentry = nfs_lookup(path, &is_find, &is_root);
	struct nfs_inode *inode;

	if (is_find == FALSE)
	{
		return -NFS_ERROR_NOTFOUND;
	}

	inode = dentry->inode;

	if (NFS_IS_DIR(inode))
	{
		return -NFS_ERROR_ISDIR;
	}

	inode->size = offset;

	return NFS_ERROR_NONE;
}

/**
 * @brief 访问文件，因为读写文件时需要查看权限
 *
 * @param path 相对于挂载点的路径
 * @param type 访问类别
 * R_OK: Test for read permission.
 * W_OK: Test for write permission.
 * X_OK: Test for execute permission.
 * F_OK: Test for existence.
 *
 * @return int 0成功，否则失败
 */
int newfs_access(const char *path, int type)
{
	boolean is_find, is_root;
	boolean is_access_ok = FALSE;
	nfs_lookup(path, &is_find, &is_root);

	switch (type)
	{
	case R_OK:
		is_access_ok = TRUE;
		break;
	case F_OK:
		if (is_find)
		{
			is_access_ok = TRUE;
		}
		break;
	case W_OK:
		is_access_ok = TRUE;
		break;
	case X_OK:
		is_access_ok = TRUE;
		break;
	default:
		break;
	}
	return is_access_ok ? NFS_ERROR_NONE : -NFS_ERROR_ACCESS;
}
/******************************************************************************
 * SECTION: FUSE入口
 *******************************************************************************/
int main(int argc, char **argv)
{
	int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	nfs_options.device = strdup("/home/students/200111223/ddriver");
	if (fuse_opt_parse(&args, &nfs_options, option_spec, NULL) == -1)
		return -1;
	ret = fuse_main(args.argc, args.argv, &operations, NULL);
	fuse_opt_free_args(&args);
	return ret;
}