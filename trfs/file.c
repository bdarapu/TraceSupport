/*
 * Copyright (c) 1998-2015 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2015 Stony Brook University
 * Copyright (c) 2003-2015 The Research Foundation of SUNY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "trfs.h"
#include "../../hw2/trctl.h"




//static DEFINE_MUTEX(buffer_lock);


static int write_file(struct file *file,char *buff, int len){
	int ret;
	mm_segment_t fs;
	fs = get_fs();
	set_fs(get_ds());
	
	ret = vfs_write(file,buff,len,&file->f_pos);

	set_fs(fs);
	return ret;
}

static ssize_t trfs_read(struct file *file, char __user *buf,
			   size_t count, loff_t *ppos)
{
	int err;
	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;
	int ioctl_flag;

	struct trfs_sb_info *sb_info = (struct trfs_sb_info *)file->f_inode->i_sb->s_fs_info;
	
	

	char type = 'r';
	char *buffcpy = sb_info->buffer;
	u16 size = sizeof(size);
	struct trfs_file_info *fp_info= (struct trfs_file_info *)file->private_data;
	int open_record_id = fp_info->record_id;
	char *buff = NULL;
	
	if(sb_info->bitmap & 0x02) //setting the ioctl_flag based upon the bitmap value saved in sb's private data
		ioctl_flag = 1;
	else
		ioctl_flag = 0;
	
	lower_file = trfs_lower_file(file);
	err = vfs_read(lower_file, buf, count, ppos);
	/* update our inode atime upon a successful lower read */
	if (err >= 0)
		fsstack_copy_attr_atime(d_inode(dentry),
					file_inode(lower_file));

	//calculating the size of the record
	if (err>0) 
	{
		size = size + sizeof(sb_info->record_id) + sizeof(type) + sizeof(open_record_id) + sizeof(count) + err + sizeof(err);
	}
	else{
		size = size + sizeof(sb_info->record_id) + sizeof(type) + sizeof(open_record_id) + sizeof(count) + sizeof(err);
	}


	mutex_lock(&sb_info->buffer_lock);   // locking the critical section so that it can't be modified parallelly
	//
	if(ioctl_flag){
		if (size<4096 && size>0)
		{
			/* writing the contents of the record to the buffer and writing to the tfile eventually */

			memcpy(sb_info->buffer,&size,sizeof(size));
			sb_info->buffer = sb_info->buffer + sizeof(size);

			memcpy(sb_info->buffer,&(sb_info->record_id),sizeof(sb_info->record_id));

			sb_info->buffer = sb_info->buffer + sizeof(sb_info->record_id);
			sb_info->record_id = sb_info->record_id + 1;
		
			memcpy(sb_info->buffer,&type,sizeof(type));

			sb_info->buffer = sb_info->buffer + sizeof(type);

			memcpy(sb_info->buffer,&open_record_id,sizeof(open_record_id));

			sb_info->buffer = sb_info->buffer + sizeof(open_record_id);

			memcpy(sb_info->buffer,&count,sizeof(count));

			sb_info->buffer = sb_info->buffer + sizeof(count);


			memcpy(sb_info->buffer,&err,sizeof(err));

			sb_info->buffer = sb_info->buffer + sizeof(err);

			if(err>0){
				buff = kzalloc(err,GFP_KERNEL);

					if(copy_from_user(buff,buf,err)){
						printk("copy_from_user Failed!");	
					}

				memcpy(sb_info->buffer,buff,err);

				sb_info->buffer = sb_info->buffer + err;

			}
		
			write_file(sb_info->tf,buffcpy,size);

			sb_info->buffer = buffcpy;
			memset(sb_info->buffer,0,4096);
		
		}
	}	
	mutex_unlock(&sb_info->buffer_lock);

	if (buff)
	{
		kfree(buff);
	}
	return err;
}

static ssize_t trfs_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	int err;

	struct file *lower_file;
	struct dentry *dentry = file->f_path.dentry;
	int ioctl_flag;
	
	struct trfs_sb_info *sb_info = (struct trfs_sb_info *)file->f_inode->i_sb->s_fs_info;

	
	char type = 'w';
	char *buffcpy = sb_info->buffer;
	u16 size = sizeof(size);
	struct trfs_file_info *fp_info= (struct trfs_file_info *)file->private_data;
	int open_record_id = fp_info->record_id;
	char *buff = NULL;
	//unsigned short s;


	if(sb_info->bitmap & 0x04)
		ioctl_flag = 1;
	else
		ioctl_flag = 0;

	buff = kzalloc(count,GFP_KERNEL);

	lower_file = trfs_lower_file(file);
	err = vfs_write(lower_file, buf, count, ppos);
	/* update our inode times+sizes upon a successful lower write */
	if (err >= 0) {
		fsstack_copy_inode_size(d_inode(dentry),
					file_inode(lower_file));
		fsstack_copy_attr_times(d_inode(dentry),
					file_inode(lower_file));
	}


	size = size + sizeof(sb_info->record_id) + sizeof(type) + sizeof(open_record_id) + sizeof(count) + count + sizeof(err);
	
	mutex_lock(&sb_info->buffer_lock);
	if(ioctl_flag){
		if (size<4096 && size>0)
		{
			memcpy(sb_info->buffer,&size,sizeof(size));
		
			sb_info->buffer = sb_info->buffer + sizeof(size);

			memcpy(sb_info->buffer,&(sb_info->record_id),sizeof(sb_info->record_id));

			sb_info->buffer = sb_info->buffer + sizeof(sb_info->record_id);
			sb_info->record_id = sb_info->record_id + 1;
		
			memcpy(sb_info->buffer,&type,sizeof(type));

			sb_info->buffer = sb_info->buffer + sizeof(type);

			memcpy(sb_info->buffer,&open_record_id,sizeof(open_record_id));

			sb_info->buffer = sb_info->buffer + sizeof(open_record_id);

			memcpy(sb_info->buffer,&count,sizeof(count));

			sb_info->buffer = sb_info->buffer + sizeof(count);
			//converting user's virtual address to physical address
			if(copy_from_user(buff,buf,count)){
				printk("copy_from_user Failed!");	
			}

			memcpy(sb_info->buffer,buff,count);

			sb_info->buffer = sb_info->buffer + count;

			memcpy(sb_info->buffer,&err,sizeof(err));

			sb_info->buffer = sb_info->buffer + sizeof(err);

			write_file(sb_info->tf,buffcpy,size);

			sb_info->buffer = buffcpy;
			memset(sb_info->buffer,0,4096);
			
		}
	}	
	mutex_unlock(&sb_info->buffer_lock);
	kfree(buff);
	return err;
}

static int trfs_readdir(struct file *file, struct dir_context *ctx)
{
	int err;
	struct file *lower_file = NULL;
	struct dentry *dentry = file->f_path.dentry;

	lower_file = trfs_lower_file(file);
	err = iterate_dir(lower_file, ctx);
	file->f_pos = lower_file->f_pos;
	if (err >= 0)		/* copy the atime */
		fsstack_copy_attr_atime(d_inode(dentry),
					file_inode(lower_file));
	return err;
}

static long trfs_unlocked_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	long err = -ENOTTY;
	struct file *lower_file;
	struct trfs_sb_info *sb_info = (struct trfs_sb_info *)file->f_inode->i_sb->s_fs_info;
	int bitmap=sb_info->bitmap;
	int set_bitmap=0;  // value passed by user
	
	//printk("test test \n");
	

	lower_file = trfs_lower_file(file);

	/* XXX: use vfs_ioctl if/when VFS exports it */
	if (!lower_file || !lower_file->f_op)
		goto out;
	if (lower_file->f_op->unlocked_ioctl)
		err = lower_file->f_op->unlocked_ioctl(lower_file, cmd, arg);

	/* some ioctls can change inode attributes (EXT2_IOC_SETFLAGS) */
	if (!err)
		fsstack_copy_attr_all(file_inode(file),
				      file_inode(lower_file));
					  
	switch(cmd)
	{
		case BITMAP_GET_VALUE:
				
			if (copy_to_user((void *)arg,(void *)&bitmap, sizeof(bitmap))) 
			{
				printk(KERN_ERR	"Line no.:[%d] ERROR in copy_to_user in BITMAP_GET_VALUE\n", __LINE__);
				err = -EFAULT;
				printk("in bitmap get value \n");
				goto out;
			}
			break;
			
		case BITMAP_ALL_VALUE:
			printk("test inside bitmap all \n");
			if (!(void *)arg) 
			{
				printk(KERN_ERR	"Line no.:[%d] ERROR!! arg is null\n", __LINE__);
				err = -EINVAL;
				goto out;
			}
			
			if (copy_from_user((void *)&set_bitmap,(void *) arg, sizeof(set_bitmap)))
			{
				printk(KERN_ERR	"Line no.:[%d] ERROR in copy_from_user in BITMAP_ALL_VALUE\n", __LINE__);
				err = -EFAULT;
				goto out;
			}
			
			bitmap=set_bitmap;
			printk("all -bitmap now set to %d \n",bitmap);
			sb_info->bitmap=set_bitmap;
			break;
			
		case BITMAP_NONE_VALUE:
			printk("in bitmap none \n");
			if (!(void *)arg) 
			{
				printk(KERN_ERR	"Line no.:[%d] ERROR!! arg is null\n", __LINE__);
				err = -EINVAL;
				goto out;
			}
			
			if (copy_from_user((void *)&set_bitmap,(void *) arg, sizeof(set_bitmap)))
			{
				printk(KERN_ERR	"Line no.:[%d] ERROR in copy_from_user in BITMAP_NONE_VALUE\n", __LINE__);
				err = -EFAULT;
				goto out;
			}
			bitmap=set_bitmap;
			printk("none -bitmap  now set to %d \n",bitmap);
			sb_info->bitmap=set_bitmap;
			break;
			
		case BITMAP_HEX_VALUE:
			printk("in bitmap hex \n");
			if (!(void *)arg) 
			{
				printk(KERN_ERR	"Line no.:[%d] ERROR!! arg is null\n", __LINE__);
				err = -EINVAL;
				goto out;
			}
			
			if (copy_from_user((void *)&set_bitmap,(void *) arg, sizeof(set_bitmap)))
			{
				printk(KERN_ERR	"Line no.:[%d] ERROR in copy_from_user in BITMAP_HEX_VALUE\n", __LINE__);
				err = -EFAULT;
				goto out;
			}
			bitmap=set_bitmap;
			printk("hex-bitmap now set to %d \n",bitmap);
			sb_info->bitmap=set_bitmap;
			break;
			
	}
	
out:
	return err;
}

#ifdef CONFIG_COMPAT
static long trfs_compat_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	long err = -ENOTTY;
	struct file *lower_file;

	lower_file = trfs_lower_file(file);

	/* XXX: use vfs_ioctl if/when VFS exports it */
	if (!lower_file || !lower_file->f_op)
		goto out;
	if (lower_file->f_op->compat_ioctl)
		err = lower_file->f_op->compat_ioctl(lower_file, cmd, arg);

out:
	return err;
}
#endif

static int trfs_mmap(struct file *file, struct vm_area_struct *vma)
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
	lower_file = trfs_lower_file(file);
	if (willwrite && !lower_file->f_mapping->a_ops->writepage) {
		err = -EINVAL;
		printk(KERN_ERR "trfs: lower file system does not "
		       "support writeable mmap\n");
		goto out;
	}

	/*
	 * find and save lower vm_ops.
	 *
	 * XXX: the VFS should have a cleaner way of finding the lower vm_ops
	 */
	if (!TRFS_F(file)->lower_vm_ops) {
		err = lower_file->f_op->mmap(lower_file, vma);
		if (err) {
			printk(KERN_ERR "trfs: lower mmap failed %d\n", err);
			goto out;
		}
		saved_vm_ops = vma->vm_ops; /* save: came from lower ->mmap */
	}

	/*
	 * Next 3 lines are all I need from generic_file_mmap.  I definitely
	 * don't want its test for ->readpage which returns -ENOEXEC.
	 */
	file_accessed(file);
	vma->vm_ops = &trfs_vm_ops;

	file->f_mapping->a_ops = &trfs_aops; /* set our aops */
	if (!TRFS_F(file)->lower_vm_ops) /* save for our ->fault */
		TRFS_F(file)->lower_vm_ops = saved_vm_ops;

out:
	return err;
}

static int trfs_open(struct inode *inode, struct file *file)
{
	int err = 0;
	struct file *lower_file = NULL;
	struct path lower_path;
	int ioctl_flag;
	struct trfs_sb_info *sb_info = (struct trfs_sb_info *)inode->i_sb->s_fs_info;
	
	
	//buffer locking syntax
	
		
	
	char *tmp = (char*)__get_free_page(GFP_TEMPORARY);
	char *path = NULL;
	char type = 'o';
	char *buffcpy = sb_info->buffer;
	u16 size=sizeof(size);
	u16 path_size;
	
	if(sb_info->bitmap & 0x01)
		ioctl_flag = 1;
	else
		ioctl_flag = 0;

	//path = d_path(&file->f_path,tmp,PAGE_SIZE);
	// using dentry_path_raw function to get the relative path of the file which will be used in treplay
	path = dentry_path_raw(file->f_path.dentry,tmp,PAGE_SIZE);
	
	//calculating size of the record and removing the / from the path for treplay purposes
	if(path){  
		if(strlen(path)>1){
			printk("path: %s\n",path);
			path = path + 1;
			size = size + sizeof(sb_info->record_id)+sizeof(type)+sizeof(file->f_flags)+sizeof(inode->i_mode)+sizeof(path_size)+strlen(path)+1+sizeof(err);
		}
	}
	


	/* don't open unhashed/deleted files */
	if (d_unhashed(file->f_path.dentry)) {
		err = -ENOENT;
		goto out_err;
	}

	file->private_data =
		kzalloc(sizeof(struct trfs_file_info), GFP_KERNEL);
	if (!TRFS_F(file)) {
		err = -ENOMEM;
		goto out_err;
	}

	/* open lower object and link trfs's file struct to lower's */
	trfs_get_lower_path(file->f_path.dentry, &lower_path);
	lower_file = dentry_open(&lower_path, file->f_flags, current_cred());
	path_put(&lower_path);
	if (IS_ERR(lower_file)) {
		err = PTR_ERR(lower_file);
		lower_file = trfs_lower_file(file);
		if (lower_file) {
			trfs_set_lower_file(file, NULL);
			fput(lower_file); /* fput calls dput for lower_dentry */
		}
	} else {
		trfs_set_lower_file(file, lower_file);
	}
	
	//setting the record id of the open function in the file's private data, so that it can be used as key for looking up fd in read and write treplays

	if(ioctl_flag){   
		if (size<4096 && size>sizeof(size) )
		{
			trfs_set_record(file,sb_info->record_id);
		}
		else{
			trfs_set_record(file,-1);
		}
	}else{
			trfs_set_record(file,-1);
	}

	if (err)
		kfree(TRFS_F(file));
	else
		fsstack_copy_attr_all(inode, trfs_lower_inode(inode));
out_err:

	mutex_lock(&sb_info->buffer_lock); //locking the critical section
	if(ioctl_flag){
		if (size<4096 && size>sizeof(size) )
		{
		
			memcpy(sb_info->buffer,&size,sizeof(size));

			sb_info->buffer = sb_info->buffer + sizeof(size);

			memcpy(sb_info->buffer,&(sb_info->record_id),sizeof(sb_info->record_id));
		
			sb_info->record_id = sb_info->record_id + 1;
			sb_info->buffer = sb_info->buffer + sizeof(sb_info->record_id);
		
			memcpy(sb_info->buffer,&type,sizeof(type));
		
			sb_info->buffer = sb_info->buffer + sizeof(type);

			memcpy(sb_info->buffer,&(file->f_flags),sizeof(file->f_flags));
		
			sb_info->buffer = sb_info->buffer + sizeof(file->f_flags);
		
			memcpy(sb_info->buffer,&(inode->i_mode),sizeof(inode->i_mode));
		
			sb_info->buffer = sb_info->buffer + sizeof(inode->i_mode);
		
			path_size = strlen(path) + 1;
		
			memcpy(sb_info->buffer, &path_size, sizeof(path_size));

			sb_info->buffer = sb_info->buffer + sizeof(path_size);
		
			memcpy(sb_info->buffer,path, strlen(path) + 1);
		
			sb_info->buffer = sb_info->buffer + strlen(path) + 1;

			memcpy(sb_info->buffer,&err,sizeof(err));
			sb_info->buffer = sb_info->buffer + sizeof(err);
		
		
			write_file(sb_info->tf,buffcpy,size);

			sb_info->buffer = buffcpy;
			memset(sb_info->buffer,0,4096);
		
		}
	}	
	mutex_unlock(&sb_info->buffer_lock);
	free_page((unsigned long)tmp);
	return err;
}

static int trfs_flush(struct file *file, fl_owner_t id)
{
	int err = 0;
	struct file *lower_file = NULL;

	lower_file = trfs_lower_file(file);
	if (lower_file && lower_file->f_op && lower_file->f_op->flush) {
		filemap_write_and_wait(file->f_mapping);
		err = lower_file->f_op->flush(lower_file, id);
	}

	return err;
}

/* release all lower object references & free the file info structure */
static int trfs_file_release(struct inode *inode, struct file *file)
{
	struct file *lower_file;
	int ioctl_flag;
	struct trfs_sb_info *sb_info = (struct trfs_sb_info *)file->f_inode->i_sb->s_fs_info;

	
	char type = 'c';
	char *buffcpy = sb_info->buffer;
	u16 size = sizeof(size);
	struct trfs_file_info *fp_info= (struct trfs_file_info *)file->private_data;
	int open_record_id = fp_info->record_id;


	if(sb_info->bitmap & 0x10)
		ioctl_flag = 1;
	else
		ioctl_flag = 0;

	size = size + sizeof(size) + sizeof(sb_info->record_id) + sizeof(open_record_id) + sizeof(type);

	lower_file = trfs_lower_file(file);
	if (lower_file) {
		trfs_set_lower_file(file, NULL);
		fput(lower_file);
	}

	mutex_lock(&sb_info->buffer_lock);
	if(ioctl_flag){
		if(size<4096 && size>0 && open_record_id!= -1){
			memcpy(sb_info->buffer,&size,sizeof(size));
		
			sb_info->buffer = sb_info->buffer + sizeof(size);

			memcpy(sb_info->buffer,&(sb_info->record_id),sizeof(sb_info->record_id));

			sb_info->buffer = sb_info->buffer + sizeof(sb_info->record_id);
			sb_info->record_id = sb_info->record_id + 1;
		
			memcpy(sb_info->buffer,&type,sizeof(type));

			sb_info->buffer = sb_info->buffer + sizeof(type);

			memcpy(sb_info->buffer,&open_record_id,sizeof(open_record_id));

			sb_info->buffer = sb_info->buffer + sizeof(open_record_id);

			write_file(sb_info->tf,buffcpy,size);

			sb_info->buffer = buffcpy;
			memset(sb_info->buffer,0,4096);

		}	
	}
	mutex_unlock(&sb_info->buffer_lock);
		
	kfree(TRFS_F(file));
	return 0;
}

static int trfs_fsync(struct file *file, loff_t start, loff_t end,
			int datasync)
{
	int err;
	struct file *lower_file;
	struct path lower_path;
	struct dentry *dentry = file->f_path.dentry;

	err = __generic_file_fsync(file, start, end, datasync);
	if (err)
		goto out;
	lower_file = trfs_lower_file(file);
	trfs_get_lower_path(dentry, &lower_path);
	err = vfs_fsync_range(lower_file, start, end, datasync);
	trfs_put_lower_path(dentry, &lower_path);
out:
	return err;
}

static int trfs_fasync(int fd, struct file *file, int flag)
{
	int err = 0;
	struct file *lower_file = NULL;

	lower_file = trfs_lower_file(file);
	if (lower_file->f_op && lower_file->f_op->fasync)
		err = lower_file->f_op->fasync(fd, lower_file, flag);

	return err;
}

/*
 * Trfs cannot use generic_file_llseek as ->llseek, because it would
 * only set the offset of the upper file.  So we have to implement our
 * own method to set both the upper and lower file offsets
 * consistently.
 */
static loff_t trfs_file_llseek(struct file *file, loff_t offset, int whence)
{
	int err;
	struct file *lower_file;

	err = generic_file_llseek(file, offset, whence);
	if (err < 0)
		goto out;

	lower_file = trfs_lower_file(file);
	err = generic_file_llseek(lower_file, offset, whence);

out:
	return err;
}

/*
 * Trfs read_iter, redirect modified iocb to lower read_iter
 */
ssize_t
trfs_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	int err;
	struct file *file = iocb->ki_filp, *lower_file;

	lower_file = trfs_lower_file(file);
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
 * Trfs write_iter, redirect modified iocb to lower write_iter
 */
ssize_t
trfs_write_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	int err;
	struct file *file = iocb->ki_filp, *lower_file;

	lower_file = trfs_lower_file(file);
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


const struct file_operations trfs_main_fops = {
	.llseek		= generic_file_llseek,
	.read		= trfs_read,
	.write		= trfs_write,
	.unlocked_ioctl	= trfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= trfs_compat_ioctl,
#endif
	.mmap		= trfs_mmap,
	.open		= trfs_open,
	.flush		= trfs_flush,
	.release	= trfs_file_release,
	.fsync		= trfs_fsync,
	.fasync		= trfs_fasync,
	.read_iter	= trfs_read_iter,
	.write_iter	= trfs_write_iter,
};

/* trimmed directory options */
const struct file_operations trfs_dir_fops = {
	.llseek		= trfs_file_llseek,
	.read		= generic_read_dir,
	.iterate	= trfs_readdir,
	.unlocked_ioctl	= trfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= trfs_compat_ioctl,
#endif
	.open		= trfs_open,
	.release	= trfs_file_release,
	.flush		= trfs_flush,
	.fsync		= trfs_fsync,
	.fasync		= trfs_fasync,
};
