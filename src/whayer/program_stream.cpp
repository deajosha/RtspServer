#include "program_stream.h"
#include <memory>
#include <algorithm>
using namespace whayer::media;

char ProgramStream::get_start_code_position(unsigned char* buffer, unsigned long buffer_size,
	unsigned char code_type, unsigned char* &position) {
	if (buffer_size > 4) {
		unsigned long search_size = buffer_size - 4;
		for (unsigned long i = 0; i < search_size; ++i) {
			if (buffer[i] == 0x00 && buffer[i + 1] == 0x00 && buffer[i + 2] == 0x01 && buffer[i + 3] == code_type)
			{
				position = buffer + i;
				return 0;
			}
		}
	}
	
	return -1;
}

char ProgramStream::get_pes_start_code(unsigned char* buffer, unsigned long buffer_size, 
	unsigned char min_code_type, unsigned char max_code_type, unsigned char* &position)
{
	for (int i = 0; i < buffer_size - 4; ++i)
	{
		if (buffer[i] == 0x00 && buffer[i + 1] == 0x00 && buffer[i + 2] == 0x01 
			&& (buffer[i + 3] >= min_code_type && buffer[i + 3] <= max_code_type))
		{
			position = buffer + i;
			return 0;
		}
	}

	return -1;
}

char ProgramStream::parse_packet_header(unsigned char* buffer, unsigned long buffer_size, unsigned char* & position) {
	unsigned char packet_header_fixed_length = sizeof(packet_header_.start_code) + sizeof(packet_header_.header_buffer);
	if (buffer_size >= packet_header_fixed_length) {
		packet_header_.start_code[0] = buffer[0];
		packet_header_.start_code[1] = buffer[1];
		packet_header_.start_code[2] = buffer[2];
		packet_header_.start_code[3] = buffer[3];

		memcpy(packet_header_.header_buffer, buffer + sizeof(packet_header_.start_code), sizeof(packet_header_.header_buffer));
		packet_header_.padding_length = (packet_header_.header_buffer[9] & 0x05);
	}

	if (buffer_size - packet_header_fixed_length >= packet_header_.padding_length) {
		position = buffer + packet_header_fixed_length + packet_header_.padding_length;
		return 0;
	}
		
	return -1;
}

char ProgramStream::parse_pes_packet_header(unsigned char* buffer, unsigned long buffer_size, 
	unsigned char* &position) {
	unsigned char pes_packet_prefix_length = sizeof(pes_packet_.start_code) + sizeof(pes_packet_.packet_buffer);

	if (buffer_size >= pes_packet_prefix_length) {
		pes_packet_.start_code[0] = buffer[0];
		pes_packet_.start_code[1] = buffer[1];
		pes_packet_.start_code[2] = buffer[2];
		pes_packet_.start_code[3] = buffer[3];

		pes_packet_.stream_id = buffer[3];
		unsigned long pes_packet_buffer_length = sizeof(pes_packet_.packet_buffer);
		memcpy(pes_packet_.packet_buffer, buffer + sizeof(pes_packet_.start_code), pes_packet_buffer_length);
		pes_packet_.pes_packet_length = (pes_packet_.packet_buffer[0] << 8) + pes_packet_.packet_buffer[1]; // 此处需要考虑网络丢包
		pes_packet_.pes_header_length = pes_packet_.packet_buffer[4];
	}

	if ((buffer_size - pes_packet_prefix_length) >= pes_packet_.pes_header_length) {
		position = buffer + pes_packet_prefix_length + pes_packet_.pes_header_length;
		return 0;
	}
	
	return -1;
}

char ProgramStream::parse_pes_packet(unsigned char* buffer, unsigned long buffer_size, unsigned long& raw_length) {
	
	unsigned char stream_id = pes_packet_.stream_id;

	// 仅处理音视频流
	if (stream_id != whayer::protocol::ps_const::kProgramStreamMap
		&& stream_id != whayer::protocol::ps_const::kPaddingStream
		&& stream_id != whayer::protocol::ps_const::kPrivateStreamTwo
		&& stream_id != whayer::protocol::ps_const::kEmmStream
		&& stream_id != whayer::protocol::ps_const::kEmmStream
		&& stream_id != whayer::protocol::ps_const::kProgramStreamDirectory
		&& stream_id != whayer::protocol::ps_const::kH2220DsmccStream
		&& stream_id != whayer::protocol::ps_const::kH2221eSream)
	{
		unsigned long pes_data_length = pes_packet_.pes_packet_length - 3 - pes_packet_.pes_header_length;  // H264/Aac payload
		pes_packet_.pes_raw_length = pes_data_length;
		
		raw_length = std::min(pes_packet_.pes_raw_length, buffer_size);
		if (stream_id >= whayer::protocol::ps_const::kMinVideoStreamId
			&& stream_id <= whayer::protocol::ps_const::kMaxAudioStreamId) // 音频负载
		{
			return 0;
		}
		else if (stream_id >= whayer::protocol::ps_const::kMinVideoStreamId 
			&& stream_id <= whayer::protocol::ps_const::kMaxVideoStreamId) // 视频负载
		{
			return 0;
		}
		else {
			return 0;
		}
	}

	return 0;
}
