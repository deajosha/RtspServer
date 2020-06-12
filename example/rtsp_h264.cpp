// RTSP Server

#include "xop/RtspServer.h"
#include <memory>
#include <iostream>
#include <string>

int main(int argc, char **argv)
{	
	std::string port = "554";

	// 启动RTSP Server服务
	std::shared_ptr<xop::EventLoop> event_loop(new xop::EventLoop());
	std::shared_ptr<xop::RtspServer> server = xop::RtspServer::Create(event_loop.get());

	if (!server->Start("0.0.0.0", atoi(port.c_str()))) { // 启动Rtsp Server Over Tcp Socket
		printf("RTSP Server listen on %s failed.\n", port.c_str());
		return 0;
	}

	xop::RtspServer::rtspServer_ = server.get();
#ifdef AUTH_CONFIG
	server->SetAuthConfig("-_-", "admin", "12345");
#endif

	std::cout << "RtspServer Started, ready to recieve push operation." << std::endl;

	while (1) {
		xop::Timer::Sleep(100);
	}

	getchar();
	return 0;
}

