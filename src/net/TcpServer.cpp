#include "TcpServer.h"
#include "Acceptor.h"
#include "EventLoop.h"
#include "Logger.h"
#include <cstdio>  

using namespace xop;
using namespace std;

TcpServer::TcpServer(EventLoop* event_loop)
	: event_loop_(event_loop)
	, port_(0)
	, acceptor_(new Acceptor(event_loop))
	, is_started_(false)
{
	// 设置客户端socket连接回调
	acceptor_->SetNewConnectionCallback([this](SOCKET sockfd) {
		TcpConnection::Ptr conn = this->OnConnect(sockfd); // client_fd,新的客户端连接请求
		if (conn) {
			this->AddConnection(sockfd, conn);
			conn->SetDisconnectCallback([this](TcpConnection::Ptr conn) {
				auto scheduler = conn->GetTaskScheduler();
				SOCKET sockfd = conn->GetSocket();
				if (!scheduler->AddTriggerEvent([this, sockfd] {this->RemoveConnection(sockfd); })) {
					scheduler->AddTimer([this, sockfd]() {this->RemoveConnection(sockfd); return false; }, 100);
				}
			});
		}
	});
}

TcpServer::~TcpServer()
{
	Stop();
}

bool TcpServer::Start(std::string ip, uint16_t port)
{
	Stop();// 先停止所有的客户端连接

	if (!is_started_) { // 判断Acceptor是否成功监听
		if (acceptor_->Listen(ip, port) < 0) {
			return false;
		}

		port_ = port;
		ip_ = ip;
		is_started_ = true;
		return true;
	}

	return false;
}

void TcpServer::Stop()
{
	if (is_started_) {
		
		mutex_.lock();
		for (auto iter : connections_) {
			iter.second->Disconnect();
		}
		mutex_.unlock();

		acceptor_->Close();
		is_started_ = false;

		while (1) {
			Timer::Sleep(1);
			if (connections_.empty()) {
				break;
			}
		}
	}	
}

TcpConnection::Ptr TcpServer::OnConnect(SOCKET sockfd)
{
	return std::make_shared<TcpConnection>(event_loop_->GetTaskScheduler().get(), sockfd);
}

void TcpServer::AddConnection(SOCKET sockfd, TcpConnection::Ptr tcpConn)
{
	std::lock_guard<std::mutex> locker(mutex_);
	connections_.emplace(sockfd, tcpConn);
}

void TcpServer::RemoveConnection(SOCKET sockfd)
{
	std::lock_guard<std::mutex> locker(mutex_);
	connections_.erase(sockfd);
}
