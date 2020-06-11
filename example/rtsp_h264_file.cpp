// RTSP Server

#include "xop/RtspServer.h"
#include "net/Timer.h"
#include <thread>
#include <memory>
#include <iostream>
#include <string>

int startCode3(uint8_t* buf)
{
	if (buf[0] == 0 && buf[1] == 0 && buf[2] == 1)
		return 1;
	else
		return 0;
}

int startCode4(uint8_t* buf)
{
	if (buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 1)
		return 1;
	else
		return 0;
}

uint8_t* findNextStartCode(uint8_t* buf, int len)
{
	int i;

	if (len < 3)
		return NULL;

	for (i = 0; i < len - 3; ++i)
	{
		if (startCode3(buf) || startCode4(buf))
			return buf;

		++buf;
	}

	if (startCode3(buf))
		return buf;

	return NULL;
}

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
	H264File(int buf_size=500000);
	~H264File();

	bool Open(const char *path);
	void Close();

	bool IsOpened() const
	{ return (m_file != NULL); }

	int ReadFrame(char* in_buf, int in_buf_size, bool* end);
	int ReadFrame(uint8_t* frame, int size);
private:
	FILE *m_file = NULL;
	char *m_buf = NULL;
	int  m_buf_size = 0;
	int  m_bytes_used = 0;
	int  m_count = 0;
};

void SendFrameThread(xop::RtspServer* rtsp_server, xop::MediaSessionId session_id, H264File* h264_file);

int main(int argc, char **argv)
{	
	if(argc != 2) {
		printf("Usage: %s test.h264\n", argv[0]);
		return 0;
	}

	H264File h264_file; // H264�ļ����
	if(!h264_file.Open(argv[1])) {
		printf("Open %s failed.\n", argv[1]);
		return 0;
	}

	std::string suffix = "live";
	std::string ip = "127.0.0.1";
	std::string port = "554";
	std::string rtsp_url = "rtsp://" + ip + ":" + port + "/" + suffix;
	
	// ����RTSP Server����
	std::shared_ptr<xop::EventLoop> event_loop(new xop::EventLoop());
	std::shared_ptr<xop::RtspServer> server = xop::RtspServer::Create(event_loop.get());

	if (!server->Start("0.0.0.0", atoi(port.c_str()))) { // ����Rtsp Server Over Tcp Socket
		printf("RTSP Server listen on %s failed.\n", port.c_str());
		return 0;
	}

#ifdef AUTH_CONFIG
	server->SetAuthConfig("-_-", "admin", "12345");
#endif
	// ����MediaSession�������H264Դ
	xop::MediaSession *session = xop::MediaSession::CreateNew("live"); 
	session->AddSource(xop::channel_0, xop::H264Source::CreateNew()); 
	//session->StartMulticast(); 
	session->SetNotifyCallback([] (xop::MediaSessionId session_id, uint32_t clients){
		std::cout << "The number of rtsp clients: " << clients << std::endl;
	});
   
	// ��MediaSession������ӵ�Rtsp server����
	xop::MediaSessionId session_id = server->AddSession(session);
    
	// ���������߳�
	std::thread t1(SendFrameThread, server.get(), session_id, &h264_file);
	t1.detach(); 

	std::cout << "Play URL: " << rtsp_url << std::endl;

	while (1) {
		xop::Timer::Sleep(100);
	}

	getchar();
	return 0;
}

void SendFrameThread(xop::RtspServer* rtsp_server, xop::MediaSessionId session_id, H264File* h264_file)
{       
	int buf_size = 2000000;
	std::unique_ptr<uint8_t> frame_buf(new uint8_t[buf_size]);
	std::unique_ptr<TFrame> tFrame(new TFrame());

	while(1) {
		bool end_of_frame = false;
		
		// ��ȡ֡����
		// int frame_size = h264_file->ReadFrame((char*)frame_buf.get(), buf_size, &end_of_frame);
		// std::cout << "frame Size" << frame_size << std::endl;
		tFrame->mFrameSize = h264_file->ReadFrame(tFrame->mBuffer, FRAME_MAX_SIZE);
		
		if (tFrame->mFrameSize < 0)
			return;

		if (startCode3(tFrame->mBuffer))
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

		rtsp_server->PushFrame(session_id, xop::channel_0, videoFrame);

		/*if(frame_size > 0) {
			xop::AVFrame videoFrame = {0};
			videoFrame.type = 0; 
			videoFrame.size = frame_size;
			videoFrame.timestamp = xop::H264Source::GetTimestamp();
			videoFrame.buffer.reset(new uint8_t[videoFrame.size]);    
			memcpy(videoFrame.buffer.get(), frame_buf.get(), videoFrame.size);
			rtsp_server->PushFrame(session_id, xop::channel_0, videoFrame);
		}
		else {
			break;
		}*/
      
		xop::Timer::Sleep(40); 
	};
}

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
	if(m_file == NULL) {      
		return false;
	}

	return true;
}

void H264File::Close()
{
	if(m_file) {
		fclose(m_file);
		m_file = NULL;
		m_count = 0;
		m_bytes_used = 0;
	}
}

int H264File::ReadFrame(char* in_buf, int in_buf_size, bool* end)
{
	if(m_file == NULL) {
		return -1;
	}

	int bytes_read = (int)fread(m_buf, 1, m_buf_size, m_file);
	if(bytes_read == 0) {
		fseek(m_file, 0, SEEK_SET); 
		m_count = 0;
		m_bytes_used = 0;
		bytes_read = (int)fread(m_buf, 1, m_buf_size, m_file);
		if(bytes_read == 0)         {            
			this->Close();
			return -1;
		}
	}

	bool is_find_start = false, is_find_end = false;
	int i = 0, start_code = 3;
	*end = false;

	for (i=0; i<bytes_read-5; i++) {
		if(m_buf[i] == 0 && m_buf[i+1] == 0 && m_buf[i+2] == 1) {
			start_code = 3;
		}
		else if(m_buf[i] == 0 && m_buf[i+1] == 0 && m_buf[i+2] == 0 && m_buf[i+3] == 1) {
			start_code = 4;
		}
		else  {
			continue;
		}
        
		if (((m_buf[i+start_code]&0x1F) == 0x5 || (m_buf[i+start_code]&0x1F) == 0x1) 
			&& ((m_buf[i+start_code+1]&0x80) == 0x80)) {
			is_find_start = true;
			i += 4;
			break;
		}
	}

	for (; i<bytes_read-5; i++) {
		if(m_buf[i] == 0 && m_buf[i+1] == 0 && m_buf[i+2] == 1)
		{
			start_code = 3;
		}
		else if(m_buf[i] == 0 && m_buf[i+1] == 0 && m_buf[i+2] == 0 && m_buf[i+3] == 1) {
			start_code = 4;
		}
		else   {
			continue;
		}
        
		if (((m_buf[i+start_code]&0x1F) == 0x7) || ((m_buf[i+start_code]&0x1F) == 0x8) 
			|| ((m_buf[i+start_code]&0x1F) == 0x6)|| (((m_buf[i+start_code]&0x1F) == 0x5 
			|| (m_buf[i+start_code]&0x1F) == 0x1) &&((m_buf[i+start_code+1]&0x80) == 0x80)))  {
			is_find_end = true;
			break;
		}
	}

	bool flag = false;
	if(is_find_start && !is_find_end && m_count>0) {        
		flag = is_find_end = true;
		i = bytes_read;
		*end = true;
	}

	if(!is_find_start || !is_find_end) {
		this->Close();
		return -1;
	}

	int size = (i<=in_buf_size ? i : in_buf_size);
	memcpy(in_buf, m_buf, size); 

	if(!flag) {
		m_count += 1;
		m_bytes_used += i;
	}
	else {
		m_count = 0;
		m_bytes_used = 0;
	}

	fseek(m_file, m_bytes_used, SEEK_SET);
	return size;
}

int H264File::ReadFrame(uint8_t* frame, int size)
{
	int rSize, frameSize;
	uint8_t* nextStartCode;

	if (m_file == NULL) {
		return -1;
	}

	rSize = (int)fread(frame, 1, size, m_file);
	if (!startCode3(frame) && !startCode4(frame))
		return -1;

	nextStartCode = findNextStartCode(frame + 3, rSize - 3);
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

