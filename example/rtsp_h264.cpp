// RTSP Server

#include "xop/RtspServer.h"
#include "whayer/config_api.h"
#include <iostream>
#include <string>

int main(int argc, char **argv)
{	
	// 加载配置文件
	const char rtsp_config_file[] = "rtsp_server.cfg";
	if (0 != whayer::access_gateway::ConfigApi::instance()->read_rtsp_config(rtsp_config_file)) {
		std::cout << "配置文件读取失败" << std::endl;
		return -1;
	}
	unsigned int port = whayer::access_gateway::ConfigApi::instance()->get_rtsp_server_port();
	char* rtsp_server_address = whayer::access_gateway::ConfigApi::instance()->get_rtsp_server_address();
	
	// 启动RTSP Server服务
	xop::RtspServer* server = xop::RtspServer::intance();

	if (!server->Start(rtsp_server_address, port)) { // 此处退出时，会有异常，需要排查
		printf("RTSP Server listen on %d failed.\n", port);
		return -1;
	}

#ifdef AUTH_CONFIG
	server->SetAuthConfig("-_-", "admin", "12345");
#endif

	std::cout << "RtspServer 启动成功." << std::endl;

	while (1) {
		xop::Timer::Sleep(100);
	}

	getchar();
	return 0;
}

