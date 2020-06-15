#include "RtspPusher.h"
#include "xop/RtspServer.h"
#include "RtspConnection.h"
#include "net/Logger.h"
#include "net/TcpSocket.h"
#include "net/Timestamp.h"
#include <memory>

using namespace xop;

RtspPusher::RtspPusher(std::string rtsp_url)
{
	parseRtspUrl(rtsp_url);
}

RtspPusher::~RtspPusher()
{
	this->Close();
}

void RtspPusher::setup() {
	std::string suffix = get_rtsp_suffix();

	// 创建MediaSession对象并添加H264源
	xop::MediaSession *session = xop::MediaSession::CreateNew(suffix);
	session->AddSource(xop::channel_0, xop::H264Source::CreateNew());
	//session->StartMulticast(); 
	session->SetNotifyCallback([](xop::MediaSessionId session_id, uint32_t clients) {
		std::cout << "Rtsp Client 连接个数: " << clients << std::endl;
	});

	// 将MediaSession对象添加到Rtsp server对象
	xop::MediaSessionId session_id = xop::RtspServer::intance()->AddSession(session);

	// 开启推流线程 并获取media_session
	//h264file* h264_file = new h264file();
	//if (!h264_file->open("test.h264")) {
	//	printf("open test.h264 failed.\n");
	//	return;
	//}

	// 启动拉流线程
	//std::shared_ptr<std::thread> t1 = std::make_shared<std::thread>(SendFrameThread, xop::RtspServer::intance().get(), session_id, h264_file);
	//t1->detach();
}

//void RtspPusher::SendFrameThread(xop::RtspServer* rtsp_server, xop::MediaSessionId session_id, H264File* h264_file)
//{
//	int buf_size = 2000000;
//	std::unique_ptr<uint8_t> frame_buf(new uint8_t[buf_size]);
//	std::unique_ptr<TFrame> tFrame(new TFrame());
//
//	while (1) {
//		bool end_of_frame = false;
//
//		// 读取帧数据
//		tFrame->mFrameSize = h264_file->ReadFrame(tFrame->mBuffer, FRAME_MAX_SIZE);
//
//		if (tFrame->mFrameSize < 0)
//			return;
//
//		if (xop::H264Parser::three_bytes_start_code(tFrame->mBuffer))
//		{
//			tFrame->mFrame = tFrame->mBuffer + 3;
//			tFrame->mFrameSize -= 3;
//		}
//		else
//		{
//			tFrame->mFrame = tFrame->mBuffer + 4;
//			tFrame->mFrameSize -= 4;
//		}
//
//		xop::AVFrame videoFrame = { 0 };
//		videoFrame.type = 0;
//		videoFrame.size = tFrame->mFrameSize;
//		videoFrame.timestamp = xop::H264Source::GetTimestamp();
//		videoFrame.buffer.reset(new uint8_t[videoFrame.size]);
//		memcpy(videoFrame.buffer.get(), tFrame->mFrame, videoFrame.size);
//
//		rtsp_server->PushFrame(session_id, xop::channel_0, videoFrame);
//
//		xop::Timer::Sleep(40);
//	};
//}


void RtspPusher::AddSession(MediaSession* session)
{
    std::lock_guard<std::mutex> locker(mutex_);
    media_session_.reset(session);
}

void RtspPusher::RemoveSession(MediaSessionId sessionId)
{
	std::lock_guard<std::mutex> locker(mutex_);
	media_session_ = nullptr;
}

MediaSessionPtr RtspPusher::LookMediaSession(MediaSessionId sessionId)
{
	return media_session_;
}

int RtspPusher::OpenUrl(std::string url, int msec)
{
	std::lock_guard<std::mutex> lock(mutex_);

	static xop::Timestamp tp;
	int timeout = msec;
	if (timeout <= 0) {
		timeout = 10000;
	}

	tp.reset();

	if (!this->parseRtspUrl(url)) {
		LOG_ERROR("rtsp url(%s) was illegal.\n", url.c_str());
		return -1;
	}

	if (rtsp_conn_ != nullptr) {
		std::shared_ptr<RtspConnection> rtspConn = rtsp_conn_;
		SOCKET sockfd = rtspConn->GetSocket();
		task_scheduler_->AddTriggerEvent([sockfd, rtspConn]() {
			rtspConn->Disconnect();
		});
		rtsp_conn_ = nullptr;
	}

	TcpSocket tcpSocket;
	tcpSocket.Create();
	if (!tcpSocket.Connect(rtsp_url_info_.ip, rtsp_url_info_.port, timeout))
	{
		tcpSocket.Close();
		return -1;
	}

	task_scheduler_ = event_loop_->GetTaskScheduler().get();
	rtsp_conn_.reset(new RtspConnection(shared_from_this(), task_scheduler_, tcpSocket.GetSocket()));
    event_loop_->AddTriggerEvent([this]() {
		//rtsp_conn_->SendOptions(RtspConnection::RTSP_PUSHER);
    });

	timeout -= (int)tp.elapsed();
	if (timeout < 0) {
		timeout = 1000;
	}

	do
	{
		xop::Timer::Sleep(100);
		timeout -= 100;
	} while (!rtsp_conn_->IsRecord() && timeout > 0);

	if (!rtsp_conn_->IsRecord()) {
		std::shared_ptr<RtspConnection> rtspConn = rtsp_conn_;
		SOCKET sockfd = rtspConn->GetSocket();
		task_scheduler_->AddTriggerEvent([sockfd, rtspConn]() {
			rtspConn->Disconnect();
		});
		rtsp_conn_ = nullptr;
		return -1;
	}

	return 0;
}

void RtspPusher::Close()
{
	std::lock_guard<std::mutex> lock(mutex_);

	if (rtsp_conn_ != nullptr) {
		std::shared_ptr<RtspConnection> rtsp_conn = rtsp_conn_;
		SOCKET sockfd = rtsp_conn->GetSocket();
		task_scheduler_->AddTriggerEvent([sockfd, rtsp_conn]() {
			rtsp_conn->Disconnect();
		});
		rtsp_conn_ = nullptr;
	}
}

bool RtspPusher::IsConnected()
{
	std::lock_guard<std::mutex> lock(mutex_);

	if (rtsp_conn_ != nullptr) {
		return (!rtsp_conn_->IsClosed());
	}
	return false;
}

bool RtspPusher::PushFrame(MediaChannelId channelId, AVFrame frame)
{
	std::lock_guard<std::mutex> locker(mutex_);
	if (!media_session_ || !rtsp_conn_) {
		return false;
	}

	return media_session_->HandleFrame(channelId, frame);
}