#ifndef XOP_AAC_PARSER_H_
#define XOP_AAC_PARSER_H_

#include <iostream>

namespace xop {
	struct AdtsHeader
	{
		unsigned int syncword;  //12 bit 同步字 '1111 1111 1111'，说明一个ADTS帧的开始
		unsigned int id;        //1 bit MPEG 标示符， 0 for MPEG-4，1 for MPEG-2
		unsigned int layer;     //2 bit 总是'00'
		unsigned int protectionAbsent;  //1 bit 1表示没有crc，0表示有crc
		unsigned int profile;           //1 bit 表示使用哪个级别的AAC
		unsigned int samplingFreqIndex; //4 bit 表示使用的采样频率
		unsigned int privateBit;        //1 bit
		unsigned int channelCfg; //3 bit 表示声道数
		unsigned int originalCopy;         //1 bit 
		unsigned int home;                  //1 bit 

		/*下面的为改变的参数即每一帧都不同*/
		unsigned int copyrightIdentificationBit;   //1 bit
		unsigned int copyrightIdentificationStart; //1 bit
		unsigned int aacFrameLength;               //13 bit 一个ADTS帧的长度包括ADTS头和AAC原始流
		unsigned int adtsBufferFullness;           //11 bit 0x7FF 说明是码率可变的码流

		/* number_of_raw_data_blocks_in_frame
		* 表示ADTS帧中有number_of_raw_data_blocks_in_frame + 1个AAC原始帧
		* 所以说number_of_raw_data_blocks_in_frame == 0
		* 表示说ADTS帧中有一个AAC数据块并不是说没有。(一个AAC原始帧包含一段时间内1024个采样及相关数据)
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