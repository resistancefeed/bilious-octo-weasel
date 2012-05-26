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

/* Pointer for device struct. You should populate this in init */
struct vmemraid_dev *dev;

//Temporary char array for rebuilding drive
char strfull[VMEMRAID_HW_SECTOR_SIZE];
//Holds a row of blocks for NUM_DISKS Disks
char disks[NUM_DISKS][VMEMRAID_HW_SECTOR_SIZE];


static void sbd_transfer(struct vmemraid_dev *device, sector_t sector, unsigned long nsect, char *buffer, int write) {

	int numofsect= nsect;
//Current block to write to
	int block = sector/(VMEMRAID_HW_SECTOR_SIZE/VMEMRAID_SECTOR_SIZE);
//Drive to write on ...modified later for parity drive location
	int drive = block%(NUM_DISKS-1);
	int numblocks;
//Where to write in the HW sector size
	int offset = sector%(VMEMRAID_HW_SECTOR_SIZE/VMEMRAID_SECTOR_SIZE);
	int bufferptr=0;
//what row the block is on
	int dblock = block/(NUM_DISKS-1);
//what drive is the acting parity block on
	int pblock = NUM_DISKS-1 - dblock%(NUM_DISKS);
	int i,g=0;
	int ptr;
	memset(disks,0,NUM_DISKS*VMEMRAID_HW_SECTOR_SIZE);
	memset(strfull,0,VMEMRAID_HW_SECTOR_SIZE);
//If parirty block is same as drive block
//Move up to next avaiable drive ... drive+1 or 0 if no more drives left
	if(pblock<=drive)
		drive++;
	if(drive>(NUM_DISKS-1)){
		dblock++;
		drive=0;
		pblock--;
		if(pblock<0)
			pblock=NUM_DISKS-1;
		if(pblock==drive)
			drive++;
	}
	if ((sector+numofsect/(VMEMRAID_SECTOR_SIZE/KERNEL_SECTOR_SIZE))>device->size) {
		printk (KERN_NOTICE "vmemraid: Beyond-end write\n");
		return;
	}
//Copying row of drive blocks over to char array for easier access
	for(ptr=0;ptr<NUM_DISKS;ptr++) {
		if(device->missingdisk==-1||device->missingdisk!=ptr)
			memdisk_read_block(device->disk_array->disks[ptr],disks[ptr],dblock);
//Rebuild drive in row if drive is missing
	}
	if(device->missingdisk!=-1){
		for(g=0;g<VMEMRAID_HW_SECTOR_SIZE;g++){
			for(i=0;i<NUM_DISKS;i++){
				if(i!=device->missingdisk)
					strfull[g]=strfull[g]^disks[i][g];
			}
		}
		memcpy(disks[device->missingdisk],strfull,VMEMRAID_HW_SECTOR_SIZE);
		memset(strfull,0,VMEMRAID_HW_SECTOR_SIZE);
	}
	if (write){
		while(numofsect>0){
			if(offset+numofsect>(VMEMRAID_HW_SECTOR_SIZE/VMEMRAID_SECTOR_SIZE)) {
				numblocks = (VMEMRAID_HW_SECTOR_SIZE/VMEMRAID_SECTOR_SIZE)-offset;
				numofsect=numofsect-numblocks;
				memcpy(disks[drive]+offset*VMEMRAID_SECTOR_SIZE,buffer+bufferptr,numblocks*VMEMRAID_SECTOR_SIZE);
				if(device->missingdisk==-1||device->missingdisk!=drive)
					memdisk_write_block(device->disk_array->disks[drive],disks[drive],dblock);
				drive++;
				if(pblock==drive)
					drive++;
				if(drive>(NUM_DISKS-1)){
			//Compute parity block
					for(g=0;g<VMEMRAID_HW_SECTOR_SIZE;g++){
						for(i=0;i<NUM_DISKS;i++){
							if(i!=pblock)
								strfull[g]=strfull[g]^disks[i][g];
						}
					}
					memcpy(disks[pblock],strfull,VMEMRAID_HW_SECTOR_SIZE);
					memset(strfull,0,VMEMRAID_HW_SECTOR_SIZE);
					if(device->missingdisk==-1||device->missingdisk!=pblock)
						memdisk_read_block(device->disk_array->disks[pblock],disks[pblock],dblock);
					dblock++;
					drive=0;
					pblock--;
					if(pblock<0)
						pblock=NUM_DISKS-1;
					if(pblock==drive)
						drive++;
				}
			//Copying row of drive blocks over to char array for easier access
				for(ptr=0;ptr<NUM_DISKS;ptr++) {
					if(device->missingdisk==-1||device->missingdisk!=ptr)
						memdisk_read_block(device->disk_array->disks[ptr],disks[ptr],dblock);
				}
			///Rebuild drive in row if drive is missing
				if(dev->missingdisk!=-1){
					for(g=0;g<VMEMRAID_HW_SECTOR_SIZE;g++){
						for(i=0;i<NUM_DISKS;i++){
							if(i!=device->missingdisk)
								strfull[g]=strfull[g]^disks[i][g];
						}
					}
					memcpy(disks[device->missingdisk],strfull,VMEMRAID_HW_SECTOR_SIZE);
					memset(strfull,0,VMEMRAID_HW_SECTOR_SIZE);
				}
				offset=0;
				bufferptr=bufferptr+VMEMRAID_SECTOR_SIZE*numblocks;
			}
			else {
				memcpy(disks[drive]+offset*VMEMRAID_SECTOR_SIZE,buffer+bufferptr,numofsect*VMEMRAID_SECTOR_SIZE);
				numofsect=0;
				if(device->missingdisk==-1||device->missingdisk!=drive)
					memdisk_write_block(device->disk_array->disks[drive],disks[drive],dblock);
			//Compute parity block
				for(g=0;g<VMEMRAID_HW_SECTOR_SIZE;g++){
					for(i=0;i<NUM_DISKS;i++){
						if(i!=pblock)
							strfull[g]=strfull[g]^disks[i][g];
					}
				}
				memcpy(disks[pblock],strfull,VMEMRAID_HW_SECTOR_SIZE);
				memset(strfull,0,VMEMRAID_HW_SECTOR_SIZE);
				if(device->missingdisk==-1||device->missingdisk!=pblock)
					memdisk_write_block(device->disk_array->disks[pblock],disks[pblock],dblock);
			}
		}
	}
	else{
		while(numofsect>0){
			if(offset+numofsect>(VMEMRAID_HW_SECTOR_SIZE/VMEMRAID_SECTOR_SIZE)){
				numblocks = (VMEMRAID_HW_SECTOR_SIZE/VMEMRAID_SECTOR_SIZE)-offset;
				numofsect=numofsect-numblocks;
				memcpy(buffer+bufferptr,disks[drive]+VMEMRAID_SECTOR_SIZE*offset,numblocks*VMEMRAID_SECTOR_SIZE);
				drive++;
				if(pblock==drive)
					drive++;
				if(drive>NUM_DISKS-1){
					dblock++;
					drive=0;
					pblock--;
					if(pblock<0)
						pblock=NUM_DISKS-1;
					if(pblock==drive)
						drive++;
			///Rebuild drive in row if drive is missing
					if(device->missingdisk!=-1){
						for(g=0;g<VMEMRAID_HW_SECTOR_SIZE;g++){
							for(i=0;i<NUM_DISKS;i++){
								if(i!=device->missingdisk)
									strfull[g]=strfull[g]^disks[i][g];
							}
						}
						memcpy(disks[device->missingdisk],strfull,VMEMRAID_HW_SECTOR_SIZE);
						memset(strfull,0,VMEMRAID_HW_SECTOR_SIZE);
					}
				}
				for(ptr=0;ptr<NUM_DISKS;ptr++) {
					if(device->missingdisk==-1||device->missingdisk!=ptr)
						memdisk_read_block(device->disk_array->disks[ptr],disks[ptr],dblock);
				}
				offset=0;
				bufferptr=bufferptr+VMEMRAID_SECTOR_SIZE*numblocks;
			}
			else {
				memcpy(buffer+bufferptr,disks[drive]+VMEMRAID_SECTOR_SIZE*offset,numofsect*VMEMRAID_SECTOR_SIZE);
				numofsect=0;
			}
		}
	}
}



/* Request function. This is registered with the block API and gets */
/* called whenever someone wants data from your device */
static void vmemraid_request(struct request_queue *q) {
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
/* populate dev->size with the total usable bytes of your disk */
/* this implentation will work */
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
/* NOTE: This will be called with dev->lock HELD */
int vmemraid_callback_drop_disk(int disk_num){

	if(dev->missingdisk!=-1)
		return -1;
	else
		dev->missingdisk=disk_num;
		return 0;
}



/* This gets called when a dropped disk is replaced with a new one */
/* NOTE: This will be called with dev->lock HELD */
int vmemraid_callback_new_disk(int disk_num)
{
	int g,c;
	int row;
	int i =0;
	int ptr;
	memset(strfull,0,VMEMRAID_HW_SECTOR_SIZE);

	row = DISK_SIZE_SECTORS;
	row=row/(VMEMRAID_HW_SECTOR_SIZE/VMEMRAID_SECTOR_SIZE);
	for(i=i;i<row;i++){
		for(ptr=0;ptr<NUM_DISKS;ptr++) {
			if(dev->missingdisk!=ptr)
				memdisk_read_block(dev->disk_array->disks[ptr],disks[ptr],i);

		}
		for(g=0;g<VMEMRAID_HW_SECTOR_SIZE;g++){
			for(c=0;c<NUM_DISKS;c++){
				if(c!=dev->missingdisk)
					strfull[g]=strfull[g]^disks[c][g];
			}
		}
		memdisk_write_block(dev->disk_array->disks[dev->missingdisk],strfull,i);
		memset(strfull,0,VMEMRAID_HW_SECTOR_SIZE);
	}
	dev->missingdisk=-1;
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
static int __init vmemraid_init(void)
{
//Allocate space for vmemraid_dev
	dev = vmalloc(sizeof(struct vmemraid_dev));
//Setup device size
	dev->size = DISK_SIZE_SECTORS*VMEMRAID_SECTOR_SIZE*(NUM_DISKS-1);
//Initialize lock
	spin_lock_init(&dev->lock);
//Initialize major
	dev->major = 0;
//Create disk array (unsigned num_disks, unsigned disk_size_bytes)
	dev->disk_array = create_disk_array(NUM_DISKS, dev->size/(NUM_DISKS-1));
	if(dev->disk_array == NULL) {
		printk(KERN_WARNING "vmemraid: not enough space\n");
		return -ENOMEM;
	}

dev->missingdisk=-1;

//Allocation of request queue
	dev->queue = blk_init_queue(vmemraid_request, &dev->lock);
		if(dev->queue==NULL){
		printk(KERN_WARNING "vmemraid: unable to get request queue\n");
		destroy_disk_array(dev->disk_array);
		vfree(dev);
		return -ENOMEM;
}
	blk_queue_logical_block_size(dev->queue,VMEMRAID_SECTOR_SIZE);

//Register with kernel
	dev->major = register_blkdev(dev->major, "vmemraid");
	if(dev->major <= 0) {
		printk(KERN_WARNING "vmemraid: unable to get major number\n");
		destroy_disk_array(dev->disk_array);
		vfree(dev);
		return -ENOMEM;
	}

//Gen disk structure
	dev->gd=alloc_disk(VMEMRAID_NUM_MINORS);
	if(!dev->gd) {
		unregister_blkdev(dev->major,"vmemraid");
		destroy_disk_array(dev->disk_array);
		vfree(dev);
		return -ENOMEM;
	}
	dev->gd->major=dev->major;
	dev->gd->first_minor=0;
	dev->gd->fops=&vmemraid_ops;
	dev->gd->private_data=dev;
	strcpy(dev->gd->disk_name, "vmemraid");
	set_capacity(dev->gd, DISK_SIZE_SECTORS*(NUM_DISKS-1));
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

