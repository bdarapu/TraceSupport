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
#include <linux/module.h>

/*
 * There is no need to lock the trfs_super_info's rwsem as there is no
 * way anyone can have a reference to the superblock at this point in time.
 */
static int trfs_read_super(struct super_block *sb, void *raw_data, int silent)
{
	int err = 0;
	struct super_block *lower_sb;
	struct path lower_path;
	char *dev_name = NULL;
	struct inode *inode;
	struct trfs_path_info *tfile = (struct trfs_path_info *)raw_data;
	struct file *fp = NULL;
	char *buffer = NULL; /*buffer for tfile*/
	buffer = kzalloc(4096,GFP_KERNEL);
	if(IS_ERR(buffer)){
		printk(KERN_ERR "Memory Allocation Error! ");
		err = -EINVAL;
		goto out;
	}
	


	fp = filp_open(tfile->tfile_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if(IS_ERR(fp)){
		printk(KERN_ERR "File Open Error! ");
		err = (int) PTR_ERR(fp);
		kfree(buffer);
		goto out;
	}
	
	
	dev_name = tfile->dev_name;

	//validation for tfile->tpath_info pending	

	if (!dev_name) {
		printk(KERN_ERR
		       "trfs: read_super: missing dev_name argument\n");
		err = -EINVAL;
		kfree(buffer);
		filp_close(fp,NULL);
		goto out;
	}

	/* parse lower path */
	err = kern_path(dev_name, LOOKUP_FOLLOW | LOOKUP_DIRECTORY,
			&lower_path);
	if (err) {
		printk(KERN_ERR	"trfs: error accessing "
		       "lower directory '%s'\n", dev_name);
		kfree(buffer);
                filp_close(fp,NULL);
		goto out;
	}

	/* allocate superblock private data */
	sb->s_fs_info = kzalloc(sizeof(struct trfs_sb_info), GFP_KERNEL);
	if (!TRFS_SB(sb)) {
		printk(KERN_CRIT "trfs: read_super: out of memory\n");
		err = -ENOMEM;
		kfree(buffer);
                filp_close(fp,NULL);
		goto out_free;
	}

	/* set the lower superblock field of upper superblock */
	lower_sb = lower_path.dentry->d_sb;
	atomic_inc(&lower_sb->s_active);
	trfs_set_lower_super(sb, lower_sb);
		
	/*adding file path to struct trfs_sb_info, which is stored in private data of SB */
	trfs_set_tfile(sb,fp);

	//referencing allocated buffer to sb's private info struct->buffer
	trfs_set_tbuffer(sb,buffer);
	//storing a copy to the initial location of the buffer for kfreeing purpose
	trfs_set_tbufferc(sb,buffer);
	//setting the default value of the record id counter
	trfs_set_record_id(sb,0);
	
	trfs_set_buffer_size(sb,0);
	//setting the default bitmap value to sb' private info struct
	trfs_set_bitmap(sb,0x7FFFFFFF);
	//initializing the mutex lock saved in sb's private data
	mutex_init(&TRFS_SB(sb)->buffer_lock);	
	
	/* inherit maxbytes from lower file system */
	sb->s_maxbytes = lower_sb->s_maxbytes;

	/*
	 * Our c/m/atime granularity is 1 ns because we may stack on file
	 * systems whose granularity is as good.
	 */
	sb->s_time_gran = 1;

	sb->s_op = &trfs_sops;

	sb->s_export_op = &trfs_export_ops; /* adding NFS support */

	/* get a new inode and allocate our root dentry */
	inode = trfs_iget(sb, d_inode(lower_path.dentry));
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		kfree(buffer);
                filp_close(fp,NULL);
		goto out_sput;
	}
	sb->s_root = d_make_root(inode);
	if (!sb->s_root) {
		err = -ENOMEM;
		kfree(buffer);
                filp_close(fp,NULL);
		goto out_iput;
	}
	d_set_d_op(sb->s_root, &trfs_dops);

	/* link the upper and lower dentries */
	sb->s_root->d_fsdata = NULL;
	err = new_dentry_private_data(sb->s_root);
	if (err){
		kfree(buffer);
                filp_close(fp,NULL);
		goto out_freeroot;
	}

	/* if get here: cannot have error */

	/* set the lower dentries for s_root */
	trfs_set_lower_path(sb->s_root, &lower_path);

	/*
	 * No need to call interpose because we already have a positive
	 * dentry, which was instantiated by d_make_root.  Just need to
	 * d_rehash it.
	 */
	d_rehash(sb->s_root);
	if (!silent)
		printk(KERN_INFO
		       "trfs: mounted on top of %s type %s\n",
		       dev_name, lower_sb->s_type->name);
	goto out; /* all is well */

	/* no longer needed: free_dentry_private_data(sb->s_root); */
out_freeroot:
	dput(sb->s_root);
out_iput:
	iput(inode);
out_sput:
	/* drop refs we took earlier */
	atomic_dec(&lower_sb->s_active);
	kfree(TRFS_SB(sb));
	sb->s_fs_info = NULL;
out_free:
	path_put(&lower_path);

out:
	kfree(tfile->tfile_path);
	kfree(tfile);
	return err;
}

struct dentry *trfs_mount(struct file_system_type *fs_type, int flags,
			    const char *dev_name, void *raw_data)
{
	void *lower_path_name = NULL;
	char *tfile_path = NULL;
	struct trfs_path_info *tfile = NULL;
	int err = 0;
	char *mopt, *tpath;
	
	tpath = (char *)raw_data; //tpath ideally should contain "tfile=/temp/tfile.txt"
	
	if(IS_ERR(tpath)){
		printk(KERN_ERR "Mount Option for tfile not passed\n");
		err = -EINVAL;
		goto out;
	}
	
	/*tokenizing over "=" delimiter* using strsep*/
	mopt = strsep(&tpath, "=");
	
	if (strcmp(mopt,"tfile")!=0){
		printk(KERN_ERR "Mount option should be tfile=/some/file\n" );
		err = -EINVAL;
		goto out;
	}
	//tpath should now point to /temp/tfile.txt
	if(IS_ERR(tpath)){
		printk(KERN_ERR "TFile path not entered!\n");
		err = -EINVAL;
		goto out;
	}
	tfile_path = kzalloc(strlen(tpath),GFP_KERNEL);
	if(IS_ERR(tfile_path)){
		printk(KERN_ERR "Error Allocating Memory to tfile path buffer\n");
		err = -ENOMEM;
		goto out;
	}
	memcpy(tfile_path,tpath, strlen(tpath));
	
	tfile = kzalloc(sizeof(struct trfs_path_info), GFP_KERNEL);
	
	if(IS_ERR(tfile)){
		printk(KERN_ERR " Error allocating memory to struct tfile_path_info\n");
		kfree(tfile_path);
		err= -ENOMEM;
		goto out;
	}
	
	tfile->dev_name = (char *)dev_name;
	tfile->tfile_path = tfile_path;
	lower_path_name = (void *)tfile;
	/*saving dev_name and tfile_path in a struct and passing its address to trfs_read_super*/
	return mount_nodev(fs_type, flags, lower_path_name,
			   trfs_read_super);

out:
	
	return ERR_PTR(err);
}

static struct file_system_type trfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= TRFS_NAME,
	.mount		= trfs_mount,
	.kill_sb	= generic_shutdown_super,
	.fs_flags	= 0,
};
MODULE_ALIAS_FS(TRFS_NAME);

static int __init init_trfs_fs(void)
{
	int err;

	pr_info("Registering trfs " TRFS_VERSION "\n");

	err = trfs_init_inode_cache();
	if (err)
		goto out;
	err = trfs_init_dentry_cache();
	if (err)
		goto out;
	err = register_filesystem(&trfs_fs_type);
out:
	if (err) {
		trfs_destroy_inode_cache();
		trfs_destroy_dentry_cache();
	}
	return err;
}

static void __exit exit_trfs_fs(void)
{
	trfs_destroy_inode_cache();
	trfs_destroy_dentry_cache();
	unregister_filesystem(&trfs_fs_type);
	pr_info("Completed trfs module unload\n");
}

MODULE_AUTHOR("Erez Zadok, Filesystems and Storage Lab, Stony Brook University"
	      " (http://www.fsl.cs.sunysb.edu/)");
MODULE_DESCRIPTION("Trfs " TRFS_VERSION
		   " (http://trfs.filesystems.org/)");
MODULE_LICENSE("GPL");

module_init(init_trfs_fs);
module_exit(exit_trfs_fs);
