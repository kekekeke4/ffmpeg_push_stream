/*
 * 基于ffmpeg的rtmp推流小程序
 * 服务器可以使用livego(基于golang实现的rtmp服务器,当然还有其他协议支持)
 * create by 科长
 */

#include <stdio.h>
extern "C"
{
#include "libavformat/avformat.h"
#include "libavutil/mathematics.h"
#include "libavutil/time.h"
};

int main(int argc, char* argv[])
{
	AVOutputFormat *ofmt = NULL;
	AVFormatContext *in_fmt_ctx = NULL; 
	AVFormatContext *out_fmt_ctx = NULL;
	AVPacket pkt;
	int ret, i;
	int videoindex=-1;
	int frame_index=0;
	int64_t start_time=0;
	const char *in_filename  = "test.flv";//输入URL（Input file URL）
	const char *out_filename="rtmp://localhost:1935/live/movie";

	av_register_all();
	avformat_network_init(); // 初始化网络
	
	//输入
	if ((ret = avformat_open_input(&in_fmt_ctx, in_filename, 0, 0)) < 0) 
	{
		printf( "打开文件失败");
		goto end;
	}

	if ((ret = avformat_find_stream_info(in_fmt_ctx, 0)) < 0) 
	{
		printf( "查找输入流信息失败");
		goto end;
	}

	// 查找视频流索引
	for(i=0; i<in_fmt_ctx->nb_streams; i++) 
	{
		if(in_fmt_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
		{
			videoindex=i;
			break;
		}
	}
		
	av_dump_format(in_fmt_ctx, 0, in_filename, 0);

	//输出
	avformat_alloc_output_context2(&out_fmt_ctx, NULL, "flv", out_filename); //RTMP
	if (!out_fmt_ctx) 
	{
		printf( "创建输出上下文失败\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}

	ofmt = out_fmt_ctx->oformat;
	for (i = 0; i < in_fmt_ctx->nb_streams; i++) {
		// 根据输入流创建输出流
		AVStream *in_stream = in_fmt_ctx->streams[i];
		AVStream *out_stream = avformat_new_stream(out_fmt_ctx, in_stream->codec->codec);
		if (!out_stream) 
		{
			printf( "创建输出流失败\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}

		// 拷贝编解码上下文设置
		ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
		if (ret < 0) 
		{
			printf( "拷贝编解码上下文设置失败\n");
			goto end;
		}

		out_stream->codec->codec_tag = 0;
		if (out_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		{
			out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}
	}
	
	av_dump_format(out_fmt_ctx, 0, out_filename, 1);

	//打卡输出URL
	if (!(ofmt->flags & AVFMT_NOFILE)) 
	{
		ret = avio_open(&out_fmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
		if (ret < 0) 
		{
			printf( "无法打开输出地址: '%s'", out_filename);
			goto end;
		}
	}

	// 向输出上下文写文件头
	ret = avformat_write_header(out_fmt_ctx, NULL);
	if (ret < 0) 
	{
		printf( "写入文件头失败\n");
		goto end;
	}

	start_time=av_gettime();
	while (1) 
	{
		AVStream *in_stream;
		AVStream *out_stream;

		//读取帧数据
		ret = av_read_frame(in_fmt_ctx, &pkt);
		if (ret < 0)
		{
			break;
		}
			
		// 写pts
		if(pkt.pts==AV_NOPTS_VALUE)
		{
			AVRational time_base1=in_fmt_ctx->streams[videoindex]->time_base;
			int64_t calc_duration=(double)AV_TIME_BASE/av_q2d(in_fmt_ctx->streams[videoindex]->r_frame_rate);
			pkt.pts=(double)(frame_index*calc_duration)/(double)(av_q2d(time_base1)*AV_TIME_BASE);
			pkt.dts = pkt.pts;
			pkt.duration=(double)calc_duration/(double)(av_q2d(time_base1)*AV_TIME_BASE);
		}

		if(pkt.stream_index==videoindex)
		{
			AVRational time_base=in_fmt_ctx->streams[videoindex]->time_base;
			AVRational time_base_q={1,AV_TIME_BASE};
			int64_t pts_time = av_rescale_q(pkt.dts, time_base, time_base_q);
			int64_t now_time = av_gettime() - start_time;
			if (pts_time > now_time)
			{
				av_usleep(pts_time - now_time);
			}
		}

		in_stream  = in_fmt_ctx->streams[pkt.stream_index];
		out_stream = out_fmt_ctx->streams[pkt.stream_index];
		
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		pkt.pos = -1;
		if(pkt.stream_index==videoindex)
		{
			printf("Send %8d video frames to output URL\n",frame_index);
			frame_index++;
		}

		// 写入帧数据到输出上下文(RTMP)
		ret = av_interleaved_write_frame(out_fmt_ctx, &pkt);
		if (ret < 0) 
		{
			printf( "写入失败\n");
			break;
		}
		
		av_free_packet(&pkt);
	}

	//写入文件尾
	av_write_trailer(out_fmt_ctx);
end:
	avformat_close_input(&in_fmt_ctx);
	
	if (out_fmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
	{
		avio_close(out_fmt_ctx->pb);
	}
	avformat_free_context(out_fmt_ctx);
	if (ret < 0 && ret != AVERROR_EOF) 
	{
		printf( "失败.\n");
		return -1;
	}
	return 0;
}