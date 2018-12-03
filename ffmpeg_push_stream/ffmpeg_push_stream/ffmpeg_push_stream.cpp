/*
 * ����ffmpeg��rtmp����С����
 * ����������ʹ��livego(����golangʵ�ֵ�rtmp������,��Ȼ��������Э��֧��)
 * create by �Ƴ�
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
	const char *in_filename  = "test.flv";//����URL��Input file URL��
	const char *out_filename="rtmp://localhost:1935/live/movie";

	av_register_all();
	avformat_network_init(); // ��ʼ������
	
	//����
	if ((ret = avformat_open_input(&in_fmt_ctx, in_filename, 0, 0)) < 0) 
	{
		printf( "���ļ�ʧ��");
		goto end;
	}

	if ((ret = avformat_find_stream_info(in_fmt_ctx, 0)) < 0) 
	{
		printf( "������������Ϣʧ��");
		goto end;
	}

	// ������Ƶ������
	for(i=0; i<in_fmt_ctx->nb_streams; i++) 
	{
		if(in_fmt_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
		{
			videoindex=i;
			break;
		}
	}
		
	av_dump_format(in_fmt_ctx, 0, in_filename, 0);

	//���
	avformat_alloc_output_context2(&out_fmt_ctx, NULL, "flv", out_filename); //RTMP
	if (!out_fmt_ctx) 
	{
		printf( "�������������ʧ��\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}

	ofmt = out_fmt_ctx->oformat;
	for (i = 0; i < in_fmt_ctx->nb_streams; i++) {
		// �������������������
		AVStream *in_stream = in_fmt_ctx->streams[i];
		AVStream *out_stream = avformat_new_stream(out_fmt_ctx, in_stream->codec->codec);
		if (!out_stream) 
		{
			printf( "���������ʧ��\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}

		// �������������������
		ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
		if (ret < 0) 
		{
			printf( "�������������������ʧ��\n");
			goto end;
		}

		out_stream->codec->codec_tag = 0;
		if (out_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		{
			out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}
	}
	
	av_dump_format(out_fmt_ctx, 0, out_filename, 1);

	//�����URL
	if (!(ofmt->flags & AVFMT_NOFILE)) 
	{
		ret = avio_open(&out_fmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
		if (ret < 0) 
		{
			printf( "�޷��������ַ: '%s'", out_filename);
			goto end;
		}
	}

	// �����������д�ļ�ͷ
	ret = avformat_write_header(out_fmt_ctx, NULL);
	if (ret < 0) 
	{
		printf( "д���ļ�ͷʧ��\n");
		goto end;
	}

	start_time=av_gettime();
	while (1) 
	{
		AVStream *in_stream;
		AVStream *out_stream;

		//��ȡ֡����
		ret = av_read_frame(in_fmt_ctx, &pkt);
		if (ret < 0)
		{
			break;
		}
			
		// дpts
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

		// д��֡���ݵ����������(RTMP)
		ret = av_interleaved_write_frame(out_fmt_ctx, &pkt);
		if (ret < 0) 
		{
			printf( "д��ʧ��\n");
			break;
		}
		
		av_free_packet(&pkt);
	}

	//д���ļ�β
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
		printf( "ʧ��.\n");
		return -1;
	}
	return 0;
}