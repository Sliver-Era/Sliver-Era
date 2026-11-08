#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"
//获取路径字符串转换方法
#include "路径字符串转换.h"
//获取日志系统
#include "Engine/EngineFoundation/src/log/日志系统运行包.h"

namespace engine
{
	//辅助工具命名空间
	namespace detail
	{
        //路径有效性检查 —— path重载
        inline bool path_check(const std::filesystem::path& target_path)
        {
            //若未解析出有效路径
            if (target_path.begin() == target_path.end())
            {
                logger.info("路径无效");
                return false;
            }

            //异常信息记录
            std::error_code ec;
            //若访问路径不存在或发生系统错误或非可读取文件
            if (!std::filesystem::exists(target_path, ec) || 
                !std::filesystem::is_regular_file(target_path, ec))
            {
                logger.info("访问路径异常\n请自行排查访问路径是否存在及是否可读取");
                //输出异常信息
                logger.info("{}", ec);
                //清空异常信息
                ec.clear();
                //返回检查未通过
                return false;
            }

            //若所有检查均通过
            return true;
        }
        //路径有效性检查 —— string重载
        inline bool path_check(const std::string& target_path)
        {
            //转化为标准路径
            std::filesystem::path suspect_path = string_to_path(target_path);
            //调用path重载
            return path_check(suspect_path);
        }

        //路径存在性检查（静默版，不写日志；供探测式调用使用）
        inline bool path_exists(const std::filesystem::path& target_path)
        {
            //异常信息记录
            std::error_code error_info;
            //存在且无系统错误
            return std::filesystem::exists(target_path, error_info) && !error_info;
        }
        //路径存在性检查 —— string重载
        inline bool path_exists(const std::string& target_path)
        {
            //转化为标准路径后调用path重载
            return path_exists(string_to_path(target_path));
        }

        //文件体量获取（失败时保留错误码）
        inline bool file_size_get(const std::filesystem::path& target_path,
            uintmax_t& size_out, std::error_code& error_info)
        {
            //先清空错误码
            error_info.clear();
            //读取文件体量
            size_out = std::filesystem::file_size(target_path, error_info);
            //返回是否成功
            return !error_info;
        }
        //文件体量获取 —— string重载
        inline bool file_size_get(const std::string& target_path,
            uintmax_t& size_out, std::error_code& error_info)
        {
            //转化为标准路径后调用path重载
            return file_size_get(string_to_path(target_path), size_out, error_info);
        }

        /*
        整文件读取
        以二进制方式读入全部字节；失败时清空输出并保留错误码。
        体量上限由调用方校验（本函数只负责读取）。
        */
        inline bool file_read_whole(const std::filesystem::path& target_path,
            std::vector<unsigned char>& bytes_out, std::error_code& error_info)
        {
            //先清空输出与错误码
            bytes_out.clear();
            error_info.clear();
            //以二进制方式打开并定位到末尾取体量
            std::ifstream file(target_path, std::ios::binary | std::ios::ate);
            //无法打开
            if (!file)
            {
                error_info = std::make_error_code(std::errc::no_such_file_or_directory);
                return false;
            }
            //文件体量
            const std::streamsize byte_count = file.tellg();
            //体量非法
            if (byte_count < 0)
            {
                error_info = std::make_error_code(std::errc::io_error);
                return false;
            }
            bytes_out.resize(static_cast<std::size_t>(byte_count));
            //回到文件头
            file.seekg(0, std::ios::beg);
            //空文件无需读取
            if (byte_count > 0)
            {
                file.read(reinterpret_cast<char*>(bytes_out.data()), byte_count);
                //读取不完整
                if (!file)
                {
                    error_info = std::make_error_code(std::errc::io_error);
                    bytes_out.clear();
                    return false;
                }
            }
            return true;
        }
        //整文件读取 —— string重载
        inline bool file_read_whole(const std::string& target_path,
            std::vector<unsigned char>& bytes_out, std::error_code& error_info)
        {
            //转化为标准路径后调用path重载
            return file_read_whole(string_to_path(target_path), bytes_out, error_info);
        }

        /*
        路径越界检查
        拒绝含 ".." 父目录返回的路径；供配置路径与资产外部 URI 约束使用。
        绝对路径不在此处限制，由调用方按场景决定。
        */
        inline bool path_traversal_check(const std::filesystem::path& target_path)
        {
            //逐节检查
            for (const auto& part : target_path)
            {
                //存在父目录返回符号
                if (part.string() == "..")
                    return false;
            }
            return true;
        }
        //路径越界检查 —— string重载
        inline bool path_traversal_check(const std::string& target_path)
        {
            //转化为标准路径后调用path重载
            return path_traversal_check(string_to_path(target_path));
        }
    }
}
