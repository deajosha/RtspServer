#ifndef WHAYER_MEDIA_PROGRAM_STREAM_H_
#define WHAYER_MEDIA_PROGRAM_STREAM_H_

#include "protocol_ps.h"
#include <vector>
namespace whayer {
	namespace media {
		class ProgramStream {
		public:
			char get_start_code_position(unsigned char* buffer, unsigned long buffer_size,
				unsigned char code_type, unsigned char* &position); // 获取起始码
			char get_pes_start_code(unsigned char* buffer, unsigned long buffer_size,
				unsigned char min_code_type, unsigned char max_code_type, unsigned char* &position); // 获取流媒体的起始码
			char parse_packet_header(unsigned char* buffer, unsigned long buffer_size, unsigned char* & position); // 解析ps_header
			char parse_pes_packet_header(unsigned char* buffer, unsigned long buffer_size, unsigned char* &position); // 解析PES头
			char parse_pes_packet(unsigned char* buffer, unsigned long buffer_size, unsigned long& raw_length); // 解析pes 中的音视频流
			//char parse_system_packet(unsigned char* buffer, unsigned long buffer_size, unsigned char* & position); // 解析ps_header

			unsigned char get_pes_payload_type() {
				if (pes_packet_.stream_id >= whayer::protocol::ps_const::kMinAudioStreamId
					&& pes_packet_.stream_id <= whayer::protocol::ps_const::kMaxAudioStreamId) // 音频负载
				{
					return whayer::protocol::ps_const::kAudioStream;
				}
				else if (pes_packet_.stream_id >= whayer::protocol::ps_const::kMinVideoStreamId
					&& pes_packet_.stream_id <= whayer::protocol::ps_const::kMaxVideoStreamId) // 视频负载
				{
					return whayer::protocol::ps_const::kVideoStream;
				}
				else {
					return whayer::protocol::ps_const::KOtherStream;
				}
			}

		private:
			whayer::protocol::ps_complex::PacketHeader packet_header_;
			whayer::protocol::ps_complex::PesPacket pes_packet_;
			unsigned char current_step_; // PS_header, PS_system_header, PS_map, PES_header, PES_raw_data
			unsigned char* current_position; // 当前位置 
			std::vector<unsigned char> cache_buffer; // 缓存的buffer数据
		};
	}

}


#endif