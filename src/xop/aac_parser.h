#ifndef XOP_AAC_PARSER_H_
#define XOP_AAC_PARSER_H_

#include <iostream>

namespace xop {
	struct AdtsHeader
	{
		unsigned int syncword;  //12 bit ͬ���� '1111 1111 1111'��˵��һ��ADTS֡�Ŀ�ʼ
		unsigned int id;        //1 bit MPEG ��ʾ���� 0 for MPEG-4��1 for MPEG-2
		unsigned int layer;     //2 bit ����'00'
		unsigned int protectionAbsent;  //1 bit 1��ʾû��crc��0��ʾ��crc
		unsigned int profile;           //1 bit ��ʾʹ���ĸ������AAC
		unsigned int samplingFreqIndex; //4 bit ��ʾʹ�õĲ���Ƶ��
		unsigned int privateBit;        //1 bit
		unsigned int channelCfg; //3 bit ��ʾ������
		unsigned int originalCopy;         //1 bit 
		unsigned int home;                  //1 bit 

		/*�����Ϊ�ı�Ĳ�����ÿһ֡����ͬ*/
		unsigned int copyrightIdentificationBit;   //1 bit
		unsigned int copyrightIdentificationStart; //1 bit
		unsigned int aacFrameLength;               //13 bit һ��ADTS֡�ĳ��Ȱ���ADTSͷ��AACԭʼ��
		unsigned int adtsBufferFullness;           //11 bit 0x7FF ˵�������ʿɱ������

		/* number_of_raw_data_blocks_in_frame
		* ��ʾADTS֡����number_of_raw_data_blocks_in_frame + 1��AACԭʼ֡
		* ����˵number_of_raw_data_blocks_in_frame == 0
		* ��ʾ˵ADTS֡����һ��AAC���ݿ鲢����˵û�С�(һ��AACԭʼ֡����һ��ʱ����1024���������������)
		*/
		unsigned int numberOfRawDataBlockInFrame; //2 bit
	};

	class Acc_Parser {
	public:
		static int parser_adts_header(unsigned char* buffer, struct AdtsHeader* header) {
			memset(header, 0, sizeof(*header));

			if ((buffer[0] == 0xFF) && ((buffer[1] & 0xF0) == 0xF0))
			{
				header->id = ((unsigned int)buffer[1] & 0x08) >> 3;
				header->layer = ((unsigned int)buffer[1] & 0x06) >> 1;
				header->protectionAbsent = (unsigned int)buffer[1] & 0x01;
				header->profile = ((unsigned int)buffer[2] & 0xc0) >> 6;
				header->samplingFreqIndex = ((unsigned int)buffer[2] & 0x3c) >> 2;
				header->privateBit = ((unsigned int)buffer[2] & 0x02) >> 1;
				header->channelCfg = ((((unsigned int)buffer[2] & 0x01) << 2) | (((unsigned int)buffer[3] & 0xc0) >> 6));
				header->originalCopy = ((unsigned int)buffer[3] & 0x20) >> 5;
				header->home = ((unsigned int)buffer[3] & 0x10) >> 4;
				header->copyrightIdentificationBit = ((unsigned int)buffer[3] & 0x08) >> 3;
				header->copyrightIdentificationStart = (unsigned int)buffer[3] & 0x04 >> 2;
				header->aacFrameLength = (((((unsigned int)buffer[3]) & 0x03) << 11) |
					(((unsigned int)buffer[4] & 0xFF) << 3) |
					((unsigned int)buffer[5] & 0xE0) >> 5);
				header->adtsBufferFullness = (((unsigned int)buffer[5] & 0x1f) << 6 |
					((unsigned int)buffer[6] & 0xfc) >> 2);
				header->numberOfRawDataBlockInFrame = ((unsigned int)buffer[6] & 0x03);

				return 0;
			}

			return -1;
		}
	};
}

#endif