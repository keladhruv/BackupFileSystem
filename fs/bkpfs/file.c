/*
 * Copyright (c) 1998-2017 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2017 Stony Brook University
 * Copyright (c) 2003-2017 The Research Foundation of SUNY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "bkpfs.h"

#define delete_bkp _IOW('a', 'a', int*)
#define list _IOR('a', 'b', int*)
#define list_2 _IOR('a', 'c', int*)
#define restore _IOW('a', 'd', int*)
#define view _IOW('a', 'e', int*)
#define view_2 _IOWR('a', 'i', char*)

static ssize_t bkpfs_read(struct file *file, char __user *buf,
			   size_t count, loff_t *ppos)
{
	int err;
	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;

	lower_file = bkpfs_lower_file(file);
	err = vfs_read(lower_file, buf, count, ppos);
	/* update our inode atime upon a successful lower read */
	if (err >= 0)
		fsstack_copy_attr_atime(d_inode(dentry),
					file_inode(lower_file));

	return err;
}

/*Function Written for deleting backup files, based on the bkpfs code. */
static int delete_backup(struct inode *dir, struct dentry *dentry)
{
	int err;
	struct dentry *lower_dentry;
	struct inode *lower_dir_inode = bkpfs_lower_inode(dir);
	struct dentry *lower_dir_dentry;
	struct path lower_path;

	bkpfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	dget(lower_dentry);
	lower_dir_dentry = lock_parent(lower_dentry);

	err = vfs_unlink(lower_dir_inode, lower_dentry, NULL);

	/*
	 * Note: unlinking on top of NFS can cause silly-renamed files.
	 * Trying to delete such files results in EBUSY from NFS
	 * below.  Silly-renamed files will get deleted by NFS later on, so
	 * we just need to detect them here and treat such EBUSY errors as
	 * if the upper file was successfully deleted.
	 */
	if (err == -EBUSY && lower_dentry->d_flags & DCACHE_NFSFS_RENAMED)
		err = 0;
	if (err)
		goto out;
	fsstack_copy_attr_times(dir, lower_dir_inode);
	fsstack_copy_inode_size(dir, lower_dir_inode);
	set_nlink(d_inode(dentry),
		  bkpfs_lower_inode(d_inode(dentry))->i_nlink);
	d_inode(dentry)->i_ctime = dir->i_ctime;
	d_drop(dentry); /* this is needed, else LTP fails (VFS won't do it) */
out:
	unlock_dir(lower_dir_dentry);
	dput(lower_dentry);
	bkpfs_put_lower_path(dentry, &lower_path);
	return err;
}

/* Function Written for copying data in to file. */
int copy_file_data(struct file *file_in, struct dentry *dentry)
{
	struct file *lower_file = NULL;
	struct path lower_path;
	int err = 0;
	loff_t pos_in = 0;
	loff_t pos_out = 0;
	int sizeOfFile = 0;
	sizeOfFile = file_in->f_inode->i_size;
	bkpfs_get_lower_path(dentry, &lower_path);
	lower_file = dentry_open(&lower_path, O_WRONLY, current_cred());
	vfs_copy_file_range(file_in, pos_in, lower_file, pos_out, sizeOfFile, 0);
	return err;
}
/*
	Function Written for restoring files, based on the bkpfs code.
	Just a slight differenc from the copy_file_data function, altered for specific usecase.
*/
int copy_file_ioctl(struct dentry *file_in, struct dentry *dentry)
{
	struct file *lower_file = NULL;
	struct file *orignal_file = NULL;
	struct path lower_path;
	struct path orignal_file_path;
	int err = 0, sizeOfFile = 0;
	loff_t pos_in = 0;
	loff_t pos_out = 0;
	bkpfs_get_lower_path(dentry, &lower_path);
	bkpfs_get_lower_path(file_in, &orignal_file_path);
	orignal_file = dentry_open(&orignal_file_path, O_WRONLY, current_cred());
	lower_file = dentry_open(&lower_path, O_RDONLY, current_cred());
	sizeOfFile = lower_file->f_inode->i_size;
	vfs_copy_file_range(lower_file, pos_out, orignal_file, pos_in, sizeOfFile, 0);
	return err;
}
/*
	The interpose function for creating a file.
*/
static struct dentry *call_interpose(struct dentry *dentry,
					 struct super_block *sb,
					 struct path *lower_path)
{
	struct inode *inode;
	struct inode *lower_inode;
	struct super_block *lower_sb;
	struct dentry *ret_dentry;

	lower_inode = d_inode(lower_path->dentry);
	lower_sb = bkpfs_lower_super(sb);
	//d_add(dentry, lower_inode); /* instantiate and hash */
	/* check that the lower file system didn't cross a mount point */
	if (lower_inode->i_sb != lower_sb) {
		ret_dentry = ERR_PTR(-EXDEV);
		goto out;
	}

	/*
	 * We allocate our new inode below by calling bkpfs_iget,
	 * which will initialize some of the new inode's fields
	 */

	/* inherit lower inode number for bkpfs's inode */
	inode = bkpfs_iget(sb, lower_inode);
	d_add(dentry, inode);
	if (IS_ERR(inode)) {
		ret_dentry = ERR_PTR(PTR_ERR(inode));
		goto out;
	}

out:
	return ret_dentry;
}
/*
	Triger for the call_interpose.
*/
int backup_interpose(struct dentry *dentry, struct super_block *sb,
		     struct path *lower_path)
{
	struct dentry *ret_dentry;

	ret_dentry = call_interpose(dentry, sb, lower_path);
	return PTR_ERR(ret_dentry);
}
/*
	The logic for creating a file is in this code.
	PLEASE NOTE THAT backup_no (Number of backups that can be created can be changed from here,
	but for now it's hardcoded.
*/
int create_file(struct file *file)
{
	int err = 0, attr_err;
	struct qstr this;
	struct file *lower_file;
	struct path lower_path;
	struct dentry *original_file_dentry;
	struct dentry *original_file_directory_dentry;
	struct dentry *lower_directory_dentry;
	struct dentry *upper_dentry;
	struct dentry *lower_dentry;
	struct vfsmount *lower_directory_mount;
	char *name = kmalloc(sizeof(char)*255, GFP_KERNEL); /* maximum size of file name=255 */
	unsigned long *version_int = kmalloc(sizeof(long), GFP_KERNEL);
	unsigned long *oldest_int = kmalloc(sizeof(long), GFP_KERNEL);
	char *version = kmalloc(sizeof(char)*100, GFP_KERNEL);
	char *oldest = kmalloc(sizeof(char)*100, GFP_KERNEL);
	int backup_no = 5;
	struct inode *filepath_inode;
	struct dentry *file_dentry;
	original_file_dentry = file->f_path.dentry;
	original_file_directory_dentry = dget_parent(original_file_dentry);
	lower_file = bkpfs_lower_file(file);
	if (backup_no == 0)
		return 0;
	attr_err = vfs_getxattr(lower_file->f_path.dentry, "user.version", version, 100*sizeof(char));
	if (attr_err < 0 || strcmp(version, "0") == 0) {
		strcpy(name, ".");
		strcat(name, original_file_dentry->d_iname);
		strcat(name, ".bkp.");
		strcpy(version, "1");
		vfs_setxattr(file->f_path.dentry, "user.version", version, 100*sizeof(char), XATTR_CREATE);
		vfs_setxattr(file->f_path.dentry, "user.oldest", version, 100*sizeof(char), XATTR_CREATE);
		vfs_setxattr(file->f_path.dentry, "user.newest", version, 100*sizeof(char), XATTR_CREATE);
		strcat(name, version);
	} else {
		vfs_getxattr(lower_file->f_path.dentry, "user.version", version, 100*sizeof(char));
		vfs_getxattr(lower_file->f_path.dentry, "user.oldest", oldest, 100*sizeof(char));
		attr_err = kstrtoul(version, 10, version_int);
		attr_err = kstrtoul(oldest, 10, oldest_int);
		*version_int += 1;
		if ((*version_int-*oldest_int) < backup_no) {
			strcpy(name, ".");
			strcat(name, original_file_dentry->d_iname);
			strcat(name, ".bkp.");
			snprintf (version, 100*sizeof(char), "%lu", *version_int);
			strcat(name, version);
			strcpy(version, version);
			vfs_setxattr(file->f_path.dentry, "user.version", version, 100*sizeof(char), XATTR_REPLACE);
			vfs_setxattr(file->f_path.dentry, "user.newest", version, 100*sizeof(char), XATTR_REPLACE);
		} else {
			strcpy(name, ".");
			strcat(name, original_file_dentry->d_iname);
			strcat(name, ".bkp.");
			strcat(name, oldest);
			d_set_d_op(original_file_directory_dentry, &bkpfs_dops);
			this.name = name;
			this.len = strlen(name);
			this.hash = full_name_hash(original_file_directory_dentry, this.name, this.len);
			file_dentry = d_lookup(original_file_directory_dentry, &this);
			filepath_inode = d_inode(original_file_directory_dentry);
			err = delete_backup(filepath_inode, file_dentry);
			*oldest_int += 1;
			snprintf (oldest, 100*sizeof(char), "%lu", *oldest_int);
			vfs_setxattr(file->f_path.dentry, "user.oldest", oldest, 100*sizeof(char), XATTR_REPLACE);
			strcpy(name, ".");
			strcat(name, original_file_dentry->d_iname);
			strcat(name, ".bkp.");
			snprintf (version, 100*sizeof(char), "%lu", *version_int);
			strcat(name, version);
			vfs_setxattr(file->f_path.dentry, "user.version", version, 100*sizeof(char), XATTR_REPLACE);
			vfs_setxattr(file->f_path.dentry, "user.newest", version, 100*sizeof(char), XATTR_REPLACE);
		}
	}
	d_set_d_op(original_file_directory_dentry, &bkpfs_dops);
	this.name = name;
	this.len = strlen(name);
	this.hash = full_name_hash(original_file_directory_dentry, this.name, this.len);
	upper_dentry = d_lookup(original_file_directory_dentry, &this);
	if (upper_dentry)
		goto setup_lower;
	upper_dentry = d_alloc(original_file_directory_dentry, &this);
	if (!upper_dentry) {
		err = -ENOMEM;
		goto ERROR;
	}
	err = new_dentry_private_data(upper_dentry);
	if (err)
		goto ERROR;
	d_set_d_op(upper_dentry, &bkpfs_dops);
setup_lower:
	lower_directory_dentry = dget_parent(lower_file->f_path.dentry);
	lower_directory_mount = lower_file->f_path.mnt;
	this.hash = full_name_hash(lower_directory_dentry, this.name, this.len);
	lower_dentry = d_lookup(lower_directory_dentry, &this);

	if (lower_dentry)
		goto setup;
	lower_dentry = d_alloc(lower_directory_dentry, &this);
	if (!lower_dentry) {
		err = -ENOMEM;
		goto ERROR;
	}
	d_add(lower_dentry, NULL);
setup:
	lower_path.dentry = lower_dentry;
	lower_path.mnt = mntget(lower_directory_mount);
	bkpfs_set_lower_path(upper_dentry, &lower_path);
	err = vfs_create(d_inode(lower_directory_dentry), lower_dentry, file->f_mode,
			 true);
	if (err)
		goto ERROR;
	err = backup_interpose(upper_dentry, d_inode(original_file_directory_dentry)->i_sb, &lower_path);
	err = copy_file_data(lower_file, upper_dentry);
	goto ERROR;
ERROR:
	bkpfs_put_lower_path(upper_dentry, &lower_path);
	dput(upper_dentry);
	dput(lower_dentry);
	if (name)
		kfree(name);
	if (version)
		kfree(version);
	if (version_int)
		kfree(version_int);
	if (oldest)
		kfree(oldest);
	if (oldest_int)
		kfree(oldest_int);
	return err;
return err;
}
/*
	Added create_file trigger at write.
	This makes sure there is a backup every time write is called.
*/
static ssize_t bkpfs_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	int err;

	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;

	lower_file = bkpfs_lower_file(file);
	err = vfs_write(lower_file, buf, count, ppos);
	/* update our inode times+sizes upon a successful lower write */
	if (err >= 0) {
		fsstack_copy_inode_size(d_inode(dentry),
					file_inode(lower_file));
		fsstack_copy_attr_times(d_inode(dentry),
					file_inode(lower_file));
	}
	err = create_file(file);
	return err;
}

static int bkpfs_readdir(struct file *file, struct dir_context *ctx)
{
	int err;
	struct file *lower_file = NULL;
	struct dentry *dentry = file->f_path.dentry;

	lower_file = bkpfs_lower_file(file);
	err = iterate_dir(lower_file, ctx);
	file->f_pos = lower_file->f_pos;
	if (err >= 0)		/* copy the atime */
		fsstack_copy_attr_atime(d_inode(dentry),
					file_inode(lower_file));
	return err;
}
int readFile(struct file *fileRead, void *buffer, int size)
{
	int dataRead;
	mm_segment_t old_fs;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	dataRead = vfs_read(fileRead, buffer, size, &(fileRead->f_pos));
	set_fs(old_fs);
	return dataRead;
}
/*
	IOCTL FUNCTIONALITIES.
	List, View, Restore and Delete.
*/
static long bkpfs_unlocked_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	long err = -ENOTTY;
	struct file *lower_file, *fileIn;
	struct qstr this;
	struct dentry *original_file_dentry, *original_file_directory_dentry;
	struct dentry *file_dentry;
	struct path fileInPath;
	struct inode *filepath_inode;
	unsigned long *version_int = kmalloc(sizeof(long), GFP_KERNEL);
	unsigned long *oldest_int = kmalloc(sizeof(long), GFP_KERNEL);
	char *version = kmalloc(sizeof(char)*100, GFP_KERNEL);
	char *oldest = kmalloc(sizeof(char)*100, GFP_KERNEL);
	char *name = kmalloc(sizeof(char)*255, GFP_KERNEL);
	char *buffer = kmalloc(sizeof(char)*PAGE_SIZE, GFP_KERNEL);
	int attr_err = 0, value, dataRead;
	int sizeOfFile, iterationCount, leftBytes;
	lower_file = bkpfs_lower_file(file);
	original_file_dentry = file->f_path.dentry;
	original_file_directory_dentry = dget_parent(original_file_dentry);
	switch (cmd) {
	case list:
			vfs_getxattr(lower_file->f_path.dentry, "user.version", version, 100*sizeof(char));
			attr_err = kstrtoul(version, 10, version_int);
			err = copy_to_user((int *) arg, version_int, sizeof(version_int));
			break;
	case list_2:
			vfs_getxattr(lower_file->f_path.dentry, "user.oldest", oldest, 100*sizeof(char));
			attr_err = kstrtoul(oldest, 10, oldest_int);
			err = copy_to_user((int *) arg, oldest_int, sizeof(oldest_int));
			break;
	case restore:
			err = copy_from_user(&value, (int *) arg, sizeof(value));
			vfs_getxattr(lower_file->f_path.dentry, "user.version", version, 100*sizeof(char));
			attr_err = kstrtoul(version, 10, version_int);
			vfs_getxattr(lower_file->f_path.dentry, "user.oldest", oldest, 100*sizeof(char));
			attr_err = kstrtoul(oldest, 10, oldest_int);
			strcpy(name, ".");
			strcat(name, original_file_dentry->d_iname);
			strcat(name, ".bkp.");
			if (value == -1) {
			strcat(name, version);
			} else if (value == -2) {
				strcat(name, oldest);
			} else {
				if (value >= *oldest_int && value <= *version_int) {
					snprintf (version, 100*sizeof(char), "%d", value);
					strcat(name, version);
				} else {
					printk(KERN_ERR "VERSION DOES NOT EXIST\n");
					break;
				}
			}
			this.name = name;
			this.len = strlen(name);
			this.hash = full_name_hash(original_file_directory_dentry, this.name, this.len);
			file_dentry = d_lookup(original_file_directory_dentry, &this);
			err = copy_file_ioctl(original_file_dentry, file_dentry);
			break;
	case delete_bkp:
			err = copy_from_user(&value, (int *) arg, sizeof(value));
			strcpy(name, ".");
			strcat(name, original_file_dentry->d_iname);
			strcat(name, ".bkp.");
			vfs_getxattr(lower_file->f_path.dentry, "user.version", version, 100*sizeof(char));
			attr_err = kstrtoul(version, 10, version_int);
			vfs_getxattr(lower_file->f_path.dentry, "user.oldest", oldest, 100*sizeof(char));
			attr_err = kstrtoul(oldest, 10, oldest_int);

			if (value != -3) {
				if (value == -1) {
					strcat(name, version);
					*version_int -= 1;
					if ((*version_int-*oldest_int) < 0)
						*version_int = 0;
					snprintf (version, 100*sizeof(char), "%lu", *version_int);
					vfs_setxattr(file->f_path.dentry, "user.version", version, 100*sizeof(char), XATTR_REPLACE);
				}
				if (value == -2) {
					strcat(name, oldest);
					*oldest_int += 1;
					if ((*version_int-*oldest_int) < 0)
						*version_int = 0;
					snprintf (oldest, 100*sizeof(char), "%lu", *oldest_int);
					vfs_setxattr(file->f_path.dentry, "user.oldest", oldest, 100*sizeof(char), XATTR_REPLACE);
				}
				this.name = name;
				this.len = strlen(name);
				this.hash = full_name_hash(original_file_directory_dentry, this.name, this.len);
				file_dentry = d_lookup(original_file_directory_dentry, &this);
				filepath_inode = d_inode(original_file_directory_dentry);
				err = delete_backup(filepath_inode, file_dentry);
			} else {
				for (iterationCount = *oldest_int; iterationCount <= *version_int; iterationCount++) {
				snprintf (version, 100*sizeof(char), "%d", iterationCount);
				strcpy(name, ".");
				strcat(name, original_file_dentry->d_iname);
				strcat(name, ".bkp.");
				strcat(name, version);
				printk("%s\n", name);
				this.name = name;
				this.len = strlen(name);
				this.hash = full_name_hash(original_file_directory_dentry, this.name, this.len);
				file_dentry = d_lookup(original_file_directory_dentry, &this);
				filepath_inode = d_inode(original_file_directory_dentry);
				err = delete_backup(filepath_inode, file_dentry);
				vfs_setxattr(file->f_path.dentry, "user.oldest", version, 100*sizeof(char), XATTR_REPLACE);
				}
			}
			break;
	/*VERY VERY HACKY.*/
	case view:
			strcpy(name, ".");
			strcat(name, original_file_dentry->d_iname);
			strcat(name, ".bkp.");
			err = copy_from_user(&value, (int *) arg, sizeof(value));
			if (value == -1) {
				vfs_getxattr(lower_file->f_path.dentry, "user.version", version, 100*sizeof(char));
				strcat(name, version);
			} else if (value == -2) {
				vfs_getxattr(lower_file->f_path.dentry, "user.oldest", oldest, 100*sizeof(char));
				strcat(name, oldest);
			} else {
				if (value >= *oldest_int && value <= *version_int) {
					snprintf (version, 100*sizeof(char), "%d", value);
					strcat(name, version);
				} else {
					printk(KERN_ERR "VERSION DOES NOT EXIST\n");
					break;
				}
			}
			vfs_setxattr(file->f_path.dentry, "user.viewversion", name, 255*sizeof(char), 0);
			break;
	case view_2:
			vfs_getxattr(lower_file->f_path.dentry, "user.viewversion", name, 255*sizeof(char));
			this.name = name;
			this.len = strlen(name);
			this.hash = full_name_hash(original_file_directory_dentry, this.name, this.len);
			file_dentry = d_lookup(original_file_directory_dentry, &this);
			bkpfs_get_lower_path(file_dentry, &fileInPath);
			fileIn = dentry_open(&fileInPath, O_RDONLY, current_cred());
			sizeOfFile = fileIn->f_inode->i_size;
			iterationCount = (sizeOfFile)/PAGE_SIZE;
			leftBytes = (sizeOfFile)%PAGE_SIZE;
			while (iterationCount > 0) {
				dataRead = readFile(fileIn, buffer, PAGE_SIZE);
				iterationCount -= 1 ;
			}
			if (leftBytes > 0)
				dataRead = readFile(fileIn, buffer, leftBytes);
			err = copy_to_user((int *) arg, buffer, sizeof(buffer));
			break;
		}
	/* XXX: use vfs_ioctl if/when VFS exports it */
	if (!lower_file || !lower_file->f_op)
		goto out;
	if (lower_file->f_op->unlocked_ioctl)
		err = lower_file->f_op->unlocked_ioctl(lower_file, cmd, arg);

	/* some ioctls can change inode attributes (EXT2_IOC_SETFLAGS) */
	if (!err)
		fsstack_copy_attr_all(file_inode(file),
				      file_inode(lower_file));
out:
	if (version)
		kfree(version);
	if (version_int)
		kfree(version_int);
	if (oldest)
		kfree(oldest);
	if (oldest_int)
		kfree(oldest_int);
	if (name)
		kfree(name);
	if (buffer)
		kfree(buffer);
	return err;
}

#ifdef CONFIG_COMPAT
static long bkpfs_compat_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	long err = -ENOTTY;
	struct file *lower_file;
	lower_file = bkpfs_lower_file(file);
	/* XXX: use vfs_ioctl if/when VFS exports it */
	if (!lower_file || !lower_file->f_op)
		goto out;
	if (lower_file->f_op->compat_ioctl)
		err = lower_file->f_op->compat_ioctl(lower_file, cmd, arg);

out:
	return err;
}
#endif

static int bkpfs_mmap(struct file *file, struct vm_area_struct *vma)
{
	int err = 0;
	bool willwrite;
	struct file *lower_file;
	const struct vm_operations_struct *saved_vm_ops = NULL;

	/* this might be deferred to mmap's writepage */
	willwrite = ((vma->vm_flags | VM_SHARED | VM_WRITE) == vma->vm_flags);

	/*
	 * File systems which do not implement ->writepage may use
	 * generic_file_readonly_mmap as their ->mmap op.  If you call
	 * generic_file_readonly_mmap with VM_WRITE, you'd get an -EINVAL.
	 * But we cannot call the lower ->mmap op, so we can't tell that
	 * writeable mappings won't work.  Therefore, our only choice is to
	 * check if the lower file system supports the ->writepage, and if
	 * not, return EINVAL (the same error that
	 * generic_file_readonly_mmap returns in that case).
	 */
	lower_file = bkpfs_lower_file(file);
	if (willwrite && !lower_file->f_mapping->a_ops->writepage) {
		err = -EINVAL;
		printk(KERN_ERR "bkpfs: lower file system does not "
		       "support writeable mmap\n");
		goto out;
	}

	/*
	 * find and save lower vm_ops.
	 *
	 * XXX: the VFS should have a cleaner way of finding the lower vm_ops
	 */
	if (!BKPFS_F(file)->lower_vm_ops) {
		err = lower_file->f_op->mmap(lower_file, vma);
		if (err) {
			printk(KERN_ERR "bkpfs: lower mmap failed %d\n", err);
			goto out;
		}
		saved_vm_ops = vma->vm_ops; /* save: came from lower ->mmap */
	}

	/*
	 * Next 3 lines are all I need from generic_file_mmap.  I definitely
	 * don't want its test for ->readpage which returns -ENOEXEC.
	 */
	file_accessed(file);
	vma->vm_ops = &bkpfs_vm_ops;

	file->f_mapping->a_ops = &bkpfs_aops; /* set our aops */
	if (!BKPFS_F(file)->lower_vm_ops) /* save for our ->fault */
		BKPFS_F(file)->lower_vm_ops = saved_vm_ops;

out:
	return err;
}

static int bkpfs_open(struct inode *inode, struct file *file)
{
	int err = 0;
	struct file *lower_file = NULL;
	struct path lower_path;
	/* don't open unhashed/deleted files */
	if (d_unhashed(file->f_path.dentry)) {
		err = -ENOENT;
		goto out_err;
	}

	file->private_data =
		kzalloc(sizeof(struct bkpfs_file_info), GFP_KERNEL);
	if (!BKPFS_F(file)) {
		err = -ENOMEM;
		goto out_err;
	}

	/* open lower object and link bkpfs's file struct to lower's */
	bkpfs_get_lower_path(file->f_path.dentry, &lower_path);
	lower_file = dentry_open(&lower_path, file->f_flags, current_cred());
	path_put(&lower_path);
	if (IS_ERR(lower_file)) {
		err = PTR_ERR(lower_file);
		lower_file = bkpfs_lower_file(file);
		if (lower_file) {
			bkpfs_set_lower_file(file, NULL);
			fput(lower_file); /* fput calls dput for lower_dentry */
		}
	} else {
		bkpfs_set_lower_file(file, lower_file);
	}

	if (err)
		kfree(BKPFS_F(file));
	else
		fsstack_copy_attr_all(inode, bkpfs_lower_inode(inode));
out_err:
	return err;
}

static int bkpfs_flush(struct file *file, fl_owner_t id)
{
	int err = 0;
	struct file *lower_file = NULL;

	lower_file = bkpfs_lower_file(file);
	if (lower_file && lower_file->f_op && lower_file->f_op->flush) {
		filemap_write_and_wait(file->f_mapping);
		err = lower_file->f_op->flush(lower_file, id);
	}

	return err;
}

/* release all lower object references & free the file info structure */
static int bkpfs_file_release(struct inode *inode, struct file *file)
{
	struct file *lower_file;

	lower_file = bkpfs_lower_file(file);
	if (lower_file) {
		bkpfs_set_lower_file(file, NULL);
		fput(lower_file);
	}

	kfree(BKPFS_F(file));
	return 0;
}

static int bkpfs_fsync(struct file *file, loff_t start, loff_t end,
			int datasync)
{
	int err;
	struct file *lower_file;
	struct path lower_path;
	struct dentry *dentry = file->f_path.dentry;

	err = __generic_file_fsync(file, start, end, datasync);
	if (err)
		goto out;
	lower_file = bkpfs_lower_file(file);
	bkpfs_get_lower_path(dentry, &lower_path);
	err = vfs_fsync_range(lower_file, start, end, datasync);
	bkpfs_put_lower_path(dentry, &lower_path);
out:
	return err;
}

static int bkpfs_fasync(int fd, struct file *file, int flag)
{
	int err = 0;
	struct file *lower_file = NULL;

	lower_file = bkpfs_lower_file(file);
	if (lower_file->f_op && lower_file->f_op->fasync)
		err = lower_file->f_op->fasync(fd, lower_file, flag);

	return err;
}

/*
 * Wrapfs cannot use generic_file_llseek as ->llseek, because it would
 * only set the offset of the upper file.  So we have to implement our
 * own method to set both the upper and lower file offsets
 * consistently.
 */
static loff_t bkpfs_file_llseek(struct file *file, loff_t offset, int whence)
{
	int err;
	struct file *lower_file;

	err = generic_file_llseek(file, offset, whence);
	if (err < 0)
		goto out;

	lower_file = bkpfs_lower_file(file);
	err = generic_file_llseek(lower_file, offset, whence);

out:
	return err;
}

/*
 * Wrapfs read_iter, redirect modified iocb to lower read_iter
 */
ssize_t
bkpfs_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	int err;
	struct file *file = iocb->ki_filp, *lower_file;

	lower_file = bkpfs_lower_file(file);
	if (!lower_file->f_op->read_iter) {
		err = -EINVAL;
		goto out;
	}

	get_file(lower_file); /* prevent lower_file from being released */
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->read_iter(iocb, iter);
	iocb->ki_filp = file;
	fput(lower_file);
	/* update upper inode atime as needed */
	if (err >= 0 || err == -EIOCBQUEUED)
		fsstack_copy_attr_atime(d_inode(file->f_path.dentry),
					file_inode(lower_file));
out:
	return err;
}

/*
 * Wrapfs write_iter, redirect modified iocb to lower write_iter
 */
ssize_t
bkpfs_write_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	int err;
	struct file *file = iocb->ki_filp, *lower_file;

	lower_file = bkpfs_lower_file(file);
	if (!lower_file->f_op->write_iter) {
		err = -EINVAL;
		goto out;
	}

	get_file(lower_file); /* prevent lower_file from being released */
	iocb->ki_filp = lower_file;
	err = lower_file->f_op->write_iter(iocb, iter);
	iocb->ki_filp = file;
	fput(lower_file);
	/* update upper inode times/sizes as needed */
	if (err >= 0 || err == -EIOCBQUEUED) {
		fsstack_copy_inode_size(d_inode(file->f_path.dentry),
					file_inode(lower_file));
		fsstack_copy_attr_times(d_inode(file->f_path.dentry),
					file_inode(lower_file));
	}
out:
	return err;
}


const struct file_operations bkpfs_main_fops = {
	.llseek		= generic_file_llseek,
	.read		= bkpfs_read,
	.write		= bkpfs_write,
	.unlocked_ioctl	= bkpfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= bkpfs_compat_ioctl,
#endif
	.mmap		= bkpfs_mmap,
	.open		= bkpfs_open,
	.flush		= bkpfs_flush,
	.release	= bkpfs_file_release,
	.fsync		= bkpfs_fsync,
	.fasync		= bkpfs_fasync,
	.read_iter	= bkpfs_read_iter,
	.write_iter	= bkpfs_write_iter,
};

/* trimmed directory options */
const struct file_operations bkpfs_dir_fops = {
	.llseek		= bkpfs_file_llseek,
	.read		= generic_read_dir,
	.iterate	= bkpfs_readdir,
	.unlocked_ioctl	= bkpfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= bkpfs_compat_ioctl,
#endif
	.open		= bkpfs_open,
	.release	= bkpfs_file_release,
	.flush		= bkpfs_flush,
	.fsync		= bkpfs_fsync,
	.fasync		= bkpfs_fasync,
};