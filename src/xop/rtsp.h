// PHZ
// 2018-6-8

#ifndef XOP_RTSP_H
#define XOP_RTSP_H

#include <cstdio>
#include <string>
#include "MediaSession.h"
#include "net/Acceptor.h"
#include "net/EventLoop.h"
#include "net/Socket.h"
#include "net/Timer.h"

namespace xop
{

struct RtspUrlInfo
{
	std::string url;
	std::string ip;
	uint16_t port;
	std::string suffix;
	int stream_type; // 流类型，1:主码流 2: 辅码流
	std::string terminal_code; //终端编码
	std::string device_code; // 设备编码
	std::string device_id; // 设备id
};

class Rtsp : public std::enable_shared_from_this<Rtsp>
{
public:
	Rtsp() : has_auth_info_(false) {}
	virtual ~Rtsp() {}

	virtual void SetAuthConfig(std::string realm, std::string username, std::string password)
	{
		realm_ = realm;
		username_ = username;
		password_ = password;
		has_auth_info_ = true;

		if (realm_=="" || username=="") {
			has_auth_info_ = false;
		}
	}

	virtual void SetVersion(std::string version) // SDP Session Name
	{ version_ = std::move(version); }

	virtual std::string GetVersion()
	{ return version_; }

	virtual std::string GetRtspUrl()
	{ return rtsp_url_info_.url; }

	virtual std::string get_rtsp_suffix() {
		return rtsp_url_info_.suffix;
	}

	virtual std::string get_device_id() {
		return rtsp_url_info_.device_id;
	}

	virtual std::string get_rtsp_steam_id() {
		return (rtsp_url_info_.device_id + "_" + std::to_string(rtsp_url_info_.stream_type));
	}

	virtual int get_rtsp_stream_type() {
		return rtsp_url_info_.stream_type;
	}

	virtual std::string get_device_code() {
		return rtsp_url_info_.device_code;
	}

	virtual std::string get_terminal_code() {
		return rtsp_url_info_.terminal_code;
	}

	void paras_rtsp_params(std::string url) {
		const std::string stream_type_prefix = "&stream_type=";
		const std::string device_id_prefix = "&deviceId=";
		const std::string device_code_prefix = "&deviceCode=";
		const std::string terminal_code_prefix = "terminalCode=";

		const std::string stream_type_format = stream_type_prefix + "%d";
		const std::string device_id_format = device_id_prefix + "%s";
		const std::string device_code_format = device_code_prefix + "%s";
		const std::string terminal_code_format = terminal_code_prefix + "%s";

		char terminal_code[32] = { 0 };
		char device_code[32] = { 0 };
		char device_id[32] = { 0 };
		int stream_type = -1;

		int pos = url.rfind("?");
		if (pos != std::string::npos) {
			std::string params = url.substr(pos + 1);
			std::string temp;

			pos = params.rfind(stream_type_prefix);
			if (pos != params.npos) {
				temp = params.substr(pos);
				if (1 == sscanf_s(temp.c_str(), stream_type_format.c_str(), &stream_type)) {
					rtsp_url_info_.stream_type = stream_type;
				}
				params = params.substr(0, pos);
			}

			pos = params.rfind(device_id_prefix);
			if (pos != params.npos) {
				temp = params.substr(pos);
				if (temp.size() - device_id_prefix.size() > sizeof(device_id)) {
					temp = temp.substr(0, device_id_prefix.size() + sizeof(device_id) - 1);
				}
				if (1 == sscanf_s(temp.c_str(), device_id_format.c_str(), device_id, sizeof(device_id))) {
					rtsp_url_info_.device_id = device_id;
				}
				params = params.substr(0, pos);
			}

			pos = params.rfind(device_code_prefix);
			if (pos != params.npos) {
				temp = params.substr(pos);
				if (temp.size() - device_code_prefix.size() > sizeof(device_code)) {
					temp = temp.substr(0, device_code_prefix.size() + sizeof(device_code) - 1);
				}
				if (1 == sscanf_s(temp.c_str(), device_code_format.c_str(), device_code, sizeof(device_code))){
					rtsp_url_info_.device_code = device_code;
				}
				params = params.substr(0, pos);
			}

			pos = params.rfind(terminal_code_prefix);
			if (pos != params.npos) {
				temp = params.substr(pos);
				if (temp.size() - terminal_code_prefix.size() > sizeof(terminal_code)) {
					temp = temp.substr(0, terminal_code_prefix.size() + sizeof(terminal_code) - 1);
				}

				if (1 == sscanf_s(temp.c_str(), terminal_code_format.c_str(), terminal_code, sizeof(terminal_code))) {
					rtsp_url_info_.terminal_code = terminal_code;
				}

				params = params.substr(0, pos);
			}
		}
	}

	bool parseRtspUrl(std::string url)
	{
		char ip[100] = { 0 };
		char suffix[100] = { 0 };
		uint16_t port = 0;
#if defined(__linux) || defined(__linux__)
		if (sscanf(url.c_str() + 7, "%[^:]:%hu/%s", ip, &port, suffix) == 3)
#elif defined(WIN32) || defined(_WIN32)
		if (sscanf_s(url.c_str() + 7, "%[^:]:%hu/%s", ip, 100, &port, suffix, 100) == 3)
#endif
		{
			rtsp_url_info_.port = port;
		}
#if defined(__linux) || defined(__linux__)
		else if (sscanf(url.c_str() + 7, "%[^/]/%s", ip, suffix) == 2)
#elif defined(WIN32) || defined(_WIN32)
		else if (sscanf_s(url.c_str() + 7, "%[^/]/%s", ip, 100, suffix, 100) == 2)
#endif
		{
			rtsp_url_info_.port = 554;
		}
		else
		{
			//LOG("%s was illegal.\n", url.c_str());
			return false;
		}
		
		paras_rtsp_params(url);

		rtsp_url_info_.ip = ip;
		rtsp_url_info_.suffix = suffix;
		rtsp_url_info_.url = url;
		return true;
	}

protected:
	friend class RtspConnection;
	virtual MediaSessionPtr LookMediaSession(const std::string& suffix)
	{ return nullptr; }

	virtual MediaSessionPtr LookMediaSession(MediaSessionId sessionId)
	{ return nullptr; }

	bool has_auth_info_ = false;
	std::string realm_;
	std::string username_;
	std::string password_;
	std::string version_;
	struct RtspUrlInfo rtsp_url_info_;
};

}

#endif


