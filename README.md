基于ffmpeg和livego的简单rtmp推流直播流程

1.运行server文件夹中的rtmp服务器livego.exe
2.运行ffmpeg_push_stream程序(推送端)
3.使用vlc,在菜单“媒体”->“打开网络串流”输入rtmp服务器地址:rtmp://localhost:1935/live/movie可以进行播放
