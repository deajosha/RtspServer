#ifndef WHAYER_SHARE_MEMORY_TYPE_H_
#define WHAYER_SHARE_MEMORY_TYPE_H_

namespace whayer {
	namespace share_memory {
		struct ShareMemoryObject
		{
			bool end;				    // 停止共享标识 
			unsigned long buffer_size; // 共享buffer大小
		};

		typedef ShareMemoryObject* ShareMemoryObjectPtr;

		const unsigned char kMaxShareFileLength = 32;
		const unsigned char kSemaphoreNameLength = 64;

		// PS packet
		struct PsPacketObject {
			std::unique_ptr<unsigned char> buffer;
			unsigned long buffer_size;

			PsPacketObject() {};
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
