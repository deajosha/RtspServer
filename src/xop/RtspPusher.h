#ifndef XOP_RTSP_PUSHER_H
#define XOP_RTSP_PUSHER_H

#include <mutex>
#include <map>
#include "rtsp.h"
#include "aac_parser.h"

namespace xop
{

class RtspConnection;

class AacFile {
public:
	bool Open(const char* path) {
		m_file = fopen(path, "rb");
		if (m_file == NULL) {
			return false;
		}
		return true;
	}

	void Close() {
		if (m_file) {
			fclose(m_file);
			m_file = NULL;
		}
	}

	// 从文件中读取一帧数据
	int read_frame(unsigned char* buffer, int frame_max_size, int& header_size) {
		unsigned char read_size = 7;
		unsigned char read_buffer[7] = {0};

		if (m_file == NULL) {
			return -1;
		}

		int rSize = static_cast<int>(fread(read_buffer, 1, read_size, m_file));
		if (rSize <= 0)
		{
			fseek(m_file, 0, SEEK_SET);
			rSize = static_cast<int>(fread(read_buffer, 1, read_size, m_file));
			if (rSize <= 0)
				return -1;
		}

		// 解析adts头
		int index = Acc_Parser::parser_adts_header(read_buffer, &adst_header_);
		if (-1 == index)
		{
			std::cout <<"not found header, skip 8 bytes and continue ... ... \n";
			return -1;
		}
		else {
			if (0 == adst_header_.protectionAbsent) {
				fread(read_buffer, 1, 2, m_file);
				header_size = 9;
			}
			else {
				header_size = 7; // 默认是7bytes
			}
		}

		if (adst_header_.aacFrameLength > frame_max_size)
			return -1;

		//memcpy(buffer, read_buffer, read_size);
		//rSize = static_cast<int>(fread(buffer + read_size, 1, adst_header_.aacFrameLength - read_size, m_file));
		// 仅拷贝raw data
		rSize = static_cast<int>(fread(buffer, 1, adst_header_.aacFrameLength - header_size, m_file));

		if (rSize < 0)
		{
			std::cout << "read err\n";
			return -1;
		}

		return (adst_header_.aacFrameLength - header_size);
	}

private:
	FILE *m_file = NULL;
	struct xop::AdtsHeader adst_header_;
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

	void set_pos(long pos) { if (m_file) { fseek(m_file, pos, SEEK_SET); } }
	int ReadFrame(uint8_t* frame, int size);

	FILE *m_file = NULL;
private:
	char *m_buf = NULL;
	int  m_buf_size = 0;
	int  m_bytes_used = 0;
	int  m_count = 0;
};

class RtspPusher : public Rtsp
{
public:
	RtspPusher(std::string rtsp_url);
	~RtspPusher();

	void setup();
	void stop();
	static void test_h264_thread(MediaSessionId session_id);
	static void test_aac_thread(MediaSessionId session_id);
	static void test_reader_thread(const std::string share_file_name, MediaSessionId session_id);
	MediaSessionId get_session_id() { return session_id_; }
	
	bool request_stream_share_memory(unsigned int memory_size); // 请求实时流共享内存
	bool request_play_real_stream(); // 请求播放实时流
	void request_stop_real_stream(); // 请求停止实时流
private:
	friend class RtspConnection;
	
	static bool stop_pusher_;
	MediaSessionId session_id_;
	std::shared_ptr<std::thread> pusher_thread_;
	std::shared_ptr<std::thread> pusher_thread_audio_;
};

class RtspPusherManager {
public:
	static RtspPusherManager* instance();

	void add_pusher(const std::string &rtsp_url, const std::string& url_suffix);
	void remove_pusher(const std::string &url_suffix);
	void remove_pusher(MediaSessionId session_id);
	RtspPusher* find_pusher(const std::string & url_suffix);
	RtspPusher* find_pusher(MediaSessionId session_id);

private:
	RtspPusherManager() {}

	static std::shared_ptr<RtspPusherManager> rtsp_pusher_manager_;

	std::mutex mutex_;
	std::unordered_map<std::string, std::shared_ptr<RtspPusher>> rtsp_pusher_;
	std::unordered_map<MediaSessionId, std::string> session_id_pusher_;
};

}

#endif