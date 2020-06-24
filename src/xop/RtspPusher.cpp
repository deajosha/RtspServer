#include "RtspPusher.h"
#include "xop/RtspServer.h"
#include "xop/H264Parser.h"
#include "RtspConnection.h"
#include "net/Logger.h"
#include "net/TcpSocket.h"
#include "net/Timestamp.h"
#include "whayer/program_stream.h"
#include "whayer/stream_cache.h"
#include "share_memory_api.h"
#include <memory>

using namespace xop;

bool RtspPusher::stop_pusher_ = false;
whayer::media::ProgramStream program_stream;
whayer::util::StreamCache stream_cache;
std::shared_ptr<std::thread> raw_data_thread_;
FILE* save_handle = fopen("hik_real_stream.h264", "ab+");

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
	//session->AddSource(xop::channel_1, xop::AACSource::CreateNew());
	session->SetNotifyCallback([this](xop::MediaSessionId session_id, uint32_t clients) {
		std::cout << "Rtsp Client 连接个数: " << clients << std::endl;

		if (0 == clients) { // 是否释放推流通道
			RtspPusherManager::instance()->remove_pusher(session_id);
		}
	});

	session_id_ = xop::RtspServer::intance()->AddSession(session);

	// 启动拉流线程
	//pusher_thread_audio_ = std::make_shared<std::thread>(test_aac_thread, session_id_);
	//pusher_thread_ = std::make_shared<std::thread>(test_h264_thread, session_id_);
	pusher_thread_ = std::make_shared<std::thread>(test_reader_thread, suffix, session_id_);
	pusher_thread_->detach();
	//pusher_thread_audio_->detach();
}

std::vector<unsigned char> audio_buffer;
std::vector<unsigned char> video_buffer;
std::vector<unsigned char> ps_buffer;

char find_ps_packet(unsigned char* buffer, unsigned long buffer_size, unsigned char* & position, unsigned long& offset) {
	unsigned char* start = buffer;
	unsigned char* current = start;
	unsigned char* end = buffer + buffer_size - 1;

	bool find_start = false;
	while (current + 4 <= end) { // 寻找完整的PS包
		char find_position = program_stream.get_start_code_position(current, buffer_size, 0xBA, current);
		if (0 != find_position) {
			if (!find_start) { // ps起始位置未找到,丢掉寻找过的数据
				position = buffer + buffer_size - 3; 
				offset = buffer_size - 3; // 缓存最后剩下的3 bytes 数据
			}
			else {
				position = start;
				offset = 0;
			}
	
			return -1; // 未找到PS包
		}

		if (!find_start) { // 找到一个PS包的起始位置
			find_start = true;
			start = current; // 必须包含起始码，方便第二次寻找
			std::cout << "find start PS " << *start << std::endl;
			current += 4;
			continue;
		}
		else {
			offset = current - start;
			position = start; // 起始位置必须包含起始码，方便后面寻找
			std::cout << "buffer start: " << &buffer << " start PS: " << &start[0] << " end PS: " << &current[0] << " ps_size "<< offset  << std::endl;

			return 0; // 找到了完整的PS包
		}
	}

	return 0;
}

void parse_h264(unsigned char* buffer, unsigned long buffer_size, unsigned long & offset) {
	std::cout << "current buffer size " << buffer_size << std::endl;
	unsigned char* position = nullptr;
	char find_packet = find_ps_packet(buffer, buffer_size, position, offset);
	if (0 == find_packet) {
		unsigned char* ps_start = position; // 解析PS头
		char parse_packet = program_stream.parse_packet_header(ps_start, offset, ps_start);
		if (0 != parse_packet) {
			std::cout << "解析PS头失败,返回并重新寻找PS包。\n" << std::endl;
			return;
		}

		// 跳过系统头和映射流
		unsigned char* pes_start = ps_start;
		unsigned char* pes_current = ps_start;
		unsigned char* pes_end = ps_start + offset -1;
		bool is_first_pes = false;
		bool stop = false;
		while (!stop) // 直接寻找PES包（如音频和视频）
		{
			// audio stream number    110x xxxx    [0x000001c0 - 0x000001df]
			// video stream number    1110 xxxx    [0x000001e0 - 0x000001ef]
			// 判断下一个起始码是不是音视频类型
			char find_result = program_stream.get_pes_start_code(pes_current, pes_end - pes_current + 1, 0xC0, 0xEF, pes_current); // 00 00 01 E0    VIDEO_ID
			if (0 == find_result || is_first_pes)
			{
				if (find_result != 0 && is_first_pes)
				{
					pes_current = pes_end; // 未找到下一个完整的起始码，则跳转到PS包结束位置
					stop = true;
				}

				if (is_first_pes)
				{
					unsigned long pes_packet_length = pes_current - pes_start + 1; // 计算PES包的整个长度
					if (pes_packet_length < 12) {
						printf("PES包读取结束\n");
						break;
					}
					find_result = program_stream.parse_pes_packet_header(pes_start, pes_packet_length, pes_start); // 解析PES包头信息
					if (0 != find_result) {
						printf("解析PES包出错，退出，进入下一PS包寻找\n");
						break;
					}

					unsigned long pes_raw_length = 0;
					unsigned long pes_left_length = pes_current - pes_start;
					find_result = program_stream.parse_pes_packet(nullptr, pes_left_length, pes_raw_length);
					if (whayer::protocol::ps_const::kAudioStream == program_stream.get_pes_payload_type()) {
						printf("PES audio 包\n");

						//audio_buffer.insert(audio_buffer.begin() + audio_buffer.size() - 1, ps_current, ps_current + pes_raw_length);
					}
					else if (whayer::protocol::ps_const::kVideoStream == program_stream.get_pes_payload_type()) {
						// 测试写入文件
						//fwrite(pes_start, 1, pes_raw_length, save_handle); 
						video_buffer.insert(video_buffer.end(), pes_start, pes_start + pes_raw_length);
					}
				}
				else {
					is_first_pes = true;
				}

				pes_start = pes_current; // 保存PES包的起始地址
				pes_current += 4;
			}
			else
			{
				break;
			}
		}
	}
}

void test_real_time(MediaSessionId session_id) {
	whayer::util::PsPacketObject ps_packet;
	while (1) {
		if (stream_cache.read_cache(ps_packet)) {

			ps_buffer.insert(ps_buffer.end(), ps_packet.buffer, ps_packet.buffer + ps_packet.buffer_size);
			
			// 按帧解析h264流 
			unsigned long offset = 0; // 计算下一个PS的位置
			parse_h264(ps_buffer.data(), ps_buffer.size(), offset);

			if (0 < offset) { // 移动指针
				ps_buffer.erase(ps_buffer.begin(), ps_buffer.begin() + offset);
			}
			
			if (0 < video_buffer.size()) {
				unsigned char* start = video_buffer.data();
				unsigned char* current = start;
				unsigned long size = video_buffer.size();
				unsigned long frame_size = 0;
				char start_code_bytes = -1;
				bool find_start_code = false;

				while (1) {
					int packet_size = 0;
					if (xop::H264Parser::three_bytes_start_code(current)) {
						start_code_bytes = 3;
					}
					if (xop::H264Parser::four_bytes_start_code(current)) {
						start_code_bytes = 4;
					}

					if (-1 == start_code_bytes) {
						// 说明读到的不是h264的帧，丢掉已读过的数据
						unsigned long remove_length = size - 3;
						video_buffer.erase(video_buffer.begin(), video_buffer.begin() + remove_length);			
						break;
					}

					unsigned char* next_start_code =  xop::H264Parser::find_next_start_code(current + 3, size - 3);
					if (!next_start_code) {
						//说明读到不完整的h264帧，保存等待下一次使用
						video_buffer.erase(video_buffer.begin(), video_buffer.begin() + (current - start));
						break;
					}

					frame_size = static_cast<int>(next_start_code - current);
					
					// 封装rtp包, 发送数据
					xop::AVFrame videoFrame = { 0 };
					videoFrame.type = 0;
					videoFrame.size = frame_size - start_code_bytes;
					videoFrame.timestamp = xop::H264Source::GetTimestamp();
					videoFrame.buffer.reset(new uint8_t[frame_size]);
					memcpy(videoFrame.buffer.get(), current + start_code_bytes, videoFrame.size);

					// 发送rtp包
					RtspServer::intance()->PushFrame(session_id, xop::channel_0, videoFrame);
					// 更新P指针及size大小
					current = next_start_code;
					size -= frame_size;
				}
			}
		}
		else {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

}

void RtspPusher::test_reader_thread(const std::string share_file_name, MediaSessionId session_id) {
	
	int buf_size = 1024 * 500;
	unsigned long read_frame_size = 0;
	uint8_t* read_frame = nullptr;
	
	if (create_reader(share_file_name.c_str())) {
		bool end = false;
		bool find_i_frame = false;
		bool find_ps_packet = false;
		std::vector<unsigned char> recive_buffer;
		//FILE* save_handle = fopen("camera_214.ps", "ab+");

		raw_data_thread_ = std::make_shared<std::thread>(test_real_time, session_id);
		raw_data_thread_->detach();
		bool find_ps_frame = false;
		unsigned char previous_bytes[3] = { 0xFF };
		
		while (!end) {
			std::shared_ptr<unsigned char> frame_buf(new unsigned char[buf_size]);
			unsigned char* p = frame_buf.get();
			read_memory(share_file_name.c_str(), p, read_frame_size, end);
			if (!end) {
				// 验证同步的媒体流是否正确
				//size_t dd = fwrite(p, 1, read_frame_size, save_handle);
				//std::cout << "write size: " << read_frame_size << std::endl;

				stream_cache.write_cache(p, read_frame_size);
			}
			else {
				fclose(save_handle);
				//stop_read_raw_buffer = true;
				break;
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
	
	int i = 0;
	while (!stop_pusher_) {

		// 按帧读取并按帧打包发送rtp
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
		return true;
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


