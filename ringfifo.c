/*ringbuf .c*/

#include<stdio.h>
#include<ctype.h>
#include <stdlib.h>
#include <string.h>
#include "ringfifo.h"
#include "rtputils.h"

#define NMAX 32

int iput = 0; /* 环形缓冲区的当前放入位置 */
int iget = 0; /* 缓冲区的当前取出位置 */
int n = 0; /* 环形缓冲区中的元素总数量 */

struct ringbuf ringfifo[NMAX];
extern int UpdateSpsOrPps(unsigned char *data,int frame_type,int len);
void UpdateSps(unsigned char *data,int len);
void UpdatePps(unsigned char *data,int len);

/* 环形缓冲区的地址编号计算函数，如果到达唤醒缓冲区的尾部，将绕回到头部。
环形缓冲区的有效地址编号为：0到(NMAX-1)
*/
void ringmalloc(int size)
{
    int i;
    for(i =0; i<NMAX; i++)
    {
        ringfifo[i].buffer = malloc(size);
        ringfifo[i].size = 0;
        ringfifo[i].frame_type = 0;
       // printf("FIFO INFO:idx:%d,len:%d,ptr:%x\n",i,ringfifo[i].size,(int)(ringfifo[i].buffer));
    }
    iput = 0; /* 环形缓冲区的当前放入位置 */
    iget = 0; /* 缓冲区的当前取出位置 */
    n = 0; /* 环形缓冲区中的元素总数量 */
}
/**************************************************************************************************
**
**
**
**************************************************************************************************/
void ringreset()
{
    iput = 0; /* 环形缓冲区的当前放入位置 */
    iget = 0; /* 缓冲区的当前取出位置 */
    n = 0; /* 环形缓冲区中的元素总数量 */
}
/**************************************************************************************************
**
**
**
**************************************************************************************************/
void ringfree(void)
{
    int i;
    printf("begin free mem\n");
    for(i =0; i<NMAX; i++)
    {
       // printf("FREE FIFO INFO:idx:%d,len:%d,ptr:%x\n",i,ringfifo[i].size,(int)(ringfifo[i].buffer));
        free(ringfifo[i].buffer);
        ringfifo[i].size = 0;
    }
}
/**************************************************************************************************
**
**
**
**************************************************************************************************/
int addring(int i)
{
    return (i+1) == NMAX ? 0 : i+1;
}

/**************************************************************************************************
**
**
**
**************************************************************************************************/
/* 从环形缓冲区中取一个元素 */

int ringget(struct ringbuf *getinfo)
{
    int Pos;
    if(n>0)
    {
        Pos = iget;
        iget = addring(iget);
        n--;
        getinfo->buffer = (ringfifo[Pos].buffer);
        getinfo->frame_type = ringfifo[Pos].frame_type;
        getinfo->size = ringfifo[Pos].size;
        //printf("Get FIFO INFO:idx:%d,len:%d,ptr:%x,type:%d\n",Pos,getinfo->size,(int)(getinfo->buffer),getinfo->frame_type);
        return ringfifo[Pos].size;
    }
    else
    {
        //printf("Buffer is empty\n");
        return 0;
    }
}
/**************************************************************************************************
**
**
**
**************************************************************************************************/
/* 向环形缓冲区中放入一个元素*/
void ringput(unsigned char *buffer,int size,int encode_type)
{

    if(n<NMAX)
    {
        memcpy(ringfifo[iput].buffer,buffer,size);
        ringfifo[iput].size= size;
        ringfifo[iput].frame_type = encode_type;
        //printf("Put FIFO INFO:idx:%d,len:%d,ptr:%x,type:%d\n",iput,ringfifo[iput].size,(int)(ringfifo[iput].buffer),ringfifo[iput].frame_type);
        iput = addring(iput);
        n++;
    }
    else
    {
        //  printf("Buffer is full\n");
    }
}

/**************************************************************************************************
**
**
**
**************************************************************************************************/
#include <imp/imp_encoder.h>
int HisiPutH264DataToBuffer(IMPEncoderStream *stream)
{
	int i,j,x;
	int len=0,off=0,len2=2,uplen=0;
	unsigned char *pstr;
	int iframe=0;
	for (i = 0; i < stream->packCount; i++)
	{
		len+=stream->pack[i].length;
	}
	if(len>=300*1024)
	{
		printf("drop data %d\n",len);
		return 1;
	}

    if(n<NMAX)
    {
		for (i = 0; i < stream->packCount; i++)
		{
			memcpy(ringfifo[iput].buffer+off,stream->virAddr+stream->pack[i].offset,stream->pack[i].length);
			off+=stream->pack[i].length;
			pstr=stream->virAddr+stream->pack[i].offset;

			if(pstr[4]==0x67)
			{
				UpdateSps(ringfifo[iput].buffer+off,9);
				iframe=1;
			}
			if(pstr[4]==0x68)
			{
				UpdatePps(ringfifo[iput].buffer+off,4);
			}

		}

        ringfifo[iput].size= len;
        iput = addring(iput);
       n++;
   }

	return 1;
}
