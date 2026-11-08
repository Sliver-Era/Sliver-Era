#pragma once
//预编译头
#include "Engine/EngineFoundation/common/前置头文件包含.h"

//通用算法模块
namespace engine
{
    //辅助工具命名空间
    namespace detail
    {
        /*
        去掉首尾空白
        文本行 / 材质名 / 资产路径可能带空格；全空白输入返回空串。
        */
        inline std::string text_trim(const std::string& text)
        {
            //首个非空白字符位置
            const std::size_t begin = text.find_first_not_of(" \t\r\n");
            //全空白
            if (begin == std::string::npos)
                return std::string();
            //末个非空白字符位置
            const std::size_t end = text.find_last_not_of(" \t\r\n");
            //返回裁剪后的文本
            return text.substr(begin, end - begin + 1);
        }

        //去掉行尾回车（Windows 文本）
        inline void carriage_return_trim(std::string& line)
        {
            //行尾为回车
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
        }

        //浮点文本解析（必须完整消费且为有限值）
        inline bool float_parse(const std::string& text, float& value)
        {
            //空串
            if (text.empty())
                return false;
            char* end = nullptr;
            const float parsed = std::strtof(text.c_str(), &end);
            //未完全消费（含尾随字符）或非有限值
            if (end == nullptr || *end != '\0' || !std::isfinite(parsed))
                return false;
            value = parsed;
            return true;
        }
    }
}
