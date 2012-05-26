/* vmemraid skeleton */
/* vmemraid_main.c */
/* by William A. Katsak <wkatsak@cs.rutgers.edu> */
/* for CS 416, Fall 2011, Rutgers University */








/* This sets up some functions for printing output, and tagging the output */
/* with the name of the module */
/* These functions are pr_info(), pr_warn(), pr_err(), etc. and behave */
/* just like printf(). You can search LXR for information on these functions */
/* and other pr_ functions. */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/hdreg.h>
#include "vmemraid.h"

/* Constants to define the number of disks in the array */
/* and the size in HARDWARE sectors of each disk */
/* You should set these values once, right here, and always */
/* reference them via these constants. */
/* Default is 5 disks, each of size 8192 sectors x 4 KB/sector = 32 MB */

#define NUM_DISKS 5
#define DISK_SIZE_SECTORS 8192
#define p_disk 4

/* Pointer for device struct. You should populate this in init */
struct vmemraid_dev *dev;

/*buffer to receive from kernel to be sent to memory*/
static char hw_buffer[VMEMRAID_HW_SECTOR_SIZE];

/*buffer to receive from sectors and be xor'd with hw_buffer to construct parity sector*/
static char p_buffer[VMEMRAID_HW_SECTOR_SIZE];

/*buffer to receive from sectors and be xor'd with p_buffer during a dropped disk in order to reconstruct the dropped disks sectors*/
static char dropped_disk_buffer[VMEMRAID_HW_SECTOR_SIZE];

static void sbd_transfer(struct vmemraid_dev *device, sector_t sector, unsigned long nsect, char *buffer, int write){

	int c;
	for(c=0;c<nsect;c++){
		
	/*Hardware sector converted from sector in the kernel*/
		int hw_sector = (sector+c) / 8;		/* 8 for the conversion factor between sector sizes*/
		
	/*Disk to write on ...modified later for parity drive location*/
		int disk = (hw_sector) % (NUM_DISKS-1);

	/*Row on the disk to write on ...modified later for parity drive location*/
		int disk_sector = (hw_sector) / (NUM_DISKS-1);

	/*Where to write in the HW sector*/
		int offset = ((sector+c) % 8) * KERNEL_SECTOR_SIZE;
		
		
		if ( ((sector+nsect)*KERNEL_SECTOR_SIZE) >device->size)
		{
			printk (KERN_NOTICE "vmemraid: Beyond-end write\n");		
			return;
		}
		
		read_from_disk(disk_sector, disk);
		if(write){
			memcpy(hw_buffer+offset, buffer+(c*KERNEL_SECTOR_SIZE), KERNEL_SECTOR_SIZE);
			write_to_disk(disk_sector,disk);
		}
		else
			memcpy(buffer+(c*KERNEL_SECTOR_SIZE), hw_buffer+offset, KERNEL_SECTOR_SIZE);
	}	
}	

static void read_from_disk(int disk_sector, int disk){
	
	if(dev->active_disk[disk] == 0)
		read_with_a_dropped_disk(disk_sector, disk);
	else
	memdisk_read_sector(dev->disk_array->disks[disk], hw_buffer, disk_sector);
	
	
}

static void read_with_a_dropped_disk(int disk_sector, int disk){
	int i;
	memdisk_read_sector(dev->disk_array->disks[p_disk], hw_buffer, disk_sector);
	for(i=0;i<(p_disk);i++){
		if(dev->active_disk[i]==1){
			memdisk_read_sector(dev->disk_array->disks[i], p_buffer, disk_sector);
			XOR(hw_buffer, p_buffer, hw_buffer);
		}
		else
			continue;
	}
}

/* BITWISE XOR OF TWO INPUTS STORED IN RESULT*/
static void XOR(char* input1, char* input2, char* result) {
	int i;
	for (i = 0; i < VMEMRAID_HW_SECTOR_SIZE; i++) {
		result[i] = input1[i] ^ input2[i];
	}
}

static void write_to_disk(int disk_sector, int disk){
	int i;
	
	for(i=0;i<NUM_DISKS;i++){	
		if(dev->active_disk[i]==0){
			write_with_a_dropped_disk(disk_sector, disk);
			return;
		}
	}
		
	memdisk_write_sector(dev->disk_array->disks[disk], hw_buffer, disk_sector);
	write_to_parity(disk_sector);
	return;
	
}

static void write_with_a_dropped_disk(int disk_sector, int disk){	
	int i;
	
	/*if the parity disk is the dropped disk*/
	if(dev->active_disk[p_disk]==0){
		memdisk_write_sector(dev->disk_array->disks[disk], hw_buffer, disk_sector);
		return;
	}
	
	/*if the disk to be written to is active*/
	if(dev->active_disk[disk]==1){
		memdisk_read_sector(dev->disk_array->disks[p_disk], p_buffer, disk_sector);	
		for(i=0;i<(NUM_DISKS-1);i++){
			if(dev->active_disk[i]==1){		
				memdisk_read_sector(dev->disk_array->disks[i],dropped_disk_buffer, disk_sector);
				XOR(p_buffer, dropped_disk_buffer, p_buffer);
			}
			else
				continue;
		}
		memdisk_write_sector(dev->disk_array->disks[disk], hw_buffer, disk_sector);
		
		for(i=0;i<(NUM_DISKS-1);i++){
			if(dev->active_disk[i]==1){
				memdisk_read_sector(dev->disk_array->disks[i], dropped_disk_buffer, disk_sector);
				XOR(p_buffer, dropped_disk_buffer, p_buffer);
			}
			else
				continue;
		}
		memdisk_write_sector(dev->disk_array->disks[p_disk], p_buffer, disk_sector);	
		return;	
	}
	
	/*if the disk to be written to is dropped*/
	if(dev->active_disk[disk]==0){
		for(i=0;i<(NUM_DISKS-1);i++){
			if(dev->active_disk[i]==1){		
				memdisk_read_sector(dev->disk_array->disks[i],p_buffer, disk_sector);
				XOR(p_buffer, hw_buffer, hw_buffer);
			}
			else
				continue;
		}
		memdisk_write_sector(dev->disk_array->disks[p_disk], hw_buffer, disk_sector);		
		return;
	}	
}

static void write_to_parity(int disk_sector){
	int i=0;
	memdisk_read_sector(dev->disk_array->disks[i], p_buffer, disk_sector);
	for(i=1;i<(NUM_DISKS-1);i++){		
		memdisk_read_sector(dev->disk_array->disks[i], hw_buffer, disk_sector);
		XOR(p_buffer, hw_buffer, p_buffer);
	}
	memdisk_write_sector(dev->disk_array->disks[p_disk], p_buffer, disk_sector);
	
}

/* Request function. This is registered with the block API and gets */
/* called whenever someone wants data from your device */
static void vmemraid_request(struct request_queue *q){
	
	struct request *req;

	req = blk_fetch_request(q);
	while (req != NULL) {

		if (req == NULL || (req->cmd_type != REQ_TYPE_FS)) {
			printk (KERN_NOTICE "Skip non-CMD request\n");
			__blk_end_request_all(req, -EIO);
			continue;
		}

		sbd_transfer(dev, blk_rq_pos(req), blk_rq_cur_sectors(req),req->buffer, rq_data_dir(req));
		if ( ! __blk_end_request_cur(req, 0) ) {
			req = blk_fetch_request(q);
		}
	}

}

/* Open function. Gets called when the device file is opened */
static int vmemraid_open(struct block_device *block_device, fmode_t mode)
{	
	return 0;
}

/* Release function. Gets called when the device file is closed */
static int vmemraid_release(struct gendisk *gd, fmode_t mode)
{
	return 0;
}

/* getgeo function. Provides device "geometry". This should be */
/* the old cylinders:heads:sectors type geometry. As long as you */
/* populate dev->size with the total usable *****bytes***** of your disk */
/* this implementation will work */
int vmemraid_getgeo(struct block_device *block_device, struct hd_geometry *geo)
{
	long size;

	size = dev->size / KERNEL_SECTOR_SIZE;
	geo->cylinders = (size & ~0x3f) >> 6;
	geo->heads = 4;
	geo->sectors = 16;
	geo->start = 0;

	return 0;
}

/* This gets called when a disk is dropped from the array */
void vmemraid_callback_drop_disk(int disk_num){
	dev->active_disk[disk_num]=0;
}

/* This gets called when a dropped disk is replaced with a new one */
/* NOTE: This will be called with dev->lock HELD */
int vmemraid_callback_new_disk(int disk_num){
	int i;
	int g; 
	int first_time_through_loop = 1;
	int n = memdisk_num_sectors(dev->disk_array->disks[disk_num]);
	
	for(g=0;g<n;g++){
		first_time_through_loop = 1;
		for(i=0;i<(NUM_DISKS);i++){
			if(dev->active_disk[i]==1){		
				memdisk_read_sector(dev->disk_array->disks[i], p_buffer,g);
			}
			else
				continue;
			if(first_time_through_loop)
			{
				memcpy(dropped_disk_buffer,p_buffer,VMEMRAID_HW_SECTOR_SIZE);
				first_time_through_loop = 0;
			}
			else
				XOR(p_buffer, dropped_disk_buffer, dropped_disk_buffer);
				
		}
		memdisk_write_sector(dev->disk_array->disks[disk_num], dropped_disk_buffer, g);
	}
	
	dev->active_disk[disk_num]=1;
	
	return 0;

}

/* This structure must be passed the the block driver API when the */
/* device is registered */
static struct block_device_operations vmemraid_ops = {
	.owner			= THIS_MODULE,
	.open			= vmemraid_open,
	.release		= vmemraid_release,
	.getgeo			= vmemraid_getgeo,
	/* do not tamper with or attempt to replace this entry for ioctl */
	.ioctl		= vmemraid_ioctl
};

/* Init function */
/* This is executed when the module is loaded. Should result in a usable */
/* driver that is registered with the system */
/* NOTE: This is where you should allocate the disk array */
static int __init vmemraid_init(void){
	int i;
			
/*Allocate space for vmemraid_dev*/
	dev = vmalloc(sizeof(struct vmemraid_dev));
	
/*initialize active disk array to default "active" value*/

	for(i=0;i<NUM_DISKS;i++)
		dev->active_disk[i]=1;
		
/*Setup device size*/
	dev->size = DISK_SIZE_SECTORS*VMEMRAID_HW_SECTOR_SIZE*(NUM_DISKS-1);
/*Initialize lock*/
	spin_lock_init(&dev->lock);
/*Initialize major*/
	dev->major = 0;
/*Create disk array (unsigned num_disks, unsigned disk_size_bytes)*/
	dev->disk_array = create_disk_array(NUM_DISKS, DISK_SIZE_SECTORS);
	if(dev->disk_array == NULL) {
		printk(KERN_WARNING "vmemraid: not enough space\n");
		return -ENOMEM;
	}

/*Allocation of request queue*/
	dev->queue = blk_init_queue(vmemraid_request, &dev->lock);
		if(dev->queue==NULL){
		printk(KERN_WARNING "vmemraid: unable to get request queue\n");
		destroy_disk_array(dev->disk_array);
		vfree(dev);
		return -ENOMEM;
}
	blk_queue_logical_block_size(dev->queue,KERNEL_SECTOR_SIZE);

/*Register with kernel*/
	dev->major = register_blkdev(dev->major, "vmemraid");
	if(dev->major <= 0) {
		printk(KERN_WARNING "vmemraid: unable to get major number\n");
		destroy_disk_array(dev->disk_array);
		blk_cleanup_queue(dev->queue);
		vfree(dev);
		return -ENOMEM;
	}

/*Gen disk structure*/
	dev->gd=alloc_disk(VMEMRAID_NUM_MINORS);
	if(!dev->gd) {
		unregister_blkdev(dev->major,"vmemraid");
		destroy_disk_array(dev->disk_array);
		blk_cleanup_queue(dev->queue);
		vfree(dev);
		return -ENOMEM;
	}
	dev->gd->major=dev->major;
	dev->gd->first_minor=0;
	dev->gd->fops=&vmemraid_ops;
	dev->gd->private_data=dev;
	strcpy(dev->gd->disk_name, "vmemraid");
	set_capacity(dev->gd, DISK_SIZE_SECTORS*(NUM_DISKS-1)*8);
	dev->gd->queue = dev->queue;
	add_disk(dev->gd);	
	return 0;
}

/* Exit function */
/* This is executed when the module is unloaded. This must free any and all */
/* memory that is allocated inside the module and unregister the device from */
/* the system. */
static void __exit vmemraid_exit(void)
{
	del_gendisk(dev->gd);
	put_disk(dev->gd);
	unregister_blkdev(dev->major,"vmemraid");
	blk_cleanup_queue(dev->queue);
	destroy_disk_array(dev->disk_array);
	vfree(dev);
}

/* Tell the module system where the init and exit points are. */
module_init(vmemraid_init);
module_exit(vmemraid_exit);

/* Declare the license for the module to be GPL. This can be important in some */
/* cases, as certain kernel functions are only exported to GPL modules. */
MODULE_LICENSE("GPL");

