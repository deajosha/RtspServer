#include "RtspServer.h"
#include "RtspConnection.h"
#include "net/SocketUtil.h"
#include "net/Logger.h"

using namespace xop;
using namespace std;

std::shared_ptr<RtspServer> RtspServer::rtsp_server_ = nullptr;
std::shared_ptr<EventLoop> RtspServer::event_loop_ = nullptr;

RtspServer::RtspServer(EventLoop* event_loop)
	: TcpServer(event_loop)
{

}

RtspServer::~RtspServer()
{
	
}

RtspServer* RtspServer::intance()
{
	if (nullptr == RtspServer::rtsp_server_) {
		event_loop_ = std::make_shared<EventLoop>();
		rtsp_server_ = std::shared_ptr<RtspServer>(new RtspServer(event_loop_.get()));
	}
	
	return rtsp_server_.get();
}

MediaSessionId RtspServer::AddSession(MediaSession* session)
{
    std::lock_guard<std::mutex> locker(mutex_);

    if (rtsp_suffix_map_.find(session->GetRtspUrlSuffix()) != rtsp_suffix_map_.end()) {
        return 0;
    }

    std::shared_ptr<MediaSession> media_session(session); 
    MediaSessionId sessionId = media_session->GetMediaSessionId();
	rtsp_suffix_map_.emplace(std::move(media_session->GetRtspUrlSuffix()), sessionId);
	media_sessions_.emplace(sessionId, std::move(media_session));

    return sessionId;
}

void RtspServer::RemoveSession(MediaSessionId sessionId)
{
    std::lock_guard<std::mutex> locker(mutex_);

    auto iter = media_sessions_.find(sessionId);
    if(iter != media_sessions_.end()) {
        rtsp_suffix_map_.erase(iter->second->GetRtspUrlSuffix());
        media_sessions_.erase(sessionId);
    }
}

MediaSessionPtr RtspServer::LookMediaSession(const std::string& suffix)
{
    std::lock_guard<std::mutex> locker(mutex_);

    auto iter = rtsp_suffix_map_.find(suffix);
    if(iter != rtsp_suffix_map_.end()) {
        MediaSessionId id = iter->second;
        return media_sessions_[id];
    }

    return nullptr;
}

MediaSessionPtr RtspServer::LookMediaSession(MediaSessionId sessionId)
{
    std::lock_guard<std::mutex> locker(mutex_);

    auto iter = media_sessions_.find(sessionId);
    if(iter != media_sessions_.end()) {
        return iter->second;
    }

    return nullptr;
}

bool RtspServer::PushFrame(MediaSessionId sessionId, MediaChannelId channelId, AVFrame frame)
{
    std::shared_ptr<MediaSession> sessionPtr = nullptr;

    {
        std::lock_guard<std::mutex> locker(mutex_);
        auto iter = media_sessions_.find(sessionId);
        if (iter != media_sessions_.end()) {
            sessionPtr = iter->second;
        }
        else {
            return false;
        }
    }

	// 向rtsp client 推送媒体帧数据
    if (sessionPtr!=nullptr && sessionPtr->GetNumClient()!=0) {
        return sessionPtr->HandleFrame(channelId, frame);
    }

    return false;
}

TcpConnection::Ptr RtspServer::OnConnect(SOCKET sockfd)
{	
	return std::make_shared<RtspConnection>(shared_from_this(), event_loop_->GetTaskScheduler().get(), sockfd);
}

