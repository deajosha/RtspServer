#ifndef WHAYER_ACCESS_GATEWAY_CONFIG_API_H_
#define WHAYER_ACCESS_GATEWAY_CONFIG_API_H_

#include <memory>

namespace whayer {
	namespace access_gateway {

		const int kHostAddressLength = 0x10; // 主机IP地址长度
		const char kRestServerAddressField[] = "rest_server_address";
		const char kRestServerPortField[] = "rest_server_port";
		const char kRtspServerAddressField[] = "rtsp_server_address";
		const char kRtspServerProtField[] = "rtsp_server_port";

		struct RtspConfig {
			char rest_server_address[kHostAddressLength];
			unsigned int rest_server_port;
			char rtsp_server_address[kHostAddressLength];
			unsigned int rtsp_server_port;
		};

		class ConfigApi {
		public:
			static ConfigApi* instance() {
				return config_api_.get();
			}

			char read_rtsp_config(const char* file_path);

			char* get_rest_server_address() {
				return rtsp_config_.rest_server_address;
			}

			unsigned int get_rest_server_port() {
				return rtsp_config_.rest_server_port;
			}

			char* get_rtsp_server_address() {
				return rtsp_config_.rtsp_server_address;
			}

			unsigned int get_rtsp_server_port() {
				return rtsp_config_.rtsp_server_port;
			}
		private:
			ConfigApi() {};

			static std::shared_ptr<ConfigApi> config_api_;
			
			RtspConfig rtsp_config_;
		};

	}
}

#endif