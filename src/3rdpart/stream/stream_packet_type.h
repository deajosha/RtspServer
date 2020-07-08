#ifndef WHAYER_ACCESS_GATEWAY_STREAM_PACKET_TYPE_H_
#define WHAYER_ACCESS_GATEWAY_STREAM_PACKET_TYPE_H_

#include <memory>
namespace whayer {
	namespace access_gateway {
		
		typedef std::function<void(unsigned char)> ShareCallback;

		struct PsPacketObject { // PS packet
			std::unique_ptr<unsigned char> buffer;
			unsigned long buffer_size;

			PsPacketObject() {}
			PsPacketObject(const unsigned char* cache, unsigned long cache_size) {
				buffer.reset(new unsigned char[cache_size]);
				memcpy(buffer.get(), cache, cache_size);
				buffer_size = cache_size;
			}

			PsPacketObject& operator=(const PsPacketObject& ps_packet_object) {
				if (this != &ps_packet_object) {
					buffer.reset(new unsigned char[ps_packet_object.buffer_size]);
					memcpy(buffer.get(), ps_packet_object.buffer.get(), ps_packet_object.buffer_size);
					buffer_size = ps_packet_object.buffer_size;
				}

				return *this;
			}
			PsPacketObject(const PsPacketObject& ps_packet_object) {
				buffer.reset(new unsigned char[ps_packet_object.buffer_size]);
				memcpy(buffer.get(), ps_packet_object.buffer.get(), ps_packet_object.buffer_size);
				buffer_size = ps_packet_object.buffer_size;
			}
		};
	}
}


#endif
