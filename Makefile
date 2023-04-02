INCLUDE=-I./ -I../include -I../magik-toolkit/InferenceKit/nna1/mips720-glibc229/include
LIBS= ../lib/glibc/libimp.a ../lib/glibc/libalog.a -L../magik-toolkit/InferenceKit/nna1/mips720-glibc229/lib/glibc -lvenus -lpthread -lm -lrt -ldl
all:rtsp-h264
rtsp-h264:
	 mips-linux-gnu-g++ -o rtsp-h264 -O2 -std=c++11 -mfp64 -mnan=2008 -mabs=2008 -EL -march=mips32r2 -flax-vector-conversions -w -fpermissive  main.c ringfifo.c rtputils.c rtspservice.c rtsputils.c  sample-common.c sample-Encoder-video.c  inference_nv12.cpp $(INCLUDE) $(LIBS)
clean:
	rm -rfv rtsp-h264
