#ifndef WHAYER_MEDIA_PROGRAM_STREAM_H_
#define WHAYER_MEDIA_PROGRAM_STREAM_H_

#include "protocol_ps.h"
#include <vector>
namespace whayer {
	namespace media {
		class ProgramStream {
		public:
			char get_start_code_position(unsigned char* buffer, unsigned long buffer_size,
				unsigned char code_type, unsigned char* &position); // ��ȡ��ʼ��
			char get_pes_start_code(unsigned char* buffer, unsigned long buffer_size,
				unsigned char min_code_type, unsigned char max_code_type, unsigned char* &position); // ��ȡ��ý�����ʼ��
			char parse_packet_header(unsigned char* buffer, unsigned long buffer_size, unsigned char* & position); // ����ps_header
			char parse_pes_packet_header(unsigned char* buffer, unsigned long buffer_size, unsigned char* &position); // ����PESͷ
			char parse_pes_packet(unsigned char* buffer, unsigned long buffer_size, unsigned long& raw_length); // ����pes �е�����Ƶ��
			//char parse_system_packet(unsigned char* buffer, unsigned long buffer_size, unsigned char* & position); // ����ps_header

			unsigned char get_pes_payload_type() {
				if (pes_packet_.stream_id >= whayer::protocol::ps_const::kMinAudioStreamId
					&& pes_packet_.stream_id <= whayer::protocol::ps_const::kMaxAudioStreamId) // ��Ƶ����
				{
					return whayer::protocol::ps_const::kAudioStream;
				}
				else if (pes_packet_.stream_id >= whayer::protocol::ps_const::kMinVideoStreamId
					&& pes_packet_.stream_id <= whayer::protocol::ps_const::kMaxVideoStreamId) // ��Ƶ����
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
			unsigned char* current_position; // ��ǰλ�� 
			std::vector<unsigned char> cache_buffer; // �����buffer����
		};
	}

}


#endif