# v4l2 & UVC for Camera

## v4l2

框架概述：https://blog.csdn.net/u013904227/article/details/80718831

![image-20210804103311753](v4l2%20&%20UVC%20for%20Camera.assets/image-20210804103311753.png)

### 摄像头简单工作流程

https://blog.csdn.net/simonforfuture/article/details/78743800

整个过程：
首先：先启动视频采集，驱动程序开始采集一帧数据，把采集的数据放入视频采集输入队列的第一个帧缓冲区，一帧数据采集完成，也就是第一个帧缓冲区存满一帧数据后，驱动程序将该帧缓冲区移至视频采集输出队列，等待应用程序从输出队列取出。驱动程序则继续采集下一帧数据放入第二个缓冲区，同样帧缓冲区存满下一帧数据后，被放入视频采集输出队列。
然后：应用程序从视频采集输出队列中取出含有视频数据的帧缓冲区，处理帧缓冲区中的视频数据，如存储或压缩。
最后：应用程序将处理完数据的**帧缓冲区**重新放入视频采集输入队列，这样可以循环采集。

**视频缓存池**：视频缓存池主要向媒体业务提供大块物理内存管理功能，负责内存的分配和回收，充分发挥内存缓存池的作用，让物理内存资源在各个媒体处理模块中合理使用。
一组大小相同、物理地址连续的缓存块组成一个视频缓存池。
视频输入通道需要使用**公共视频缓存池**。所有的视频输入通道都可以从公共视频缓存池中获取视频缓存块用于保存采集的图像。由于视频输入通道不提供创建爱你和销毁公共视频缓存池功能。因此，在系统初始化之前，必须为视频输入通道配置公共缓存池。根据业务的不同，公共缓存池的数量、缓存块的大小和数量不同。缓存块的生存期是指经过VPSS通道传给后续模块的情形。如果该缓存块完全没有经过VPSS通道传给其他模块，则将在VPSS模块处理后被放回公共缓存池。

![image-20210804103252878](v4l2%20&%20UVC%20for%20Camera.assets/image-20210804103252878.png)

所以，我们从摄像头中获取的视频帧数据会放入视频缓存队列中，当其他模块需要处理对应的视频帧的时候，就会占用缓存块，也就是这一块内存被占用，当处理完之后，对应的数据通过VO/VENC/VDA显示之后，这一缓存块就没有用了，可以回收利用。现在来看，其实海思的底层处理和linux的底层处理是一样的。不过海思本身使用的就是linux内核。应该也就是对这一块进行封装了而已吧！

![image-20210804103304816](v4l2%20&%20UVC%20for%20Camera.assets/image-20210804103304816.png)

从这张图可以看出，海思的公共视频缓存池按我的理解应该有两部分，一部分是视频采集输入队列，另一部分是视频采集输出队列，VI通道是是视频采集输出队列中获取的视频帧，而中间linux内核的驱动程序会在视频采集输入队列中填充视频帧，变成视频输出队列。
每一个帧缓冲区都有一个对应的状态标志变量，其中每一个比特代表一个状态：



1. 打开设备文件

   int fd=open("/dev/video0",O_RDWR);

2. 取得设备的capability，看看设备具体支持哪些功能，比如是否具有视频的输入或者音频的输入等等

   ioctl(fd_v4l, VIDIOC_QUERYCAP, &cap)

3. 设置视频采集的参数：

   - 设置视频的制式，制式包括PAL/NTSC，使用 ioctl(fd_v4l, VIDIOC_S_STD, &std_id)
   - 设置视频图像的采集窗口的大小，使用 ioctl(fd_v4l, VIDIOC_S_CROP, &crop)
   - 设置视频帧格式，包括帧的点阵格式，宽度和高度等，使用 ioctl(fd_v4l, VIDIOC_S_FMT, &fmt)
   - 设置视频的帧率，使用 ioctl(fd_v4l, VIDIOC_S_PARM, &parm)
   - 设置视频的旋转方式，使用 ioctl(fd_v4l, VIDIOC_S_CTRL, &ctrl)

4. 向驱动申请视频的帧缓冲区，一般不会超过5个ioctl(fd_v4l, VIDIOC_REQBUFS, &req)
   查询帧缓冲区在内核空间中的长度和偏移量 ioctl(fd_v4l, VIDIOC_QUERYBUF, &buf)

5. 物理内存映射
   将帧缓冲区的地址映射到用户空间，这样就可以直接操作采集到的帧了，而不必去复制。
   buffers[i].start = mmap (NULL, buffers[i].length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_v4l, buffers[i].offset);

6. 将申请到的帧缓冲全部放入视频采集输出队列，以便存放采集的数据。ioctl (fd_v4l, VIDIOC_QBUF, &buf)

7. 开始视频流数据的采集。 ioctl (fd_v4l, VIDIOC_STREAMON, &type)

8. 驱动将采集到的一帧视频数据存入输入队列第一个帧缓冲区，存完后将该帧缓冲区移至视频采集输出队列。

9. 应用程序从视频采集输出队列中取出已含有采集数据的帧缓冲区。ioctl (fd_v4l, VIDIOC_DQBUF, &buf) ，应用程序处理该帧缓冲区的原始视频数据。

10. 处理完后，应用程序的将该帧缓冲区重新排入输入队列,这样便可以循环采集数据。ioctl (fd_v4l, VIDIOC_QBUF, &buf)重复上述步骤8到10，直到停止采集数据。

11. 停止视频的采集。ioctl (fd_v4l, VIDIOC_STREAMOFF, &type)

12. 释放申请的视频帧缓冲区 unmap，关闭视频设备文件 close(fd_v4l)。

### Camera

https://blog.csdn.net/morixinguan/article/details/51001713

相关结构体

系统

```c++
/**
  * struct v4l2_capability - Describes V4L2 device caps returned by VIDIOC_QUERYCAP
  *
  * @driver:	   name of the driver module (e.g. "bttv")
  * @card:	   name of the card (e.g. "Hauppauge WinTV")
  * @bus_info:	   name of the bus (e.g. "PCI:" + pci_name(pci_dev) )
  * @version:	   KERNEL_VERSION
  * @capabilities: capabilities of the physical device as a whole
  * @device_caps:  capabilities accessed via this particular device (node)
  * @reserved:	   reserved fields for future extensions
  */
struct v4l2_capability {
	__u8	driver[16];    //驱动名称
	__u8	card[32];      //设备名称
	__u8	bus_info[32];  //设备在系统中的位置
	__u32   version;       //（驱动）内核版本号
	__u32	capabilities;  //整个物理设备支持的操作
	__u32	device_caps;   //通过此特定设备（节点）访问的功能
	__u32	reserved[3];   //保留字段
};

struct v4l2_input {
	__u32	     index;		/*  Which input */
	__u8	     name[32];		/*  Label */
	__u32	     type;		/*  Type of input */
	__u32	     audioset;		/*  Associated audios (bitfield) */
	__u32        tuner;             /*  enum v4l2_tuner_type */
	v4l2_std_id  std;
	__u32	     status;
	__u32	     capabilities;
	__u32	     reserved[3];
};

/**
 * struct v4l2_format - stream data format
 * @type:	enum v4l2_buf_type; type of the data stream
 * @pix:	definition of an image format
 * @pix_mp:	definition of a multiplanar image format
 * @win:	definition of an overlaid image
 * @vbi:	raw VBI capture or output parameters
 * @sliced:	sliced VBI capture or output parameters
 * @raw_data:	placeholder for future extensions and custom formats
 */
struct v4l2_format {
	__u32	 type;
	union {
		struct v4l2_pix_format		pix;     /* V4L2_BUF_TYPE_VIDEO_CAPTURE */
		struct v4l2_pix_format_mplane	pix_mp;  /* V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE */
		struct v4l2_window		win;     /* V4L2_BUF_TYPE_VIDEO_OVERLAY */
		struct v4l2_vbi_format		vbi;     /* V4L2_BUF_TYPE_VBI_CAPTURE */
		struct v4l2_sliced_vbi_format	sliced;  /* V4L2_BUF_TYPE_SLICED_VBI_CAPTURE */
		struct v4l2_sdr_format		sdr;     /* V4L2_BUF_TYPE_SDR_CAPTURE */
		struct v4l2_meta_format		meta;    /* V4L2_BUF_TYPE_META_CAPTURE */
		__u8	raw_data[200];                   /* user-defined */
	} fmt;
};

/*
 *	M E M O R Y - M A P P I N G   B U F F E R S
 */
struct v4l2_requestbuffers {
	__u32			count;
	__u32			type;		/* enum v4l2_buf_type */
	__u32			memory;		/* enum v4l2_memory */
	__u32			capabilities;
	__u32			reserved[1];
};

/**
 * struct v4l2_buffer - video buffer info
 * @index:	id number of the buffer
 * @type:	enum v4l2_buf_type; buffer type (type == *_MPLANE for
 *		multiplanar buffers);
 * @bytesused:	number of bytes occupied by data in the buffer (payload);
 *		unused (set to 0) for multiplanar buffers
 * @flags:	buffer informational flags
 * @field:	enum v4l2_field; field order of the image in the buffer
 * @timestamp:	frame timestamp
 * @timecode:	frame timecode
 * @sequence:	sequence count of this frame
 * @memory:	enum v4l2_memory; the method, in which the actual video data is
 *		passed
 * @offset:	for non-multiplanar buffers with memory == V4L2_MEMORY_MMAP;
 *		offset from the start of the device memory for this plane,
 *		(or a "cookie" that should be passed to mmap() as offset)
 * @userptr:	for non-multiplanar buffers with memory == V4L2_MEMORY_USERPTR;
 *		a userspace pointer pointing to this buffer
 * @fd:		for non-multiplanar buffers with memory == V4L2_MEMORY_DMABUF;
 *		a userspace file descriptor associated with this buffer
 * @planes:	for multiplanar buffers; userspace pointer to the array of plane
 *		info structs for this buffer
 * @length:	size in bytes of the buffer (NOT its payload) for single-plane
 *		buffers (when type != *_MPLANE); number of elements in the
 *		planes array for multi-plane buffers
 * @request_fd: fd of the request that this buffer should use
 *
 * Contains data exchanged by application and driver using one of the Streaming
 * I/O methods.
 */
struct v4l2_buffer {
	__u32			index;
	__u32			type;
	__u32			bytesused;
	__u32			flags;
	__u32			field;
	struct timeval		timestamp;
	struct v4l2_timecode	timecode;
	__u32			sequence;

	/* memory location */
	__u32			memory;
	union {
		__u32           offset;
		unsigned long   userptr;
		struct v4l2_plane *planes;
		__s32		fd;
	} m;
	__u32			length;
	__u32			reserved2;
	union {
		__s32		request_fd;
		__u32		reserved;
	};
};

struct v4l2_event_subscription {
	__u32				type;
	__u32				id;
	__u32				flags;
	__u32				reserved[5];
};

```

自定义

```c++
struct v4l2_device {
    /* v4l2 device specific */
    int v4l2_fd;
    int is_streaming;
    char *v4l2_devname;
    /* v4l2 buffer specific */
    enum io_method io;
    struct buffer *mem;
    unsigned int nbufs;
    /* v4l2 buffer queue and dequeue counters */
    unsigned long long int qbuf_count;
    unsigned long long int dqbuf_count;
    /* uvc device hook */
    struct uvc_device *udev;
};

/* Buffer representing one video frame */
struct buffer {
    struct v4l2_buffer buf;
    void *start;
    size_t length;
};
```



```c++
struct v4l2_capability capability;
v4l2_input input;
//设备打开
int fd = open("/dev/video0", O_RDONLY);
int fd = open("/dev/video0", O_RDWR | O_NONBLOCK, 0);
//设备关闭
close(fd);
//查询设备属性
ioctl(fd, VIDIOC_QUERYCAP, &capability);
if(capability & V4L2_CAP_VIDEO_CAPTURE) {}  //是否支持图像获取
if(capability & V4L2_CAP_STREAMING) {}  //是否支持Streaming I/O模式
//输入源列举
ioctl(fd, VIDIOC_ENUMINPUT, &input);

//查看和设置当前格式
struct v4l2_format fmt;
struct v4l2_device dev;
/*v4l2 get format*/
CLEAR(fmt);
fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
//指定buf的类型为capture，用于视频捕获设备
int ret = ioctl(dev->v4l2_fd, VIDIOC_G_FMT, &fmt);
/*v4l2 set format*/
ret = ioctl(dev->v4l2_fd, VIDIOC_S_FMT, fmt);

/*视频应用可以通过两种方式从V4L2驱动申请buffer
1. USERPTR， 顾名思义是用户空间指针的意思，应用层负责分配需要的内存空间，然后以指针的形式传递给V4L2驱动层，V4L2驱动会把capture的内容保存到指针所指的空间
一般来说，应用层需要确保这个内存空间物理上是连续的（IPU处理单元的需求），在android系统可以通过PMEM驱动来分配大块的连续物理内存。应用层在不需要的时候要负责释放申请的PMEM内存。
2. MMAP方式，内存映射模式，应用调用VIDIOC_REQBUFS ioctl分配设备buffers，参数标识需要的数目和类型。这个ioctl也可以用来改变buffers的数据以及释放分配的内存，当然这个内存空间一般也是连续的。在应用空间能够访问这些物理地址之前，必须调用mmap函数把这些物理空间映射为用户虚拟地址空间。
虚拟地址空间是通过munmap函数释放的； 而物理内存的释放是通过VIDIOC_REQBUFS来实现的(设置参数buf count为(0)），物理内存的释放是实现特定的，mx51 v4l2是在关闭设备时进行释放的。
二者都是申请连续的物理内存，只是申请和释放的方式不同
*/
struct v4l2_requestbuffers req;
CLEAR(req);
req.count = nbufs;
req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
//MMAP，内存映射模式
req.memory = V4L2_MEMORY_MMAP;  
//请求/申请nbufs个帧缓冲区，一般不少于三个
int ret = ioctl(dev->v4l2_fd, VIDIOC_REQBUFS, &req);
/* Map the buffers. */
dev->mem = calloc(req.count, sizeof dev->mem[0]);
for (i = 0; i < req.count; ++i) {
	memset(&dev->mem[i].buf, 0, sizeof(dev->mem[i].buf));
    dev->mem[i].buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    dev->mem[i].buf.memory = V4L2_MEMORY_MMAP;
    dev->mem[i].buf.index = i;
	//查询帧缓冲区在内核空间的长度和偏移量
    ret = ioctl(dev->v4l2_fd, VIDIOC_QUERYBUF, &(dev->mem[i].buf)); 
}
dev->mem[i].start = mmap (NULL /* start anywhere */,
                          dev->mem[i].buf.length,
                          PROT_READ | PROT_WRITE /* required */,
                          MAP_SHARED /* recommended */,
                          dev->v4l2_fd, dev->mem[i].buf.m.offset);
dev->mem[i].length = dev->mem[i].buf.length;
dev->nbufs = req.count;

//用户空间指针
req.memory = V4L2_MEMORY_USERPTR;    
ret = ioctl(dev->v4l2_fd, VIDIOC_REQBUFS, &req);
dev->nbufs = req.count;

//填充采集输出队列
memset(&dev->mem[i].buf, 0, sizeof(dev->mem[i].buf));
dev->mem[i].buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
dev->mem[i].buf.memory = V4L2_MEMORY_MMAP;
dev->mem[i].buf.index = i;
//投放一个空的视频缓冲区到视频缓冲区输入队列中，指令(指定)的视频缓冲区进入视频输入队列，在启动视频设备拍摄图像时，相应的视频数据被保存到视频输入队列相应的视频缓冲区中。
ret = ioctl(dev->v4l2_fd, VIDIOC_QBUF, &(dev->mem[i].buf));   

//启动视频采集
int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
int ret;
//启动视频采集命令，应用程序调用VIDIOC_STREAMON启动视频采集命令后，视频设备驱动程序开始采集视频数据，并把采集到的视频数据保存到视频驱动的视频缓冲区中。
ret = ioctl(dev->v4l2_fd, VIDIOC_STREAMON, &type);
```

https://www.cnblogs.com/Lxk0825/category/1399884.html

