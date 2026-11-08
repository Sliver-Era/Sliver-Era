#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
//获取日志系统
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"

#ifdef _WIN32
//禁止 windows.h 定义 min / max 宏，避免污染标准库
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace engine
{
	//辅助工具命名空间
	namespace detail
	{
		//探测可执行文件路径（平台相关）
		inline std::filesystem::path exe_path_probe(void)
		{
#if defined(_WIN32) || defined(_WIN64)
			// Windows：使用宽字符 API 获取可执行文件路径，支持中文等 Unicode
			std::wstring buffer(32767, L'\0');
			//获取写入长度
			DWORD size = GetModuleFileNameW(nullptr, buffer.data(),
				static_cast<DWORD>(buffer.size()));
			//若首次获取失败
			if (size == 0)
				throw std::runtime_error("GetModuleFileNameW failed");
			//若缓冲区不足，扩容后重试
			if (static_cast<size_t>(size) >= buffer.size())
			{
				//扩容缓冲区
				buffer.resize(size + 1);
				//重新获取可执行文件路径
				size = GetModuleFileNameW(nullptr, buffer.data(),
					static_cast<DWORD>(buffer.size()));
				//若扩容后仍失败
				if (size == 0)
					throw std::runtime_error("GetModuleFileNameW failed after resize");
			}
			//按实际长度截断缓冲区
			buffer.resize(size);
			//构造路径对象
			return std::filesystem::path(buffer);

#elif defined(__linux__)
			// Linux：读取 /proc/self/exe 符号链接
			std::string buffer(PATH_MAX, '\0');
			//读取结果长度
			ssize_t len;
			//若缓冲区恰好填满，说明路径可能被截断，扩容后重试
			while ((len = readlink("/proc/self/exe", buffer.data(), buffer.size()))
				== static_cast<ssize_t>(buffer.size()))
				//扩容缓冲区
				buffer.resize(buffer.size() * 2);
			//若读取失败
			if (len == -1)
				throw std::runtime_error("readlink /proc/self/exe failed");
			//按实际长度截断缓冲区
			buffer.resize(len);
			//构造路径对象
			return std::filesystem::path(buffer);

#elif defined(__APPLE__)
			// macOS：使用 _NSGetExecutablePath，先获取所需大小
			uint32_t size = 0;
			_NSGetExecutablePath(nullptr, &size);
			//若无法获取所需大小
			if (size == 0)
				throw std::runtime_error("_NSGetExecutablePath failed to get size");
			//分配对应大小的缓冲区
			std::string buffer(size, '\0');
			//若获取路径失败
			if (_NSGetExecutablePath(buffer.data(), &size) != 0)
				throw std::runtime_error("_NSGetExecutablePath failed");
			//构造路径对象
			return std::filesystem::path(buffer);

#else
#error "Unsupported platform"
#endif
		}

		//获取可执行文件路径
		inline const std::filesystem::path& exe_path_get(void)
		{
			//首次调用时初始化，规避跨翻译单元静态初始化顺序问题
			static const std::filesystem::path path = [] {
				try
				{
					//探测可执行文件路径
					return exe_path_probe();
				}
				catch (const std::exception& e)
				{
					//记录错误信息
					logger.error("错误: {}", e.what());
					logger.error("已回退到工作目录");
					//回退到当前工作目录
					return std::filesystem::current_path();
				}
				}();
			//返回可执行文件路径
			return path;
		}

		//获取可执行文件目录
		inline const std::filesystem::path& exe_dir_get(void)
		{
			//首次调用时由可执行文件路径派生
			static const std::filesystem::path dir = exe_path_get().parent_path();
			//返回可执行文件目录
			return dir;
		}

		//绝对路径获取 —— 路径重载
		inline std::filesystem::path absolute_path_get(const std::filesystem::path& path)
		{
			//拼接可执行文件目录与相对路径
			return exe_dir_get() / path;
		}

		//绝对路径获取 —— 字符串重载
		inline std::filesystem::path absolute_path_get(const std::string& path)
		{
			//转化为可用字符串格式
			//要求string编码格式为UTF-8
			std::u8string u8_path(reinterpret_cast<const char8_t*>(path.data()), path.size());
			//返回拼接路径
			return exe_dir_get() / u8_path;
		}
	}
}