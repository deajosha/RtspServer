// RTSP Pusher

#include "xop/RtspPusher.h"
#include "net/Timer.h"
#include "xop/H264Parser.h"
#include <thread>
#include <memory>
#include <iostream>
#include <string>

#define PUSH_TEST "rtsp://192.168.40.167:554/test" 
#define FRAME_MAX_SIZE (1024*500)

class TFrame
{
public:
	TFrame() :
		mBuffer(new uint8_t[FRAME_MAX_SIZE]),
		mFrameSize(0)
	{ }

	~TFrame()
	{
		delete mBuffer;
	}

	void resetBuffer() {
		memset(mBuffer, 0, FRAME_MAX_SIZE);
	}

	uint8_t* mBuffer;
	uint8_t* mFrame;
	int mFrameSize;
};

class H264File
{
public:
	H264File(int buf_size = 500000);
	~H264File();

	bool Open(const char *path);
	void Close();

	bool IsOpened() const
	{
		return (m_file != NULL);
	}

	int ReadFrame(uint8_t* frame, int size);
private:
	FILE *m_file = NULL;
	char *m_buf = NULL;
	int  m_buf_size = 0;
	int  m_bytes_used = 0;
	int  m_count = 0;
};

H264File::H264File(int buf_size)
	: m_buf_size(buf_size)
{
	m_buf = new char[m_buf_size];
}

H264File::~H264File()
{
	delete m_buf;
}

bool H264File::Open(const char *path)
{
	m_file = fopen(path, "rb");
	if (m_file == NULL) {
		return false;
	}

	return true;
}

void H264File::Close()
{
	if (m_file) {
		fclose(m_file);
		m_file = NULL;
		m_count = 0;
		m_bytes_used = 0;
	}
}

int H264File::ReadFrame(uint8_t* frame, int size)
{
	int rSize, frameSize;
	uint8_t* nextStartCode;

	if (m_file == NULL) {
		return -1;
	}

	rSize = (int)fread(frame, 1, size, m_file);
	if (!xop::H264Parser::three_bytes_start_code(frame) 
		&& !xop::H264Parser::four_bytes_start_code(frame))
		return -1;

	nextStartCode = xop::H264Parser::find_next_start_code(frame + 3, rSize - 3);
	if (!nextStartCode)
	{
		fseek(m_file, 0, SEEK_SET);
		frameSize = rSize;
	}
	else
	{
		frameSize = (nextStartCode - frame);
		fseek(m_file, frameSize - rSize, SEEK_CUR);
	}

	return frameSize;
}

void snedFrameThread(xop::RtspPusher* rtspPusher, H264File* h264_file);

int main(int argc, char **argv)
{	
	std::shared_ptr<xop::EventLoop> event_loop(new xop::EventLoop());  
	std::shared_ptr<xop::RtspPusher> rtsp_pusher = xop::RtspPusher::Create(event_loop.get());

	xop::MediaSession *session = xop::MediaSession::CreateNew(); 
	session->AddSource(xop::channel_0, xop::H264Source::CreateNew()); 
	//session->AddSource(xop::channel_1, xop::AACSource::CreateNew(44100, 2, false));
	rtsp_pusher->AddSession(session);

	if (rtsp_pusher->OpenUrl(PUSH_TEST, 3000) < 0) {
		std::cout << "Open " << PUSH_TEST << " failed." << std::endl; 
		getchar();
		return 0;
	}

	std::cout << "Push stream to " << PUSH_TEST << " ..." << std::endl; 
	if (argc != 2) {
		printf("Usage: %s test.h264\n", argv[0]);
		return 0;
	}

	H264File h264_file;
	if (!h264_file.Open(argv[1])) {
		printf("Open %s failed.\n", argv[1]);
		return 0;
	}

	std::thread thread(snedFrameThread, rtsp_pusher.get(), &h264_file);
	thread.detach();

	while (1) {
		xop::Timer::Sleep(100);
	}

	getchar();
	return 0;
}

void snedFrameThread(xop::RtspPusher* rtsp_pusher, H264File* h264_file)
{       
	int buf_size = 2000000;
	std::unique_ptr<uint8_t> frame_buf(new uint8_t[buf_size]);
	std::unique_ptr<TFrame> tFrame(new TFrame());

	while(rtsp_pusher->IsConnected())
	{      
		tFrame->mFrameSize = h264_file->ReadFrame(tFrame->mBuffer, FRAME_MAX_SIZE);

		if (tFrame->mFrameSize < 0)
			return;

		if (xop::H264Parser::three_bytes_start_code(tFrame->mBuffer))
		{
			tFrame->mFrame = tFrame->mBuffer + 3;
			tFrame->mFrameSize -= 3;
		}
		else
		{
			tFrame->mFrame = tFrame->mBuffer + 4;
			tFrame->mFrameSize -= 4;
		}

		xop::AVFrame videoFrame = { 0 };
		videoFrame.type = 0;
		videoFrame.size = tFrame->mFrameSize;
		videoFrame.timestamp = xop::H264Source::GetTimestamp();
		videoFrame.buffer.reset(new uint8_t[videoFrame.size]);
		memcpy(videoFrame.buffer.get(), tFrame->mFrame, videoFrame.size);

		rtsp_pusher->PushFrame(xop::channel_0, videoFrame); //推流到服务器, 接口线程安全

		// 读取video Frame
		{                              
			/*
				//获取一帧 H264, 打包
				xop::AVFrame videoFrame = {0};
				//videoFrame.size = video frame size;  // 视频帧大小 
				videoFrame.timestamp = xop::H264Source::GetTimestamp(); // 时间戳, 建议使用编码器提供的时间戳
				videoFrame.buffer.reset(new uint8_t[videoFrame.size]);
				//memcpy(videoFrame.buffer.get(), video frame data, videoFrame.size);					

				rtsp_pusher->PushFrame(xop::channel_0, videoFrame); //推流到服务器, 接口线程安全
			*/
		}
        
		// 读取Audio Frame
		{				                       
			/*
				//获取一帧 AAC, 打包
				xop::AVFrame audioFrame = {0};
				//audioFrame.size = audio frame size;  // 音频帧大小 
				audioFrame.timestamp = xop::AACSource::GetTimestamp(44100); // 时间戳
				audioFrame.buffer.reset(new uint8_t[audioFrame.size]);
				//memcpy(audioFrame.buffer.get(), audio frame data, audioFrame.size);

				rtsp_pusher->PushFrame(xop::channel_1, audioFrame); //推流到服务器, 接口线程安全
			*/
		}		

		xop::Timer::Sleep(1); 
	}
}
