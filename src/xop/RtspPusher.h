#ifndef XOP_RTSP_PUSHER_H
#define XOP_RTSP_PUSHER_H

#include <mutex>
#include <map>
#include "rtsp.h"

namespace xop
{

class RtspConnection;

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

class RtspPusher : public Rtsp
{
public:
	RtspPusher(std::string rtsp_url);
	~RtspPusher();

	void setup();
	void stop();
	static void test_thread(MediaSessionId session_id);

	MediaSessionId get_session_id() { return session_id_; }

private:
	friend class RtspConnection;
	
	static bool stop_pusher_;
	MediaSessionId session_id_;
	std::shared_ptr<std::thread> pusher_thread_;

};

class RtspPusherManager {
public:
	static std::shared_ptr<RtspPusherManager> instance();

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