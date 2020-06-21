#ifndef XOP_H264_PARSER_H
#define XOP_H264_PARSER_H

#include <cstdint> 
#include <utility> 

namespace xop
{

class H264Parser
{
public:    
	static inline int find_five_bytes_start_code(const uint8_t* buffer);
	static inline int three_bytes_start_code(const uint8_t* buf);
	static inline int four_bytes_start_code(const uint8_t* buf);
	static inline uint8_t* find_next_start_code(const uint8_t* buf, int len);
private:
  
};
    
}

inline int xop::H264Parser::three_bytes_start_code(const uint8_t* buf) {
	bool three_bytes = (0x0 == buf[0] && 0x0 == buf[1] && 0x1 == buf[2]);
	return three_bytes ? 1 : 0;
}

inline int xop::H264Parser::find_five_bytes_start_code(const uint8_t* buffer) {
	bool five_bytes = (0x0 == buffer[0] && 0x0 == buffer[1] && 0x0 == buffer[2] && 0x1 == buffer[3] && 0x67 == buffer[4]);
	return five_bytes ? 1 : 0;
}

inline int xop::H264Parser::four_bytes_start_code(const uint8_t* buf) {
	bool four_bytes = (0x0 == buf[0] && 0x0 == buf[1] && 0x0 == buf[2] && 0x1 == buf[3]);
	return four_bytes ? 1 : 0;
}

inline uint8_t* xop::H264Parser::find_next_start_code(const uint8_t* buf, int len) {
	uint8_t* next_start_code = NULL;

	if (len > 3) {
		for (int i = 0; i < len - 3; ++i)
		{
			if (H264Parser::three_bytes_start_code(buf)
				|| H264Parser::four_bytes_start_code(buf)) {
				next_start_code = const_cast<uint8_t*>(buf);
				break;
			}
			++buf;
		}

		if (H264Parser::three_bytes_start_code(buf)) {
			next_start_code = const_cast<uint8_t*>(buf);
		}
	}

	return next_start_code;
}

#endif 

