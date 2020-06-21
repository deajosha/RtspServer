#include "RtspPusher.h"
#include "xop/RtspServer.h"
#include "xop/H264Parser.h"
#include "RtspConnection.h"
#include "net/Logger.h"
#include "net/TcpSocket.h"
#include "net/Timestamp.h"
#include "share_memory_api.h"
#include <memory>

using namespace xop;

bool RtspPusher::stop_pusher_ = false;

RtspPusher::RtspPusher(std::string rtsp_url)
	:pusher_thread_(nullptr)
{
	parseRtspUrl(rtsp_url);
}

RtspPusher::~RtspPusher()
{
	stop();
}

void RtspPusher::stop() {
	if (!stop_pusher_) {
		stop_pusher_ = true;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

void RtspPusher::setup() {
	std::string suffix = get_rtsp_suffix();

	// 创建MediaSession对象
	xop::MediaSession *session = xop::MediaSession::CreateNew(suffix);
	// 添加H264源
	session->AddSource(xop::channel_0, xop::H264Source::CreateNew());
	//session->StartMulticast(); 

	// 添加AAC源
	session->AddSource(xop::channel_1, xop::AACSource::CreateNew());
	session->SetNotifyCallback([this](xop::MediaSessionId session_id, uint32_t clients) {
		std::cout << "Rtsp Client 连接个数: " << clients << std::endl;

		if (0 == clients) { // 是否释放推流通道
			RtspPusherManager::instance()->remove_pusher(session_id);
		}
	});

	session_id_ = xop::RtspServer::intance()->AddSession(session);

	// 启动拉流线程
	pusher_thread_audio_ = std::make_shared<std::thread>(test_aac_thread, session_id_);
	pusher_thread_ = std::make_shared<std::thread>(test_h264_thread, session_id_);
	//pusher_thread_ = std::make_shared<std::thread>(test_reader_thread, suffix, session_id_);
	pusher_thread_->detach();
	pusher_thread_audio_->detach();
}

void RtspPusher::test_reader_thread(const std::string share_file_name, MediaSessionId session_id) {
	int buf_size = 1024 * 500;
	size_t read_frame_size = 0;
	uint8_t* read_frame = nullptr;
	std::shared_ptr<char> frame_buf(new char[buf_size]);

	if (create_reader(share_file_name.c_str())) {
		bool end = false;
		bool find_i_frame = false;
		while (!end) {
			read_memory(share_file_name.c_str(), frame_buf.get(), read_frame_size, end);
			if (!end) {
				std::shared_ptr<uint8_t> read_buffer(new uint8_t[read_frame_size + 1]);
				memcpy(read_buffer.get(), frame_buf.get(), read_frame_size);

				xop::AVFrame videoFrame = { 0 };
				videoFrame.type = 0;
				videoFrame.size = read_frame_size;
				videoFrame.timestamp = xop::H264Source::GetTimestamp();
				videoFrame.buffer.reset(new uint8_t[videoFrame.size]);
				memcpy(videoFrame.buffer.get(), read_buffer.get(), videoFrame.size);

				RtspServer::intance()->PushFrame(session_id, xop::channel_0, videoFrame);

				//xop::Timer::Sleep(40);
			}
		}
	}
	else {
		std::cerr << "创建读共享内存失败" << std::endl;
	}
}

void RtspPusher::test_h264_thread(xop::MediaSessionId session_id) {

	// 开启推流线程 并获取media_session
	int buf_size = 1024 * 500;
	int read_frame_size = 0;
	uint8_t* read_frame = nullptr;
	std::unique_ptr<uint8_t> frame_buf(new uint8_t[buf_size]);

	// 开启推流线程 并获取media_session
	std::shared_ptr<H264File> h264_file(new H264File());
	if (!h264_file->Open("hik_av.h264")) {
		printf("open test.h264 failed.\n");
		return;
	}
	// 这是为了操作hik media，跳掉头
	//h264_file->set_pos(126);

	int i = 0;
	while (!stop_pusher_) {

		// 读取帧数据
		read_frame_size = h264_file->ReadFrame(frame_buf.get(), buf_size);

		if (read_frame_size < 0) {
			std::cout << "文件读取结束" << std::endl;
			break;
		}

		if (xop::H264Parser::three_bytes_start_code(frame_buf.get()))
		{
			read_frame = frame_buf.get() + 3;
			read_frame_size -= 3;
		}
		else
		{
			read_frame = frame_buf.get() + 4;
			read_frame_size -= 4;
		}

		xop::AVFrame videoFrame = { 0 };
		videoFrame.type = 0;
		videoFrame.size = read_frame_size;
		videoFrame.timestamp = xop::H264Source::GetTimestamp();
		videoFrame.buffer.reset(new uint8_t[videoFrame.size]);
		memcpy(videoFrame.buffer.get(), read_frame, videoFrame.size);

		RtspServer::intance()->PushFrame(session_id, xop::channel_0, videoFrame);

		xop::Timer::Sleep(40);
	};
}

void RtspPusher::test_aac_thread(MediaSessionId session_id) {
	// 开启定时器 读取音频并发送
	int buf_size = 1024 * 500;
	int read_frame_size = 0;
	uint8_t* read_frame = nullptr;
	std::shared_ptr<uint8_t> frame_buf(new uint8_t[buf_size]);
	AVFrame av_frame(buf_size);

	// 开启推流线程 并获取media_session
	std::shared_ptr<AacFile> aac_file_handle(new AacFile());
	if (!aac_file_handle->Open("hik_av.aac")) {
		printf("读取aac文件失败\n");
		return;
	}

	auto fun = [aac_file_handle, av_frame, frame_buf, buf_size, session_id](void) mutable -> bool {
		int header_size = 7;
		int read_size = aac_file_handle->read_frame(frame_buf.get(), buf_size, header_size);
		if (read_size < 0) {
			return false;
		}

		av_frame.header_size = header_size;
		av_frame.size = read_size; 
		memcpy(av_frame.buffer.get(), frame_buf.get(), read_size); // 仅拷贝aac raw data 拷贝数据
		av_frame.type = FrameType::AUDIO_FRAME;
		av_frame.timestamp = AACSource::GetTimestamp();

		// push AAC Frame
		RtspServer::intance()->PushFrame(session_id, xop::channel_1, av_frame);
	};

	// 定时器，每40 millseconds发送一次
	std::shared_ptr<xop::Timer> aac_timer(new xop::Timer(fun, 40));
	aac_timer->Start(25000, true);
	while (!stop_pusher_) {
		
		//xop::Timer::Sleep(30);
	}

	aac_file_handle->Close();
	aac_timer->stop();
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

	rSize = static_cast<int>(fread(frame, 1, size, m_file));
	bool end = feof(m_file);
	if ((rSize < 0 && end) || (!xop::H264Parser::three_bytes_start_code(frame) && !xop::H264Parser::four_bytes_start_code(frame)))
		return -1;

	nextStartCode = xop::H264Parser::find_next_start_code(frame + 3, rSize - 3);
	if (!nextStartCode)
	{
		fseek(m_file, 0, SEEK_SET);
		frameSize = rSize;
	}
	else
	{
		frameSize = static_cast<int>(nextStartCode - frame);
		fseek(m_file, frameSize - rSize, SEEK_CUR);
	}

	return frameSize;
}

std::shared_ptr<RtspPusherManager> RtspPusherManager::rtsp_pusher_manager_(new RtspPusherManager());

std::shared_ptr<RtspPusherManager> RtspPusherManager::instance() {
	return rtsp_pusher_manager_;
}

void RtspPusherManager::add_pusher(const std::string &rtsp_url, const std::string& url_suffix) {
	std::lock_guard<std::mutex> locker(mutex_);
	
	if (rtsp_pusher_.find(url_suffix) == rtsp_pusher_.end()) {
		std::shared_ptr<RtspPusher> rtsp_pusher = std::make_shared<RtspPusher>(rtsp_url);
		rtsp_pusher->setup();
		session_id_pusher_.emplace(rtsp_pusher->get_session_id(), rtsp_pusher->get_rtsp_suffix());
		rtsp_pusher_.emplace(rtsp_pusher->get_rtsp_suffix(), std::move(rtsp_pusher));
	}
}

void RtspPusherManager::remove_pusher(const std::string & url_suffix) {
	std::lock_guard<std::mutex> locker(mutex_);
	
	if (rtsp_pusher_.find(url_suffix) != rtsp_pusher_.end()) {
		rtsp_pusher_[url_suffix]->stop();
		rtsp_pusher_.erase(url_suffix);
	}
}

void RtspPusherManager::remove_pusher(MediaSessionId session_id) {
	std::lock_guard<std::mutex> locker(mutex_);

	auto iter = session_id_pusher_.find(session_id);
	if (iter != session_id_pusher_.end()) {
		rtsp_pusher_[iter->second]->stop();
		rtsp_pusher_.erase(iter->second);
		session_id_pusher_.erase(session_id);
	}
}

RtspPusher* RtspPusherManager::find_pusher(const std::string & url_suffix) {
	std::lock_guard<std::mutex> locker(mutex_);

	auto iter = rtsp_pusher_.find(url_suffix);

	return (iter != rtsp_pusher_.end()) ? iter->second.get() : nullptr;
}

RtspPusher* RtspPusherManager::find_pusher(MediaSessionId session_id) {
	std::lock_guard<std::mutex> locker(mutex_);

	auto iter = session_id_pusher_.find(session_id);
	
	return (iter != session_id_pusher_.end()) ? rtsp_pusher_[iter->second].get() : nullptr;
}


