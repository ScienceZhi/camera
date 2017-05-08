/*
 *代码功能：实现uvc usb摄像头将拍摄到的画面显示在X210开发板上
 *功能选择：如果要呈现640×480像素的画面 请修改IMAGEWIDTH为640 IMAGEHEIGHT480 并在main函数中使用yuyv_2_rgb888_640_480函数
 *	   如果要呈现320×240像素的画面 请修改IMAGEWIDTH为320 IMAGEHEIGHT240 并在main函数中使用yuyv_2_rgb888_320_240函数
 *注释：长宽比在4:3的情况下，对比yuyv_2_rgb888_640_480和yuyv_2_rgb888_320_240两个函数，不难发现其中的规律，从而得出任意比为4:3的yuyv_2_rgb888函数
 */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <linux/types.h>
#include <linux/videodev2.h>

 
#define  TRUE	1
#define  FALSE	0

#define FILE_VIDEO 	"/dev/video3"

#define  IMAGEWIDTH    640	//320
#define  IMAGEHEIGHT   480	//240

int init_v4l2(void);
int v4l2_grab(void);
int yuyv_2_rgb888(void);
int yuyv_2_rgb888_640_480(void);
int yuyv_2_rgb888_320_240(void);
int close_v4l2(void);
static int fd; //用来打开camera
static struct v4l2_capability cap; //使用参数VIDIOC_QUERYCAP来查询并记录camera能力
struct v4l2_fmtdesc fmtdesc; //使用参数VIDIOC_ENUM_FMT来查询并记录列举摄像头所支持像素格式
struct v4l2_format fmt; //使用参数VIDIOC_G_FMT和VIDIOC_S_FMT来查询和设置像素格式。一般的USB摄像头都会支持YUYV				
struct v4l2_streamparm setfps;  //帧速属性等设置
struct v4l2_requestbuffers req; //设置v4l2_requestbuffers结构中定义了缓存的数量，使用参数VIDIOC_REQBUFS后，系统会据此申请对应数量的视频缓存。
struct v4l2_buffer buf; //结构体变量中保存了指令的缓冲区的相关信息，详细百度VIDIOC_QUERYBUF这个参数
enum v4l2_buf_type type; //使用参数VIDIOC_STREAMON开始采集视频。				
unsigned char frame_buffer[IMAGEWIDTH*IMAGEHEIGHT*3];	//定义一个frame_buffer，用来缓存RGB颜色数据
unsigned long *p = NULL;


struct buffer		//该结构体用来存储mmap后的起始地址信息与长度
{
	void * start;
	unsigned int length;
} * buffers;
 


int main(void)
{

    int fd_fb = open("/dev/fb0", O_RDWR);
    unsigned long *fbp32 = NULL;
    long int screen_size = 1024 * 1200 * 24 / 8;//一屏的字节数 
    fbp32 = (unsigned long *)mmap(NULL, screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_fb, 0);        
	init_v4l2();    
	v4l2_grab();	

	int y,x;
	unsigned int cnt = 0;
	//开始视频显示  

	while(1)  
	{  
		 p = fbp32; 
		//内存空间出队列  
	        ioctl(fd, VIDIOC_DQBUF, &buf);//从帧缓冲区拿出数据放到buf中  
		
		//printf("index = %d\n",buf.index);
		yuyv_2_rgb888();//yuyv_2_rgb888_640_480();

		cnt = IMAGEWIDTH*IMAGEHEIGHT*3 - 3;

		if(1) //vinfo.bits_per_pixel == 32 
         	{  
           		for(y = 0; y < IMAGEHEIGHT;  y++)  
              		{  
               			for(x = IMAGEWIDTH-1; x>-1; x--)  
                 		{  
					*(p + 1024 * y + x) = ((frame_buffer[cnt+0]<<0) | (frame_buffer[cnt+1]<<8) | (frame_buffer[cnt+2]<<16));
					cnt -= 3;
                  		}
               		}  
  
         	 }  
		// 内存重新入队列  
        	ioctl(fd, VIDIOC_QBUF, &buf);  
  
    	}

    close_v4l2();
    
    return(TRUE);
}


//各种对摄像头设备的检查与初始化
int init_v4l2(void)
{
	int i;
	int ret = 0;
	
	//opendev
	if ((fd = open(FILE_VIDEO, O_RDWR)) == -1) 
	{
		printf("Error opening V4L interface\n");
		return (FALSE);
	}

	//query cap
	if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) 
	{
		printf("Error opening device %s: unable to query device.\n",FILE_VIDEO);
		return (FALSE);
	}
	else
	{
     	printf("driver:\t\t%s\n",cap.driver);
     	printf("card:\t\t%s\n",cap.card);
     	printf("bus_info:\t%s\n",cap.bus_info);
     	printf("version:\t%d\n",cap.version);
     	printf("capabilities:\t%x\n",cap.capabilities);
     	
     	if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == V4L2_CAP_VIDEO_CAPTURE) 
     	{
			printf("Device %s: supports capture.\n",FILE_VIDEO);
		}

		if ((cap.capabilities & V4L2_CAP_STREAMING) == V4L2_CAP_STREAMING) 
		{
			printf("Device %s: supports streaming.\n",FILE_VIDEO);
		}
	} 
	
	//emu all support fmt
	fmtdesc.index=0;
	fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	printf("Support format:\n");
	while(ioctl(fd,VIDIOC_ENUM_FMT,&fmtdesc)!=-1)
	{
		printf("\t%d.%s\n",fmtdesc.index+1,fmtdesc.description);
		fmtdesc.index++;
	}
	
    	//set fmt
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.height = IMAGEHEIGHT;
	fmt.fmt.pix.width = IMAGEWIDTH;
	fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
	
	if(ioctl(fd, VIDIOC_S_FMT, &fmt) == -1)
	{
		printf("Unable to set format\n");
		return FALSE;
	} 	
	if(ioctl(fd, VIDIOC_G_FMT, &fmt) == -1)
	{
		printf("Unable to get format\n");
		return FALSE;
	} 
	{
     	printf("fmt.type:\t\t%d\n",fmt.type);
     	printf("pix.pixelformat:\t%c%c%c%c\n",fmt.fmt.pix.pixelformat & 0xFF, (fmt.fmt.pix.pixelformat >> 8) & 0xFF,(fmt.fmt.pix.pixelformat >> 16) & 0xFF, (fmt.fmt.pix.pixelformat >> 24) & 0xFF);
     	printf("pix.height:\t\t%d\n",fmt.fmt.pix.height);
     	printf("pix.width:\t\t%d\n",fmt.fmt.pix.width);
     	printf("pix.field:\t\t%d\n",fmt.fmt.pix.field);
	}
	//set fps
	setfps.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	setfps.parm.capture.timeperframe.numerator = 1;
	setfps.parm.capture.timeperframe.denominator = 33;//帧率的设置不是随意的，各个分辨率下硬件支持的分辨率各不相同，硬件会选择最接近的帧率。比如640*480帧率33，因为分辨率设置的是640×480,所以这里denominator设置33以上都没用
	
	ioctl(fd, VIDIOC_S_PARM, &setfps);
	
	printf("init %s \t[OK]\n",FILE_VIDEO);
	    
	return TRUE;
}



//函数作用：缓冲区的申请与相关数据的记录等
int v4l2_grab(void)
{
	unsigned int n_buffers;
	
	//request for 1 buffers 
	req.count=1;
	req.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory=V4L2_MEMORY_MMAP;
	if(ioctl(fd,VIDIOC_REQBUFS,&req)==-1)
	{
		printf("request for buffers error\n");
	}

	//mmap for buffers
	buffers = malloc(req.count*sizeof (*buffers));
	if (!buffers) 
	{
		printf ("Out of memory\n");
		return(FALSE);
	}
	
	for (n_buffers = 0; n_buffers < req.count; n_buffers++) 
	{
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = n_buffers;
		//query buffers
		if (ioctl (fd, VIDIOC_QUERYBUF, &buf) == -1)
		{
			printf("query buffer error\n");
			return(FALSE);
		}

		buffers[n_buffers].length = buf.length;
		//map
		buffers[n_buffers].start = mmap(NULL,buf.length,PROT_READ |PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
		if (buffers[n_buffers].start == MAP_FAILED)
		{
			printf("buffer map error\n");
			return(FALSE);
		}
	}
		
	//queue	将1帧放入缓冲区
	
	for (n_buffers = 0; n_buffers < req.count; n_buffers++)
	{
		buf.index = n_buffers;
		ioctl(fd, VIDIOC_QBUF, &buf);
	} 
	
	
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;	
	ioctl (fd, VIDIOC_STREAMON, &type);//开始拍摄视频

        printf("grab yuyv OK\n");
	return(TRUE);
}



/*函数作用与相关解释：
 *YUV的格式也很多，不过常见的就是422、420等。YUYV就是422形式，简单来说就是，两个像素点P1、P2本应该有Y1、U1、V1和Y2、U2、V2这六个分量，但是实际只保留Y1、U1、Y2、V2。
 *RGB是一种颜色的表示法，计算机中一般采用24位来存储，每个颜色占8位。YUV也是一种颜色空间，为什么要出现YUV,主要有两个原因，一个是为了让彩色信号兼容黑白电视机，另外一个原因是为了减少传输的带宽。YUV中，Y表示亮度，U和V表示色度，总之它是将RGB信号进行了一种处理，根据人对亮度更敏感些，增加亮度的信号，减少颜色的信号，以这样“欺骗”人的眼睛的手段来节省空间。YUV到RGB颜色空间转换关系是：R = Y + 1.042*(V-128);G = Y - 0.34414*(U-128) - 0.71414*(V-128);B = Y + 1.772*(U-128);
 */
int yuyv_2_rgb888_320_240()
{
    int i,j;
    unsigned char y1,y2,u,v;
    int r1,g1,b1,r2,g2,b2;
    char * pointer;
    pointer = buffers[0].start;	
 	
    for(i=0;i<240;i++)
    {
    	for(j=0;j<160;j++)
    	{
    		y1 = *( pointer + (i*160+j)*4);
    		u  = *( pointer + (i*160+j)*4 + 1);
    		y2 = *( pointer + (i*160+j)*4 + 2);
    		v  = *( pointer + (i*160+j)*4 + 3);
    		
    		r1 = y1 + 1.042*(v-128);
    		g1 = y1 - 0.34414*(u-128) - 0.71414*(v-128);
    		b1 = y1 + 1.772*(u-128);
    		
    		r2 = y2 + 1.042*(v-128);
    		g2 = y2 - 0.34414*(u-128) - 0.71414*(v-128);
    		b2 = y2 + 1.772*(u-128);
    		
    		if(r1>255)
    			r1 = 255;
    		else if(r1<0)
    			r1 = 0;
    		
    		if(b1>255)
    			b1 = 255;
    		else if(b1<0)
    			b1 = 0;	
    		
    		if(g1>255)
    			g1 = 255;
    		else if(g1<0)
    			g1 = 0;	
    			
    		if(r2>255)
    			r2 = 255;
    		else if(r2<0)
    			r2 = 0;
    		
    		if(b2>255)
    			b2 = 255;
    		else if(b2<0)
    			b2 = 0;	
    		
    		if(g2>255)
    			g2 = 255;
    		else if(g2<0)
    			g2 = 0;		
    		//frame_buffer全部被用了		
    		*(frame_buffer + ((240-1-i)*160+j)*6    ) = (unsigned char)b1;
    		*(frame_buffer + ((240-1-i)*160+j)*6 + 1) = (unsigned char)g1;
    		*(frame_buffer + ((240-1-i)*160+j)*6 + 2) = (unsigned char)r1;
    		*(frame_buffer + ((240-1-i)*160+j)*6 + 3) = (unsigned char)b2;
    		*(frame_buffer + ((240-1-i)*160+j)*6 + 4) = (unsigned char)g2;
    		*(frame_buffer + ((240-1-i)*160+j)*6 + 5) = (unsigned char)r2;
    	}
    }
    //printf("change to RGB OK \n");
}

int yuyv_2_rgb888_640_480()
{
    int i,j;
    unsigned char y1,y2,u,v;
    int r1,g1,b1,r2,g2,b2;
    char * pointer;
    pointer = buffers[0].start;	
 	
    for(i=0;i<480;i++)
    {
    	for(j=0;j<320;j++)
    	{
    		y1 = *( pointer + (i*320+j)*4);
    		u  = *( pointer + (i*320+j)*4 + 1);
    		y2 = *( pointer + (i*320+j)*4 + 2);
    		v  = *( pointer + (i*320+j)*4 + 3);
    		
    		r1 = y1 + 1.042*(v-128);
    		g1 = y1 - 0.34414*(u-128) - 0.71414*(v-128);
    		b1 = y1 + 1.772*(u-128);
    		
    		r2 = y2 + 1.042*(v-128);
    		g2 = y2 - 0.34414*(u-128) - 0.71414*(v-128);
    		b2 = y2 + 1.772*(u-128);
    		
    		if(r1>255)
    			r1 = 255;
    		else if(r1<0)
    			r1 = 0;
    		
    		if(b1>255)
    			b1 = 255;
    		else if(b1<0)
    			b1 = 0;	
    		
    		if(g1>255)
    			g1 = 255;
    		else if(g1<0)
    			g1 = 0;	
    			
    		if(r2>255)
    			r2 = 255;
    		else if(r2<0)
    			r2 = 0;
    		
    		if(b2>255)
    			b2 = 255;
    		else if(b2<0)
    			b2 = 0;	
    		
    		if(g2>255)
    			g2 = 255;
    		else if(g2<0)
    			g2 = 0;		
    		//frame_buffer全部被用了		
    		*(frame_buffer + ((480-1-i)*320+j)*6    ) = (unsigned char)b1;
    		*(frame_buffer + ((480-1-i)*320+j)*6 + 1) = (unsigned char)g1;
    		*(frame_buffer + ((480-1-i)*320+j)*6 + 2) = (unsigned char)r1;
    		*(frame_buffer + ((480-1-i)*320+j)*6 + 3) = (unsigned char)b2;
    		*(frame_buffer + ((480-1-i)*320+j)*6 + 4) = (unsigned char)g2;
    		*(frame_buffer + ((480-1-i)*320+j)*6 + 5) = (unsigned char)r2;
    	}
    }
    //printf("change to RGB OK \n");
}

int yuyv_2_rgb888()
{
    int i,j;
    unsigned char y1,y2,u,v;
    int r1,g1,b1,r2,g2,b2;
    char * pointer;
    pointer = buffers[0].start;	
 	
    for(i=0;i<IMAGEHEIGHT;i++)
    {
    	for(j=0;j<(IMAGEWIDTH/2);j++)
    	{
    		y1 = *( pointer + (i*IMAGEWIDTH/2+j)*4);
    		u  = *( pointer + (i*IMAGEWIDTH/2+j)*4 + 1);
    		y2 = *( pointer + (i*IMAGEWIDTH/2+j)*4 + 2);
    		v  = *( pointer + (i*IMAGEWIDTH/2+j)*4 + 3);
    		
    		r1 = y1 + 1.042*(v-128);
    		g1 = y1 - 0.34414*(u-128) - 0.71414*(v-128);
    		b1 = y1 + 1.772*(u-128);
    		
    		r2 = y2 + 1.042*(v-128);
    		g2 = y2 - 0.34414*(u-128) - 0.71414*(v-128);
    		b2 = y2 + 1.772*(u-128);
    		
    		if(r1>255)
    			r1 = 255;
    		else if(r1<0)
    			r1 = 0;
    		
    		if(b1>255)
    			b1 = 255;
    		else if(b1<0)
    			b1 = 0;	
    		
    		if(g1>255)
    			g1 = 255;
    		else if(g1<0)
    			g1 = 0;	
    			
    		if(r2>255)
    			r2 = 255;
    		else if(r2<0)
    			r2 = 0;
    		
    		if(b2>255)
    			b2 = 255;
    		else if(b2<0)
    			b2 = 0;	
    		
    		if(g2>255)
    			g2 = 255;
    		else if(g2<0)
    			g2 = 0;		
    		//frame_buffer全部被用了		
    		*(frame_buffer + ((IMAGEHEIGHT-1-i)*IMAGEWIDTH/2+j)*6    ) = (unsigned char)b1;
    		*(frame_buffer + ((IMAGEHEIGHT-1-i)*IMAGEWIDTH/2+j)*6 + 1) = (unsigned char)g1;
    		*(frame_buffer + ((IMAGEHEIGHT-1-i)*IMAGEWIDTH/2+j)*6 + 2) = (unsigned char)r1;
    		*(frame_buffer + ((IMAGEHEIGHT-1-i)*IMAGEWIDTH/2+j)*6 + 3) = (unsigned char)b2;
    		*(frame_buffer + ((IMAGEHEIGHT-1-i)*IMAGEWIDTH/2+j)*6 + 4) = (unsigned char)g2;
    		*(frame_buffer + ((IMAGEHEIGHT-1-i)*IMAGEWIDTH/2+j)*6 + 5) = (unsigned char)r2;
    	}
    }
    //printf("change to RGB OK \n");
}

//作用：关闭打开的资源
int close_v4l2(void)
{
     if(fd != -1) 
     {
         close(fd);
         return (TRUE);
     }
     return (FALSE);
}

