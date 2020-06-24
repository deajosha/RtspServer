#ifndef WHAYER_UTILE_STREAM_CACHE_H_
#define WHAYER_UTILE_STREAM_CACHE_H_

#include <mutex>
#include <queue>
namespace whayer {
	namespace util {

		const unsigned long kMaxPsPacketLength = 1024 * 200;

		struct PsPacketObject {
			unsigned char buffer[kMaxPsPacketLength];
			unsigned long buffer_size;

			PsPacketObject() {};
			PsPacketObject(unsigned char* cache, unsigned long cache_size) {
				memcpy(buffer, cache, cache_size);
				buffer_size = cache_size;
			}

			PsPacketObject& operator=(const PsPacketObject& ps_packet_object) {
				if (this != &ps_packet_object) {
					memcpy(buffer, ps_packet_object.buffer, ps_packet_object.buffer_size);
					buffer_size = ps_packet_object.buffer_size;
				}

				return *this;
			}

			PsPacketObject(const PsPacketObject& ps_packet_object) {
				memcpy(buffer, ps_packet_object.buffer, ps_packet_object.buffer_size);
				buffer_size = ps_packet_object.buffer_size;
			}
		};

		class StreamCache{
		public:
			StreamCache() {
				
			}

			bool write_cache(unsigned char* cache, unsigned long buffer_size) {
				std::lock_guard<std::mutex> write_locker(cache_mutex_);
				std::shared_ptr<PsPacketObject> ps_packet_object(new PsPacketObject(cache, buffer_size));
				cache_queue_.push(std::move(ps_packet_object));
				return true;
			}

			bool read_cache(PsPacketObject & ps_packet_object) {
				std::lock_guard<std::mutex> read_locker(cache_mutex_);
				if (!cache_queue_.empty()) {
					std::shared_ptr<PsPacketObject> ps_packet = cache_queue_.front();
					ps_packet_object = *ps_packet;
					cache_queue_.pop();
				}
				else {
					return false;
				}

				return true;
			}

			void clear() {
				std::queue<std::shared_ptr<PsPacketObject>> empty;
				std::swap(cache_queue_, empty);
			}
		private:
			std::mutex cache_mutex_;
			std::queue<std::shared_ptr<PsPacketObject>> cache_queue_;
		};
	}
}

#endif
