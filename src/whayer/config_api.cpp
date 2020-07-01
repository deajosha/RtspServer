#include "3rdpart/libconfig/libconfig.h++"
#include "config_api.h"
#include <iostream>

using namespace whayer::access_gateway;

std::shared_ptr<ConfigApi> ConfigApi::config_api_(new ConfigApi());

char ConfigApi::read_rtsp_config(const char* file_path) {

	libconfig::Config cfg;

	try
	{
		cfg.readFile(file_path);
		const std::string reset_address = cfg.lookup(kRestServerAddressField);
		unsigned int reset_port = cfg.lookup(kRestServerPortField);

		memset(rtsp_config_.rest_server_address, 0, kHostAddressLength);
		if (0 < reset_port && reset_port < 65535) {
			rtsp_config_.rest_server_port = reset_port;
		}

		if (0 < reset_address.size() && reset_address.size() < kHostAddressLength) {
			memcpy(rtsp_config_.rest_server_address, reset_address.c_str(), reset_address.size());
		}

		const std::string rtsp_address = cfg.lookup(kRtspServerAddressField);
		unsigned int rtsp_port = cfg.lookup(kRtspServerProtField);
		memset(rtsp_config_.rtsp_server_address, 0, kHostAddressLength);
		if (0 < rtsp_port && rtsp_port < 65535) {
			rtsp_config_.rtsp_server_port = rtsp_port;
		}

		if (0 < rtsp_address.size() && rtsp_address.size() < kHostAddressLength) {
			memcpy(rtsp_config_.rtsp_server_address, rtsp_address.c_str(), rtsp_address.size());
		}
	}
	catch (const libconfig::FileIOException &fioex){
		return -1; // 未找到文件
	}
	catch (const libconfig::ParseException &pex){
		std::cout << pex.getError() << std::endl;
		return -2; // 文件解析出错
	}catch(const libconfig::SettingNotFoundException &nfex)
	{
		return -3; // 未找到指定key
	}

	return 0;
}