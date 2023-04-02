/*
 *         (C) COPYRIGHT Ingenic Limited
 *              ALL RIGHT RESERVED
 *
 * File        : model_run.cc
 * Authors     : klyu
 * Create Time : 2020-10-28 12:22:44 (CST)
 * Description :
 *
 */

#include "venus.h"
#include <math.h>
#include <fstream>
#include <iostream>
#include <memory.h>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>;
#include <imp/imp_log.h>
#include <imp/imp_common.h>
#include <imp/imp_system.h>
#include <imp/imp_framesource.h>
#include <imp/imp_encoder.h>
//字库
/* 
#include "freetype2/ft2build.h"
#include FT_FREETYPE_H 
*/
//贴图
#include "logodata_100x100_bgra.h"

#include "sample-common.h"

#define IS_ALIGN_64(x) (((size_t)x) & 0x3F)




static bool cvtbgra = true;
static std::unique_ptr<venus::Tensor> input;
static  std::unique_ptr<venus::BaseNet> test_net;

extern void showbox(int index,int x0,int y0,int x1,int y1);
extern void clearallbox(void);

void write_output_bin(const float* out_ptr, int size)
{
    std::string out_name = "out_res.bin";
    std::ofstream owput;
    owput.open(out_name, std::ios::binary);
    if (!owput || !owput.is_open() || !owput.good()) {
        owput.close();
        return ;
    }
    owput.write((char *)out_ptr, size * sizeof(float));
    owput.close();
	return ;
}

using namespace std;
using namespace magik::venus;

typedef struct
{
    unsigned char* image;  
    int w;
    int h;
}input_info_t;

struct PixelOffset {
    int top;
    int bottom;
    int left;
    int right;
};

//检查像素偏移  使偏移像素不会为奇数
void check_pixel_offset(PixelOffset &pixel_offset){
    // 5 5 -> 6 4
    // padding size not is Odd number
    if(pixel_offset.top % 2 == 1){
        pixel_offset.top += 1;
        pixel_offset.bottom -=1;
    }
    if(pixel_offset.left % 2 == 1){
        pixel_offset.left += 1;
        pixel_offset.right -=1;
    }
}
//使用字库写字
/* 
void freetype(char *text, int x, int y, int size)
{

    FT_Library library;
    FT_Face face;

    FT_Init_FreeType(&library);
    FT_New_Face(library, "simsun.ttc", 0, &face);
    FT_Set_Char_Size(face, size << 6, size << 6, 96, 96);
    FT_Set_Pixel_Sizes(face, size, size);
    FT_GlyphSlot slot = face->glyph;
    int pen_x = x * 64, pen_y = y * 64;
    for (int i = 0; text[i]; i++)
    {
        FT_Load_Char(face, text[i], FT_LOAD_RENDER);
        for (int y = 0; y < slot->bitmap.rows; y++)
        {
            for (int x = 0; x < slot->bitmap.width; x++)
            {
                int p = (y + pen_y) * 800 + x + pen_x;
                if (p >= 0 && p < 800 * 600)
                {
                    screen[p] |= slot->bitmap.buffer[y * slot->bitmap.width + x];
                }
            }
        }
        pen_x += slot->advance.x >> 6;
        pen_y += slot->advance.y >> 6;
    }
    FT_Done_Face(face);
    FT_Done_FreeType(library);
}
 */



void trans_coords(std::vector<magik::venus::ObjBbox_t> &in_boxes, PixelOffset &pixel_offset,float scale){
    
   // printf("pad_x:%d pad_y:%d scale:%f \n",pixel_offset.left,pixel_offset.top,scale);
    for(int i = 0; i < in_boxes.size(); i++) {

        in_boxes[i].box.x0 = (in_boxes[i].box.x0 - pixel_offset.left) / scale;
        in_boxes[i].box.x1 = (in_boxes[i].box.x1 - pixel_offset.left) / scale;
        in_boxes[i].box.y0 = (in_boxes[i].box.y0 - pixel_offset.top) / scale;
        in_boxes[i].box.y1 = (in_boxes[i].box.y1 - pixel_offset.top) / scale;
    }
}


void generateBBox(std::vector<venus::Tensor> out_res, std::vector<magik::venus::ObjBbox_t>& candidate_boxes, int img_w, int img_h);

//int main(int argc, char **argv) {
void yoloy5_init(void){
//贴字库
    IMPRgnHandle *prHander[8]={NULL};
    int grpNum = 1;
	//画框
	int i,ret;
	for(i=0;i<8;i++)
	{
		prHander[i] = IMP_OSD_CreateRgn(NULL);
		if (prHander[i] == INVHANDLE) 
		{
			printf("IMP_OSD_CreateRgn Line error !\n");
			return;
		}
		ret = IMP_OSD_RegisterRgn(prHander[i], grpNum, NULL);
		if (ret < 0) 
		{
			printf("IMP IMP_OSD_RegisterRgn failed\n");
			return;
		}
		IMPOSDRgnAttr rAttrRect;
		memset(&rAttrRect, 0, sizeof(IMPOSDRgnAttr));
		rAttrRect.type = OSD_REG_RECT;
		rAttrRect.rect.p0.x = 0;
		rAttrRect.rect.p0.y = 0;
		rAttrRect.rect.p1.x = 110;
		rAttrRect.rect.p1.y = 110;
		rAttrRect.fmt = PIX_FMT_MONOWHITE;
		rAttrRect.data.lineRectData.color = OSD_GREEN;
		rAttrRect.data.lineRectData.linewidth = 1;
		ret = IMP_OSD_SetRgnAttr(prHander[i], &rAttrRect);
		if (ret < 0) 
		{
			printf("IMP_OSD_SetRgnAttr Rect error !\n");
			return;
		}
		IMPOSDGrpRgnAttr grAttrRect;

		if (IMP_OSD_GetGrpRgnAttr(prHander[i], grpNum, &grAttrRect) < 0) {
			printf("IMP_OSD_GetGrpRgnAttr Rect error !\n");
			return NULL;
		}
		memset(&grAttrRect, 0, sizeof(IMPOSDGrpRgnAttr));
		grAttrRect.show = 1;
		grAttrRect.layer = 1;
		grAttrRect.scalex = 1;
		grAttrRect.scaley = 1;
		if (IMP_OSD_SetGrpRgnAttr(prHander[i], grpNum, &grAttrRect) < 0) {
			printf("IMP_OSD_SetGrpRgnAttr Rect error !\n");
			return ;
		}
	}



/*
//以下是OSD贴图功能
IMPRgnHandle *prHander[8]={NULL};
    int i, ret;
    int grpNum = 1;
		prHander[i] = IMP_OSD_CreateRgn(NULL);
		if (prHander[i] == INVHANDLE) 
		{
			printf("IMP_OSD_CreateRgn Line error !\n");
			return;
		}
		ret = IMP_OSD_RegisterRgn(prHander[i], grpNum, NULL);
		if (ret < 0) 
		{
			printf("IMP IMP_OSD_RegisterRgn failed\n");
			return;
		}
		IMPOSDRgnAttr rAttrLogo0;
		memset(&rAttrLogo0, 0, sizeof(IMPOSDRgnAttr));

		int picw1 = 196;
		int pich1 = 108;

		rAttrLogo0.type = OSD_REG_PIC;
		rAttrLogo0.rect.p0.x = 382;
		rAttrLogo0.rect.p0.y = 216;

		rAttrLogo0.rect.p1.x = rAttrLogo0.rect.p0.x+picw1-1; //p0 is start，and p1 well be epual p0+width(or heigth)-1
		rAttrLogo0.rect.p1.y = rAttrLogo0.rect.p0.y+pich1-1;
		rAttrLogo0.fmt = PIX_FMT_BGRA;
		rAttrLogo0.data.picData.pData = logodata_100x100_bgra;


		ret = IMP_OSD_SetRgnAttr(prHander[i], &rAttrLogo0);
		if (ret < 0) 
		{
			printf("IMP_OSD_SetRgnAttr Rect error !\n");
			return;
		}
		IMPOSDGrpRgnAttr grAttrLogo0;

		if (IMP_OSD_GetGrpRgnAttr(prHander[i], grpNum, &grAttrLogo0) < 0) {
			printf("IMP_OSD_GetGrpRgnAttr Rect error !\n");
			return NULL;
		}
		memset(&grAttrLogo0, 0, sizeof(IMPOSDGrpRgnAttr));
		grAttrLogo0.show = 1;
		grAttrLogo0.layer = 1;
		grAttrLogo0.scalex = 1;
		grAttrLogo0.scaley = 1;
		if (IMP_OSD_SetGrpRgnAttr(prHander[i], grpNum, &grAttrLogo0) < 0) {
			printf("IMP_OSD_SetGrpRgnAttr Rect error !\n");
			return ;
		}

//电源图
		prHander[i] = IMP_OSD_CreateRgn(NULL);
		if (prHander[i] == INVHANDLE) 
		{
			printf("IMP_OSD_CreateRgn Line error !\n");
			return;
		}
		ret = IMP_OSD_RegisterRgn(prHander[i], grpNum, NULL);
		if (ret < 0) 
		{
			printf("IMP IMP_OSD_RegisterRgn failed\n");
			return;
		}
		IMPOSDRgnAttr rAttrLogo;
		memset(&rAttrLogo, 0, sizeof(IMPOSDRgnAttr));

		int picw0 = 76;
		int pich0 = 76;

		rAttrLogo.type = OSD_REG_PIC;
		rAttrLogo.rect.p0.x = 880;
		rAttrLogo.rect.p0.y = 0;

		rAttrLogo.rect.p1.x = rAttrLogo.rect.p0.x+picw0-1; //p0 is start，and p1 well be epual p0+width(or heigth)-1
		rAttrLogo.rect.p1.y = rAttrLogo.rect.p0.y+pich0-1;
		rAttrLogo.fmt = PIX_FMT_BGRA;
		rAttrLogo.data.picData.pData = logodata_100x100_bgra;


		ret = IMP_OSD_SetRgnAttr(prHander[i], &rAttrLogo);
		if (ret < 0) 
		{
			printf("IMP_OSD_SetRgnAttr Rect error !\n");
			return;
		}
		IMPOSDGrpRgnAttr grAttrLogo;

		if (IMP_OSD_GetGrpRgnAttr(prHander[i], grpNum, &grAttrLogo) < 0) {
			printf("IMP_OSD_GetGrpRgnAttr Rect error !\n");
			return NULL;
		}
		memset(&grAttrLogo, 0, sizeof(IMPOSDGrpRgnAttr));
		grAttrLogo.show = 1;
		grAttrLogo.layer = 1;
		grAttrLogo.scalex = 1;
		grAttrLogo.scaley = 1;
		if (IMP_OSD_SetGrpRgnAttr(prHander[i], grpNum, &grAttrLogo) < 0) {
			printf("IMP_OSD_SetGrpRgnAttr Rect error !\n");
			return ;
		}
*/




//以下是YOLO的代码
    /*set绑定cpu亲和性*/
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) == -1) {
            printf("warning: could not set CPU affinity, continuing...\n");
    }
    //
    //int ret = 0;

    
    void *handle = NULL;

    int ori_img_h;
    int ori_img_w;
    float scale;
    int in_w = 640, in_h = 384;
//上下左右的像素偏移
    PixelOffset pixel_offset;
  
    ret = venus::venus_init();
    if (0 != ret) {
        fprintf(stderr, "venus init failed.\n");
        exit(0);
    }

    if (cvtbgra){
        test_net = venus::net_create(TensorFormat::NHWC);
    }
    else { 
        test_net = venus::net_create(TensorFormat::NV12);
    }

    std::string model_path = "/model/yolov5s-person-4bit.bin";
    ret = test_net->load_model(model_path.c_str());

	IMPFSChnAttr fs_chn_attr[2];
	//snap yuv
	ret = IMP_FrameSource_GetChnAttr(1, &fs_chn_attr[1]);
	if (ret < 0) {
		printf("%s(%d):IMP_FrameSource_GetChnAttr failed\n", __func__, __LINE__);
		return;
	}

	fs_chn_attr[1].pixFmt = PIX_FMT_NV12;//PIX_FMT_YUYV422;
	ret = IMP_FrameSource_SetChnAttr(1, &fs_chn_attr[1]);
	if (ret < 0) {
		printf("%s(%d):IMP_FrameSource_SetChnAttr failed\n", __func__, __LINE__);
		return;
	}

	/* Step.4 Snap raw 原始拍照数据*/
	ret = IMP_FrameSource_SetFrameDepth(1, 1);
	if (ret < 0) 
	{
		printf("%s(%d):IMP_FrameSource_SetFrameDepth failed\n", __func__, __LINE__);
		return ;
	}
}

void yoloy5_run(void)
{
    float scale;
     PixelOffset pixel_offset;
    int in_w = 640, in_h = 384;
   
    input_info_t input_src;
  
    input_src.w = 960;
    input_src.h = 540;
    int nv12_size = input_src.w * input_src.h * 1.5;
    IMPFrameInfo *frame_bak;
    int ret = IMP_FrameSource_GetFrame(1, &frame_bak);
    if (ret < 0)
    {
    	printf("%s(%d):IMP_FrameSource_GetFrame failed\n", __func__, __LINE__);
    	return 0;
    }
    input_src.image = (unsigned char*)frame_bak->virAddr;
    

    //---------------------process-------------------------------
    // get ori image w h
    int ori_img_w = input_src.w;
    int ori_img_h = input_src.h;
    //printf("ori_image w,h: %d ,%d \n",ori_img_w,ori_img_h);
    int line_stride = ori_img_w;
    input = test_net->get_input(0);
    magik::venus::shape_t rgba_input_shape = input->shape();
  //  printf("model-->%d ,%d %d \n",rgba_input_shape[1], rgba_input_shape[2], rgba_input_shape[3]);
    if (cvtbgra)
    {
        input->reshape({1, in_h, in_w , 4});
    }else
    {
        input->reshape({1, in_h, in_w, 1});
    }

    uint8_t *indata = input->mudata<uint8_t>();

    //std::cout << "input shape:" << std::endl;
   // printf("-->%d %d \n",in_h, in_w);
    //resize and padding
    magik::venus::Tensor temp_ori_input({1, ori_img_h, ori_img_w, 1}, TensorFormat::NV12);

    uint8_t *tensor_data = temp_ori_input.mudata<uint8_t>();

    int src_size = int(ori_img_h * ori_img_w * 1.5);
    magik::venus::memcopy((void*)tensor_data, (void*)input_src.image, src_size * sizeof(uint8_t));


    float scale_x = (float)in_w/(float)ori_img_w;
    float scale_y = (float)in_h/(float)ori_img_h;
    scale = scale_x < scale_y ? scale_x:scale_y;  //min scale
   // printf("scale---> %f\n",scale);

    int valid_dst_w = (int)(scale*ori_img_w);
	if (valid_dst_w % 2 == 1)
		valid_dst_w = valid_dst_w + 1;
    int valid_dst_h = (int)(scale*ori_img_h);
	if (valid_dst_h % 2 == 1)
		valid_dst_h = valid_dst_h + 1;

    int dw = in_w - valid_dst_w;
    int dh = in_h - valid_dst_h;

    pixel_offset.top = int(round(float(dh)/2 - 0.1));
    pixel_offset.bottom = int(round(float(dh)/2 + 0.1));
    pixel_offset.left = int(round(float(dw)/2 - 0.1));
    pixel_offset.right = int(round(float(dw)/2 + 0.1));
    
//    check_pixel_offset(pixel_offset);

    magik::venus::BsCommonParam param;
    param.pad_val = 0;
    param.pad_type = magik::venus::BsPadType::SYMMETRY;
    param.input_height = ori_img_h;
    param.input_width = ori_img_w;
    param.input_line_stride = ori_img_w;
    param.in_layout = magik::venus::ChannelLayout::NV12;
    param.out_layout = magik::venus::ChannelLayout::RGBA;

    magik::venus::common_resize((const void*)tensor_data, *input.get(), magik::venus::AddressLocate::NMEM_VIRTUAL, &param);


   // printf("resize padding over: \n");
   // printf("resize valid_dst, w:%d h %d\n",valid_dst_w,valid_dst_h);
   // printf("padding info top :%d bottom %d left:%d right:%d \n",pixel_offset.top,pixel_offset.bottom,pixel_offset.left,pixel_offset.right);

    test_net->run();
	
    std::unique_ptr<const venus::Tensor> out0 = test_net->get_output(0);
    std::unique_ptr<const venus::Tensor> out1 = test_net->get_output(1);
    std::unique_ptr<const venus::Tensor> out2 = test_net->get_output(2);

    std::unique_ptr<const venus::Tensor> out_tensor = test_net->get_output(0);
    const float *out_ptr = out_tensor->data<float>();
   // write_output_bin(out_ptr, out_tensor->shape()[0] * out_tensor->shape()[1] * out_tensor->shape()[2] * out_tensor->shape()[3]);

    auto shape0 = out0->shape();
    auto shape1 = out1->shape();
    auto shape2 = out2->shape();

    int shape_size0 = shape0[0] * shape0[1] * shape0[2] * shape0[3];
    int shape_size1 = shape1[0] * shape1[1] * shape1[2] * shape1[3];
    int shape_size2 = shape2[0] * shape2[1] * shape2[2] * shape2[3];

    venus::Tensor temp0(shape0);
    venus::Tensor temp1(shape1);
    venus::Tensor temp2(shape2);

    float* p0 = temp0.mudata<float>();
    float* p1 = temp1.mudata<float>();
    float* p2 = temp2.mudata<float>();

    memcopy((void*)p0, (void*)out0->data<float>(), shape_size0 * sizeof(float));
    memcopy((void*)p1, (void*)out1->data<float>(), shape_size1 * sizeof(float));
    memcopy((void*)p2, (void*)out2->data<float>(), shape_size2 * sizeof(float));
   
    std::vector<venus::Tensor> out_res;
    out_res.push_back(temp0);
    out_res.push_back(temp1);
    out_res.push_back(temp2);

    std::vector<magik::venus::ObjBbox_t>  output_boxes;
    output_boxes.clear();
    generateBBox(out_res, output_boxes, in_w, in_h);
    trans_coords(output_boxes, pixel_offset, scale);

    clearallbox();
  // printf("\n");
    int kk=0;
    for (int i = 0; i < int(output_boxes.size()); i++) {
        auto person = output_boxes[i];
     // printf("box:   ");
     // printf("%d ",(int)person.box.x0);
     //   printf("%d ",(int)person.box.y0);
     //   printf("%d ",(int)person.box.x1);
     //   printf("%d ",(int)person.box.y1);
     //   printf("%.2f ",person.score);
     //  printf("\n");
	if(person.score>=0.5)
	{
		showbox(kk,(int)person.box.x0,(int)person.box.y0,(int)person.box.x1,(int)person.box.y1);
		kk++;
	}
    }
   // printf("\n");

    IMP_FrameSource_ReleaseFrame(1, frame_bak);
    if (ret < 0)
    {
	printf( "%s(%d):IMP_FrameSource_ReleaseFrame failed\n", __func__, __LINE__);
	return;
    }
    usleep(30*1000);
    return;
}

void yoloy5_exit(void)
{
    int ret = venus::venus_deinit();
    if (0 != ret) {
       printf("venus deinit failed.\n");
       return ;
   }
   printf("yoloy5 exit\n");
}
void generateBBox(std::vector<venus::Tensor> out_res, std::vector<magik::venus::ObjBbox_t>& candidate_boxes, int img_w, int img_h){

  float person_threshold = 0.3;
  int classes = 1;
  float nms_threshold = 0.6;
  std::vector<float> strides = {8.0, 16.0, 32.0};
  int box_num = 3;
  std::vector<float> anchor = {10,13,  16,30,  33,23, 30,61,  62,45,  59,119, 116,90,  156,198,  373,326};


  std::vector<magik::venus::ObjBbox_t>  temp_boxes;
  venus::generate_box(out_res, strides, anchor, temp_boxes, img_w, img_h, classes, box_num, person_threshold, magik::venus::DetectorType::YOLOV5);
  venus::nms(temp_boxes, candidate_boxes, nms_threshold); 
}

