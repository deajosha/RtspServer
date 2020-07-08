#ifndef WHAYER_ACCESS_GATEWAY_SHARE_MEMORY_API_H_
#define WHAYER_ACCESS_GATEWAY_SHARE_MEMORY_API_H_

#if defined(STREAM_SHARE_MEMORY_EXPORTS)
#define STREAM_SHARE_MEMORY_API  __declspec(dllexport) 
#else 
#define STREAM_SHARE_MEMORY_API  __declspec(dllimport) 
#endif

#include <memory>
#include <functional>
#include "stream_packet_type.h"

namespace whayer {
	namespace access_gateway {
		namespace stream_share_memory {
			/**
				����һ��д�Ĺ����ڴ����
				share_file_name: �����ڴ��ļ����ƣ����ܳ���32�ֽ�
				memory_buffer_size: �����ڴ�д���ڴ��С
				create_cache�� �Ƿ񴴽�������У�Ĭ�ϲ�����
				callback: �����ڴ泬ʱ����ص�
				semphore_timeout: д�����ڴ泬ʱ��С��Ĭ��10��
			*/
			STREAM_SHARE_MEMORY_API bool create_writer(const char* share_file_name, unsigned long memory_buffer_size,
				bool create_cache = false, std::function<void(char)> callback = nullptr, unsigned long semphore_timeout = 3000);

			/**
				����һ�����Ĺ����ڴ����
				share_file_name: �����ڴ��ļ����ƣ����ܳ���32�ֽ�
				create_cache�� �Ƿ񴴽�������У�Ĭ�ϲ�����
				callback: �����ڴ泬ʱ����ص�
				semphore_timeout: �������ڴ泬ʱ��С��Ĭ��10��
			*/
			STREAM_SHARE_MEMORY_API bool create_reader(const char* share_file_name, bool create_cache = false,
				std::function<void(char)> callback = nullptr, unsigned long semphore_timeout = 3000);

			/**
				��ʼ�������¼�������
				share_file_name: �����ڴ��ļ����ƣ����ܳ���32�ֽ�
			*/
			STREAM_SHARE_MEMORY_API bool init_reader(const char* share_file_name);

			/**
				��ʼ��д���¼�������
				share_file_name: �����ڴ��ļ����ƣ����ܳ���32�ֽ�
			*/
			STREAM_SHARE_MEMORY_API bool init_writer(const char* share_file_name);
			/**
				�����ڴ�д����
				share_file_name: �����ڴ��ʶ
				buffer: ��������
				buffer_size: �������ݴ�С
			*/
			STREAM_SHARE_MEMORY_API bool write_memory(const char* share_file_name, const unsigned char* buffer, unsigned int buffer_size, bool end = false);

			/**
				�ӹ����ڴ������
				share_file_name: �����ڴ��ʶ
				buffer: ��ȡ�Ĺ����ڴ�����
				buffer_size: ��ȡ�����ڴ�����ݴ�С
				end: д�������ݶ��Ƿ����д
			*/
			STREAM_SHARE_MEMORY_API bool read_memory(const char* share_file_name, std::unique_ptr<unsigned char>& buffer, unsigned int& buffer_size, bool& end);

			/**
				д�����ڴ����뻺��
				share_file_name�� �����ڴ��ʶ
				buffer: ��������
				buffer_size: �������ݴ�С
			*/
			STREAM_SHARE_MEMORY_API bool writer_push_cache(const char* share_file_name, const unsigned char* buffer, unsigned long buffer_size);

			/**
				д�����ڴ���ȡ����
				share_file_name�� �����ڴ��ʶ
				packet_object: ������õ�������
			*/
			STREAM_SHARE_MEMORY_API bool writer_pop_cache(const char* share_file_name, whayer::access_gateway::PsPacketObject& packet_object);

			/**
				�������ڴ����뻺��
				share_file_name�� �����ڴ��ʶ
				buffer: ��������
				buffer_size: �������ݴ�С
			*/
			STREAM_SHARE_MEMORY_API bool reader_push_cache(const char* share_file_name, const unsigned char* buffer, unsigned long buffer_size);

			/**
				�������ڴ���ȡ����
				share_file_name�� �����ڴ��ʶ
				packet_object: ������õ�������
			*/
			STREAM_SHARE_MEMORY_API bool reader_pop_cache(const char* share_file_name, whayer::access_gateway::PsPacketObject& packet_object);

			/**
				ֹͣһ��д�Ĺ����ڴ����
				share_file_name: �����ڴ��ļ����ƣ����ܳ���32�ֽ�
			*/
			STREAM_SHARE_MEMORY_API void stop_writer(const char* share_file_name);

			/**
				ֹͣһ�����Ĺ����ڴ����
				share_file_name: �����ڴ��ļ����ƣ����ܳ���32�ֽ�
			*/
			STREAM_SHARE_MEMORY_API void stop_reader(const char* share_file_name);

			/**
				�ͷ����еĶ�ȡ�����ڴ����
			*/
			STREAM_SHARE_MEMORY_API void release_all();
		}
	}
}

#endif