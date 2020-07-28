#ifndef WHAYER_PROTOCOL_PS__H_
#define WHAYER_PROTOCOL_PS__H_

namespace whayer {
	namespace protocol {
		namespace ps_const {
			const unsigned char kPacketStartCode[] = {0x00, 0x00, 0x01, 0xBA}; // 起始码
			const unsigned char kStuffingCode = 0x07; // 填充位长度计算值
			const unsigned char kSystemHeaderStartCode[] = {0x00, 0x00, 0x01, 0xBB}; // 系统头起始码
			const unsigned char kMapHeaderStartCode[] = {0x00, 0x00, 0x01, 0xBC}; // 映射头起始码
			const unsigned char kMinAudioStreamId = 0xC0; // 最小音频流ID
			const unsigned char kMaxAudioStreamId = 0xDf; // 最大音频流ID
			const unsigned char kMinVideoStreamId = 0xE0; // 量小视频流ID
			const unsigned char kMaxVideoStreamId = 0xEF; // 最大视频流ID
			const unsigned char kPesStartCode[] = {0x00, 0x00, 0x01}; // Pes包的起始码
			const unsigned char kAudioStream = 0x00; // 音频流
			const unsigned char kVideoStream = 0x01; // 视频流
			const unsigned char KOtherStream = 0xFF; // 其他流

			// mpeg2
			// ISO_IEC13818-1_2000.pdf    Page34
			// Table 2-18 – Stream_id assignments
			const unsigned char kProgramStreamMap = 0xBC;
			const unsigned char kPaddingStream = 0xBE;
			const unsigned char kPrivateStreamTwo = 0xBF;
			const unsigned char kEcmStream = 0xF0;
			const unsigned char kEmmStream = 0xF1;
			const unsigned char kH2220DsmccStream = 0xF2;
			const unsigned char kH2221eSream = 0xF8;
			const unsigned char kProgramStreamDirectory = 0xFF;
		}
		
		namespace ps_complex {

			enum PacketHeaderStatus{
				NONE,   // 未读取
				START_CODE, // 起始码
				HEADER, // 正在读Header
				PADDING, // 正在读Padding
				FINISHED // 已读完
			};

			struct PacketHeader{
				unsigned char status;
				unsigned char padding_length; // 填充长度大小 
				unsigned char start_code[4];  // 起始码
				unsigned char header_buffer[10]; // 其他字段
			};

			struct PacketSystemHeader{
				unsigned char start_code[4]; // 系统头起始码
				unsigned int header_length; // 系统头长度
			};

			struct PacketMapHeader{
				unsigned char start_code[4]; // 起始码
				unsigned int header_length;  // 映射头长度
			};

			struct PesPacket {
				unsigned char status;
				unsigned char stream_id; // 流ID  audio stream number 110x xxxx  [0xc0 ~ 0xdf]
										 //       video stream number 1110 xxxx  [0xe0 ~ 0xef]
				unsigned char pes_header_length; // pes header length
				unsigned char start_code[4]; // 起始码
				unsigned char packet_buffer[5];  // pes packet 缓存
				unsigned long pes_packet_length; // pes包长
				unsigned long pes_raw_length; // pes包长
			};

			struct Packet {
				unsigned char packet_status; // 是否结束 1.none 2.reading 3.finnished 
				unsigned char current_packet_type; // 当前包类型 1.none 2.PsHeader 3.PsSystemHeader 4.PsMapHeader 5.PesHeader 6.PesPayload
				unsigned int read_length; // 已经读取的payload长度
			};
		}
	}
}

#endif // !WHAYER_PS_PROTOCOL_H_
