// RTSP Server

#include "xop/RtspServer.h"
#include <iostream>
#include <string>

int main(int argc, char **argv)
{	
	std::string port = "554";
	// ����RTSP Server����
	std::shared_ptr<xop::RtspServer> server = xop::RtspServer::intance();

	if (!server->Start("0.0.0.0", atoi(port.c_str()))) {
		printf("RTSP Server listen on %s failed.\n", port.c_str());
		return 0;
	}

#ifdef AUTH_CONFIG
	server->SetAuthConfig("-_-", "admin", "12345");
#endif

	std::cout << "RtspServer �����ɹ�." << std::endl;

	while (1) {
		xop::Timer::Sleep(100);
	}

	getchar();
	return 0;
}

