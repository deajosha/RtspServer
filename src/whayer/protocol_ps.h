#ifndef WHAYER_PROTOCOL_PS__H_
#define WHAYER_PROTOCOL_PS__H_

namespace whayer {
	namespace protocol {
		namespace ps_const {
			const unsigned char kPacketStartCode[] = {0x00, 0x00, 0x01, 0xBA}; // ��ʼ��
			const unsigned char kStuffingCode = 0x07; // ���λ���ȼ���ֵ
			const unsigned char kSystemHeaderStartCode[] = {0x00, 0x00, 0x01, 0xBB}; // ϵͳͷ��ʼ��
			const unsigned char kMapHeaderStartCode[] = {0x00, 0x00, 0x01, 0xBC}; // ӳ��ͷ��ʼ��
			const unsigned char kMinAudioStreamId = 0xC0; // ��С��Ƶ��ID
			const unsigned char kMaxAudioStreamId = 0xDf; // �����Ƶ��ID
			const unsigned char kMinVideoStreamId = 0xE0; // ��С��Ƶ��ID
			const unsigned char kMaxVideoStreamId = 0xEF; // �����Ƶ��ID
			const unsigned char kPesStartCode[] = {0x00, 0x00, 0x01}; // Pes������ʼ��
			const unsigned char kAudioStream = 0x00; // ��Ƶ��
			const unsigned char kVideoStream = 0x01; // ��Ƶ��
			const unsigned char KOtherStream = 0xFF; // ������

			// mpeg2
			// ISO_IEC13818-1_2000.pdf    Page34
			// Table 2-18 �C Stream_id assignments
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
				NONE,   // δ��ȡ
				START_CODE, // ��ʼ��
				HEADER, // ���ڶ�Header
				PADDING, // ���ڶ�Padding
				FINISHED // �Ѷ���
			};

			struct PacketHeader{
				unsigned char status;
				unsigned char padding_length; // ��䳤�ȴ�С 
				unsigned char start_code[4];  // ��ʼ��
				unsigned char header_buffer[10]; // �����ֶ�
			};

			struct PacketSystemHeader{
				unsigned char start_code[4]; // ϵͳͷ��ʼ��
				unsigned int header_length; // ϵͳͷ����
			};

			struct PacketMapHeader{
				unsigned char start_code[4]; // ��ʼ��
				unsigned int header_length;  // ӳ��ͷ����
			};

			struct PesPacket {
				unsigned char status;
				unsigned char stream_id; // ��ID  audio stream number 110x xxxx  [0xc0 ~ 0xdf]
										 //       video stream number 1110 xxxx  [0xe0 ~ 0xef]
				unsigned char pes_header_length; // pes header length
				unsigned char start_code[4]; // ��ʼ��
				unsigned char packet_buffer[5];  // pes packet ����
				unsigned long pes_packet_length; // pes����
				unsigned long pes_raw_length; // pes����
			};

			struct Packet {
				unsigned char packet_status; // �Ƿ���� 1.none 2.reading 3.finnished 
				unsigned char current_packet_type; // ��ǰ������ 1.none 2.PsHeader 3.PsSystemHeader 4.PsMapHeader 5.PesHeader 6.PesPayload
				unsigned int read_length; // �Ѿ���ȡ��payload����
			};
		}
	}
}

#endif // !WHAYER_PS_PROTOCOL_H_
