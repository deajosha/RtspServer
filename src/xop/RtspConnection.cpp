// PHZ
// 2018-6-10

#include "RtspConnection.h"
#include "RtspServer.h"
#include "MediaSession.h"
#include "MediaSource.h"
#include "net/SocketUtil.h"
#include "xop/H264Parser.h"
#include "net/Timer.h"

#define USER_AGENT "-_-"
#define RTSP_DEBUG 0
#define MAX_RTSP_MESSAGE_SIZE 2048

using namespace xop;
using namespace std;

RtspConnection::RtspConnection(std::shared_ptr<Rtsp> rtsp, TaskScheduler *task_scheduler, SOCKET sockfd)
	: TcpConnection(task_scheduler, sockfd)
	, task_scheduler_(task_scheduler)
	, rtsp_(rtsp)
	, rtp_channel_(new Channel(sockfd))
	, rtsp_request_(new RtspRequest)
	, rtsp_response_(new RtspResponse)
{
	this->SetReadCallback([this](std::shared_ptr<TcpConnection> conn, xop::BufferReader& buffer) {
		return this->OnRead(buffer);
	});

	this->SetCloseCallback([this](std::shared_ptr<TcpConnection> conn) {
		this->OnClose();
	});

	alive_count_ = 1;

	rtp_channel_->SetReadCallback([this]() { this->HandleRead(); });
	rtp_channel_->SetWriteCallback([this]() { this->HandleWrite(); });
	rtp_channel_->SetCloseCallback([this]() { this->HandleClose(); });
	rtp_channel_->SetErrorCallback([this]() { this->HandleError(); });

	for(int chn=0; chn<MAX_MEDIA_CHANNEL; chn++) {
		rtcp_channels_[chn] = nullptr;
	}

	has_auth_ = true;
	if (rtsp->has_auth_info_) {
		has_auth_ = false;
		auth_info_.reset(new DigestAuthentication(rtsp->realm_, rtsp->username_, rtsp->password_));
	}	
}

RtspConnection::~RtspConnection()
{

}

// 读取并处理Rtsp Client请求
bool RtspConnection::OnRead(BufferReader& buffer)
{
	KeepAlive();

	int size = buffer.ReadableBytes();
	if (size <= 0) {
		return false; //close
	}

	if (conn_mode_ == RTSP_SERVER) {
		if (!HandleRtspRequest(buffer)){
			return false; 
		}
	}
	else if (conn_mode_ == RTSP_PUSHER) {
		if (!HandleRtspResponse(buffer)) {           
			return false;
		}
	}

	if (buffer.ReadableBytes() > MAX_RTSP_MESSAGE_SIZE) {
		buffer.RetrieveAll(); 
	}

	return true;
}

void RtspConnection::OnClose()
{
	if(session_id_ != 0) {
		auto rtsp = rtsp_.lock();
		if (rtsp) {
			MediaSessionPtr media_session = rtsp->LookMediaSession(session_id_);
			if (media_session) {
				media_session->RemoveClient(this->GetSocket());
			}
		}	
	}

	for(int chn=0; chn<MAX_MEDIA_CHANNEL; chn++) {
		if(rtcp_channels_[chn] && !rtcp_channels_[chn]->IsNoneEvent()) {
			task_scheduler_->RemoveChannel(rtcp_channels_[chn]);
		}
	}
}

bool RtspConnection::HandleRtspRequest(BufferReader& buffer)
{
#if RTSP_DEBUG
	string str(buffer.Peek(), buffer.ReadableBytes());
	if (str.find("rtsp") != string::npos || str.find("RTSP") != string::npos)
	{
		std::cout << str << std::endl;
	}
#endif

    if (rtsp_request_->ParseRequest(&buffer)) {
		RtspRequest::Method method = rtsp_request_->GetMethod();
		std::cout << "request method :" << method << std::endl;
		if(method == RtspRequest::RTCP) {
			HandleRtcp(buffer);
			return true;
		}
		else if(!rtsp_request_->GotAll()) {
			return true;
		}
        
		switch (method)
		{
		case RtspRequest::OPTIONS:
			HandleCmdOption();
			break;
		case RtspRequest::DESCRIBE:
			HandleCmdDescribe();
			break;
		case RtspRequest::SETUP:
			HandleCmdSetup();
			break;
		case RtspRequest::PLAY:
			HandleCmdPlay();
			break;
		case RtspRequest::TEARDOWN:
			HandleCmdTeardown();
			break;
		case RtspRequest::GET_PARAMETER:
			HandleCmdGetParamter();
			break;
		default:
			break;
		}

		if (rtsp_request_->GotAll()) {
			rtsp_request_->Reset();
		}
    }
	else {
		return false;
	}

	return true;
}

bool RtspConnection::HandleRtspResponse(BufferReader& buffer)
{
#if RTSP_DEBUG
	string str(buffer.Peek(), buffer.ReadableBytes());
	if(str.find("rtsp")!=string::npos || str.find("RTSP") != string::npos)
		cout << str << endl;
#endif

	if (rtsp_response_->ParseResponse(&buffer)) {
		RtspResponse::Method method = rtsp_response_->GetMethod();
		switch (method)
		{
		case RtspResponse::OPTIONS:
			if (conn_mode_ == RTSP_PUSHER) {
				SendAnnounce();
			}             
			break;
		case RtspResponse::ANNOUNCE:
		case RtspResponse::DESCRIBE:
			SendSetup();
			break;
		case RtspResponse::SETUP:
			SendSetup();
			break;
		case RtspResponse::RECORD:
			HandleRecord();
			break;
		default:            
			break;
		}
	}
	else {
		return false;
	}

	return true;
}

void RtspConnection::SendRtspMessage(std::shared_ptr<char> buf, uint32_t size)
{
#if RTSP_DEBUG
	cout << buf.get() << endl;
#endif

	this->Send(buf, size);
	return;
}

void RtspConnection::HandleRtcp(BufferReader& buffer)
{    
	char *peek = buffer.Peek();
	if(peek[0] == '$' &&  buffer.ReadableBytes() > 4) {
		uint32_t pkt_size = peek[2]<<8 | peek[3];
		if(pkt_size +4 >=  buffer.ReadableBytes()) {
			buffer.Retrieve(pkt_size +4);  
		}
	}
}
 
void RtspConnection::HandleRtcp(SOCKET sockfd)
{
    char buf[1024] = {0};
    if(recv(sockfd, buf, 1024, 0) > 0) {
        KeepAlive();
    }
}

void RtspConnection::HandleCmdOption()
{
	std::shared_ptr<char> res(new char[2048]);
	int size = rtsp_request_->BuildOptionRes(res.get(), 2048);
	this->SendRtspMessage(res, size);	
}

#define FRAME_MAX_SIZE (1024*500)

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

	rSize = static_cast<int>(fread(frame, 1, size, m_file));
	if (!xop::H264Parser::three_bytes_start_code(frame) && !xop::H264Parser::four_bytes_start_code(frame))
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

void SendFrameThread(xop::RtspServer* rtsp_server, xop::MediaSessionId session_id, H264File* h264_file)
{
	int buf_size = FRAME_MAX_SIZE;
	int read_frame_size = 0;
	uint8_t* read_frame = nullptr;
	std::unique_ptr<uint8_t> frame_buf(new uint8_t[buf_size]);

	while (1) {
		bool end_of_frame = false;

		// 读取帧数据
		read_frame_size = h264_file->ReadFrame(frame_buf.get(), FRAME_MAX_SIZE);

		if (read_frame_size < 0)
			return;

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

		rtsp_server->PushFrame(session_id, xop::channel_0, videoFrame);

		xop::Timer::Sleep(40);
	};
}


void RtspConnection::HandleCmdDescribe()
{
	if (auth_info_!=nullptr && !HandleAuthentication()) {
		return;
	}

	if (rtp_conn_ == nullptr) {
		rtp_conn_.reset(new RtpConnection(shared_from_this()));
	}

	int size = 0;
	std::shared_ptr<char> res(new char[4096]);
	MediaSessionPtr media_session = nullptr;

	auto rtsp = rtsp_.lock();
	if (rtsp) {
		std::string rtsp_url_suffix = rtsp_request_->GetRtspUrlSuffix();
		media_session = rtsp->LookMediaSession(rtsp_url_suffix);
		if (nullptr == media_session) { // 创建Pusher
			
			// 创建MediaSession对象并添加H264源
			xop::MediaSession *session = xop::MediaSession::CreateNew(rtsp_url_suffix);
			session->AddSource(xop::channel_0, xop::H264Source::CreateNew());
			//session->StartMulticast(); 
			session->SetNotifyCallback([](xop::MediaSessionId session_id, uint32_t clients) {
				std::cout << "Rtsp Client 连接个数: " << clients << std::endl;
			});

			// 将MediaSession对象添加到Rtsp server对象
			xop::MediaSessionId session_id = xop::RtspServer::intance()->AddSession(session);

			// 开启推流线程 并获取media_session
			H264File* h264_file = new H264File();
			if (!h264_file->Open("test.h264")) {
				printf("open test.h264 failed.\n");
				return;
			}

			// 启动拉流线程
			std::shared_ptr<std::thread> t1 = std::make_shared<std::thread>(SendFrameThread, xop::RtspServer::intance().get(), session_id, h264_file);
			t1->detach();

			media_session = rtsp->LookMediaSession(rtsp_url_suffix);
		}
	}
	
	if(!rtsp || !media_session) {
		size = rtsp_request_->BuildNotFoundRes(res.get(), 4096);
	}
	else {
		session_id_ = media_session->GetMediaSessionId();
		media_session->AddClient(this->GetSocket(), rtp_conn_);

		for(int chn=0; chn<MAX_MEDIA_CHANNEL; chn++) {
			MediaSource* source = media_session->GetMediaSource((MediaChannelId)chn);
			if(source != nullptr) {
				rtp_conn_->SetClockRate((MediaChannelId)chn, source->GetClockRate());
				rtp_conn_->SetPayloadType((MediaChannelId)chn, source->GetPayloadType());
			}
		}

		std::string sdp = media_session->GetSdpMessage(rtsp->GetVersion());
		if(sdp == "") {
			size = rtsp_request_->BuildServerErrorRes(res.get(), 4096);
		}
		else {
			size = rtsp_request_->BuildDescribeRes(res.get(), 4096, sdp.c_str());		
		}
	}

	SendRtspMessage(res, size);
	return ;
}

void RtspConnection::HandleCmdSetup()
{
	if (auth_info_ != nullptr && !HandleAuthentication()) {
		return;
	}

	int size = 0;
	std::shared_ptr<char> res(new char[4096]);
	MediaChannelId channel_id = rtsp_request_->GetChannelId();
	MediaSessionPtr media_session = nullptr;

	auto rtsp = rtsp_.lock();
	if (rtsp) {
		media_session = rtsp->LookMediaSession(session_id_);
	}

	if(!rtsp || !media_session)  {
		goto server_error;
	}

	if(media_session->IsMulticast())  {
		std::string multicast_ip = media_session->GetMulticastIp();
		if(rtsp_request_->GetTransportMode() == RTP_OVER_MULTICAST) {
			uint16_t port = media_session->GetMulticastPort(channel_id);
			uint16_t session_id = rtp_conn_->GetRtpSessionId();
			if (!rtp_conn_->SetupRtpOverMulticast(channel_id, multicast_ip.c_str(), port)) {
				goto server_error;
			}

			size = rtsp_request_->BuildSetupMulticastRes(res.get(), 4096, multicast_ip.c_str(), port, session_id);
		}
		else {
			goto transport_unsupport;
		}
	}
	else {
		if(rtsp_request_->GetTransportMode() == RTP_OVER_TCP) {
			uint16_t rtp_channel = rtsp_request_->GetRtpChannel();
			uint16_t rtcp_channel = rtsp_request_->GetRtcpChannel();
			uint16_t session_id = rtp_conn_->GetRtpSessionId();

			rtp_conn_->SetupRtpOverTcp(channel_id, rtp_channel, rtcp_channel);
			size = rtsp_request_->BuildSetupTcpRes(res.get(), 4096, rtp_channel, rtcp_channel, session_id);
		}
		else if(rtsp_request_->GetTransportMode() == RTP_OVER_UDP) {
			uint16_t cliRtpPort = rtsp_request_->GetRtpPort();
			uint16_t cliRtcpPort = rtsp_request_->GetRtcpPort();
			uint16_t session_id = rtp_conn_->GetRtpSessionId();

			if(rtp_conn_->SetupRtpOverUdp(channel_id, cliRtpPort, cliRtcpPort)) {                
				SOCKET rtcpfd = rtp_conn_->GetRtcpfd(channel_id);
				rtcp_channels_[channel_id].reset(new Channel(rtcpfd));
				rtcp_channels_[channel_id]->SetReadCallback([rtcpfd, this]() { this->HandleRtcp(rtcpfd); });
				rtcp_channels_[channel_id]->EnableReading();
				task_scheduler_->UpdateChannel(rtcp_channels_[channel_id]);
			}
			else {
				goto server_error;
			}

			uint16_t serRtpPort = rtp_conn_->GetRtpPort(channel_id);
			uint16_t serRtcpPort = rtp_conn_->GetRtcpPort(channel_id);
			size = rtsp_request_->BuildSetupUdpRes(res.get(), 4096, serRtpPort, serRtcpPort, session_id);
		}
		else {          
			goto transport_unsupport;
		}
	}

	SendRtspMessage(res, size);
	return ;

transport_unsupport:
	size = rtsp_request_->BuildUnsupportedRes(res.get(), 4096);
	SendRtspMessage(res, size);
	return ;

server_error:
	size = rtsp_request_->BuildServerErrorRes(res.get(), 4096);
	SendRtspMessage(res, size);
	return ;
}

void RtspConnection::HandleCmdPlay()
{
	if (auth_info_ != nullptr && !HandleAuthentication()) {
		return;
	}

	conn_state_ = START_PLAY;
	rtp_conn_->Play();

	uint16_t session_id = rtp_conn_->GetRtpSessionId();
	std::shared_ptr<char> res(new char[2048]);

	int size = rtsp_request_->BuildPlayRes(res.get(), 2048, nullptr, session_id);
	SendRtspMessage(res, size);
}

void RtspConnection::HandleCmdTeardown()
{
	rtp_conn_->Teardown();

	uint16_t session_id = rtp_conn_->GetRtpSessionId();
	std::shared_ptr<char> res(new char[2048]);
	int size = rtsp_request_->BuildTeardownRes(res.get(), 2048, session_id);
	SendRtspMessage(res, size);

	HandleClose();
}

void RtspConnection::HandleCmdGetParamter()
{
	uint16_t session_id = rtp_conn_->GetRtpSessionId();
	std::shared_ptr<char> res(new char[2048]);
	int size = rtsp_request_->BuildGetParamterRes(res.get(), 2048, session_id);
	SendRtspMessage(res, size);
}

bool RtspConnection::HandleAuthentication()
{
	if (auth_info_ != nullptr && !has_auth_) {
		std::string cmd = rtsp_request_->MethodToString[rtsp_request_->GetMethod()];
		std::string url = rtsp_request_->GetRtspUrl();

		if (_nonce.size() > 0 && (auth_info_->GetResponse(_nonce, cmd, url) == rtsp_request_->GetAuthResponse())) {
			_nonce.clear();
			has_auth_ = true;
		}
		else {
			std::shared_ptr<char> res(new char[4096]);
			_nonce = auth_info_->GetNonce();
			int size = rtsp_request_->BuildUnauthorizedRes(res.get(), 4096, auth_info_->GetRealm().c_str(), _nonce.c_str());
			SendRtspMessage(res, size);
			return false;
		}
	}

	return true;
}

void RtspConnection::SendOptions(ConnectionMode mode)
{
	if (rtp_conn_ == nullptr) {
		rtp_conn_.reset(new RtpConnection(shared_from_this()));
	}

	auto rtsp = rtsp_.lock();
	if (!rtsp) {
		HandleClose();
		return;
	}	

	conn_mode_ = mode;
	rtsp_response_->SetUserAgent(USER_AGENT);
	rtsp_response_->SetRtspUrl(rtsp->GetRtspUrl().c_str());

	std::shared_ptr<char> req(new char[2048]);
	int size = rtsp_response_->BuildOptionReq(req.get(), 2048);
	SendRtspMessage(req, size);
}

void RtspConnection::SendAnnounce()
{
	MediaSessionPtr media_session = nullptr;

	auto rtsp = rtsp_.lock();
	if (rtsp) {
		media_session = rtsp->LookMediaSession(1);
	}

	if (!rtsp || !media_session) {
		HandleClose();
		return;
	}
	else {
		session_id_ = media_session->GetMediaSessionId();
		media_session->AddClient(this->GetSocket(), rtp_conn_);

		for (int chn = 0; chn<2; chn++) {
			MediaSource* source = media_session->GetMediaSource((MediaChannelId)chn);
			if (source != nullptr) {
				rtp_conn_->SetClockRate((MediaChannelId)chn, source->GetClockRate());
				rtp_conn_->SetPayloadType((MediaChannelId)chn, source->GetPayloadType());
			}
		}
	}

	std::string sdp = media_session->GetSdpMessage(rtsp->GetVersion());
	if (sdp == "") {
		HandleClose();
		return;
	}

	std::shared_ptr<char> req(new char[4096]);
	int size = rtsp_response_->BuildAnnounceReq(req.get(), 4096, sdp.c_str());
	SendRtspMessage(req, size);
}

void RtspConnection::SendDescribe()
{
	std::shared_ptr<char> req(new char[2048]);
	int size = rtsp_response_->BuildDescribeReq(req.get(), 2048);
	SendRtspMessage(req, size);
}

void RtspConnection::SendSetup()
{
	int size = 0;
	std::shared_ptr<char> buf(new char[2048]);	
	MediaSessionPtr media_session = nullptr;

	auto rtsp = rtsp_.lock();
	if (rtsp) {
		media_session = rtsp->LookMediaSession(session_id_);
	}
	
	if (!rtsp || !media_session) {
		HandleClose();
		return;
	}

	if (media_session->GetMediaSource(channel_0) && !rtp_conn_->IsSetup(channel_0)) {
		rtp_conn_->SetupRtpOverTcp(channel_0, 0, 1);
		size = rtsp_response_->BuildSetupTcpReq(buf.get(), 2048, channel_0);
	}
	else if (media_session->GetMediaSource(channel_1) && !rtp_conn_->IsSetup(channel_1)) {
		rtp_conn_->SetupRtpOverTcp(channel_1, 2, 3);
		size = rtsp_response_->BuildSetupTcpReq(buf.get(), 2048, channel_1);
	}
	else {
		size = rtsp_response_->BuildRecordReq(buf.get(), 2048);
	}

	SendRtspMessage(buf, size);
}

void RtspConnection::HandleRecord()
{
	conn_state_ = START_PUSH;
	rtp_conn_->Record();
}
