#ifndef WHAYER_SHARE_MEMORY_API_
#define WHAYER_SHARE_MEMORY_API_

#if defined(SHARE_MEMORY_EXPORTS)
#define SHARE_MEMORY_API  __declspec(dllexport) 
#else 
#define SHARE_MEMORY_API  __declspec(dllimport) 
#endif

#include <functional>
#include "share_memory_type.h"

/**
	创建一个写的共享内存对象
	share_file_name: 共享内存文件名称，不能超过32字节
	memory_buffer_size: 向共享内存写的内存大小
	create_cache： 是否创建缓存队列，默认不创建
	callback: 共享内存超时出错回调
	semphore_timeout: 写共享内存超时大小，默认10秒
*/
SHARE_MEMORY_API bool create_writer(const char* share_file_name, unsigned long memory_buffer_size,
	bool create_cache=false, std::function<void(char)> callback = nullptr, unsigned long semphore_timeout = 10000);

/**
	创建一个读的共享内存对象
	share_file_name: 共享内存文件名称，不能超过32字节
	create_cache： 是否创建缓存队列，默认不创建
	callback: 共享内存超时出错回调
	semphore_timeout: 读共享内存超时大小，默认10秒
*/
SHARE_MEMORY_API bool create_reader(const char* share_file_name, bool create_cache=false, 
	std::function<void(char)> callback = nullptr, unsigned long semphore_timeout = 10000);

/**
	初始化读的事件监听器
	share_file_name: 共享内存文件名称，不能超过32字节
*/
SHARE_MEMORY_API bool init_reader(const char* share_file_name);

/**
	向共享内存写数据
	share_file_name: 共享内存标识
	buffer: 共享数据
	buffer_size: 共享数据大小
*/
SHARE_MEMORY_API bool write_memory(const char* share_file_name, const unsigned char* buffer, unsigned long buffer_size);

/**
	从共享内存读数据
	share_file_name: 共享内存标识
	buffer: 读取的共享内存数据
	buffer_size: 读取共享内存的数据大小
	end: 写共享数据端是否结束写
*/
SHARE_MEMORY_API bool read_memory(const char* share_file_name, std::unique_ptr<unsigned char>& buffer, unsigned long& buffer_size, bool& end);

/**
	写共享内存者入缓存
	share_file_name： 共享内存标识 
	buffer: 缓存数据
	buffer_size: 缓存数据大小
*/
SHARE_MEMORY_API bool writer_push_cache(const char* share_file_name, const unsigned char* buffer, unsigned long buffer_size);

/**
	写共享内存者取缓存
	share_file_name： 共享内存标识
	packet_object: 读缓存得到的数据
*/
SHARE_MEMORY_API bool writer_pop_cache(const char* share_file_name, whayer::share_memory::PsPacketObject& packet_object);

/**
	读共享内存者入缓存
	share_file_name： 共享内存标识
	buffer: 缓存数据
	buffer_size: 缓存数据大小
*/
SHARE_MEMORY_API bool reader_push_cache(const char* share_file_name, const unsigned char* buffer, unsigned long buffer_size);

/**
	读共享内存者取缓存
	share_file_name： 共享内存标识
	packet_object: 读缓存得到的数据
*/
SHARE_MEMORY_API bool reader_pop_cache(const char* share_file_name, whayer::share_memory::PsPacketObject& packet_object);

/**
	停止一个写的共享内存对象
	share_file_name: 共享内存文件名称，不能超过32字节
*/
SHARE_MEMORY_API void stop_writer(const char* share_file_name);

/**
	停止一个读的共享内存对象
	share_file_name: 共享内存文件名称，不能超过32字节
*/
SHARE_MEMORY_API void stop_reader(const char* share_file_name);

/**
	释放所有的读取共享内存对象
*/
SHARE_MEMORY_API void release_all();

#endif
