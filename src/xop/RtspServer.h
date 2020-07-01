// PHZ
// 2020-4-2

#ifndef XOP_RTSP_SERVER_H
#define XOP_RTSP_SERVER_H

#include <memory>
#include <string>
#include <mutex>
#include <unordered_map>
#include "net/TcpServer.h"
#include "rtsp.h"

namespace xop
{

class RtspConnection;

class RtspServer : public Rtsp, public TcpServer
{
public:   
	static RtspServer* intance();
	
	~RtspServer();

    MediaSessionId AddSession(MediaSession* session);
    void RemoveSession(MediaSessionId sessionId);

    bool PushFrame(MediaSessionId sessionId, MediaChannelId channelId, AVFrame frame);

private:
    friend class RtspConnection;
	
	RtspServer(EventLoop* event_loop);

	static std::shared_ptr<RtspServer> rtsp_server_; 
	static std::shared_ptr<EventLoop> event_loop_;

    MediaSessionPtr LookMediaSession(const std::string& suffix);
    MediaSessionPtr LookMediaSession(MediaSessionId sessionId);
    virtual TcpConnection::Ptr OnConnect(SOCKET sockfd);

    std::mutex mutex_;
    std::unordered_map<MediaSessionId, std::shared_ptr<MediaSession>> media_sessions_;
    std::unordered_map<std::string, MediaSessionId> rtsp_suffix_map_;
};

}

#endif

