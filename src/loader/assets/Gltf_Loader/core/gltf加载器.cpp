//获取模块局部命名空间使用
#include "../局部命名空间使用.h"
//路径字符串转换（UTF-8 ↔ 文件系统路径）
#include "src/detail/methods/路径字符串转换.h"
//文件路径校验、体量获取与整文件读取
#include "src/detail/methods/文件路径可用性校验.h"
//日志系统
#include "src/log/日志系统运行包.h"
//解析限制
#include "gltf配置.h"
//模型转换
#include "gltf转换.h"
//tinygltf 声明
#include "tinygltf包含.h"

namespace engine
{
    //模块私有实现
    namespace
    {
        //文件存在性判定（路径按 UTF-8 解释，支持中文目录；拒绝越界路径）
        bool fs_exists(const string& file_path, void*)
        {
            //外部资源不允许父目录返回
            if (!detail::path_traversal_check(file_path))
                return false;
            return detail::path_exists(file_path);
        }

        //路径展开（不做环境变量 / 波浪号展开，原样返回）
        string fs_expand(const string& file_path, void*)
        {
            return file_path;
        }

        //整文件读取（供 tinygltf 读取外部 buffer）
        bool fs_read(vector<unsigned char>* out, string* err, const string& file_path, void*)
        {
            //外部资源不允许父目录返回
            if (!detail::path_traversal_check(file_path))
            {
                if (err)
                    *err = "外部文件路径越界: " + file_path;
                return false;
            }
            //复用通用整文件读取
            error_code code;
            if (!detail::file_read_whole(file_path, *out, code))
            {
                if (err)
                    *err = "外部文件读取失败: " + file_path + " (" + code.message() + ")";
                return false;
            }
            return true;
        }

        //整文件写入（本加载器只读，不提供写入能力）
        bool fs_write(string* err, const string&, const vector<unsigned char>&, void*)
        {
            if (err)
                *err = "加载器不支持写入";
            return false;
        }

        //文件体量获取
        bool fs_size(size_t* size_out, string* err, const string& file_path, void*)
        {
            //外部资源不允许父目录返回
            if (!detail::path_traversal_check(file_path))
            {
                if (err)
                    *err = "外部文件路径越界: " + file_path;
                return false;
            }
            //复用通用体量获取
            uintmax_t byte_count = 0;
            error_code code;
            if (!detail::file_size_get(file_path, byte_count, code))
            {
                if (err)
                    *err = "无法获取外部文件体量: " + file_path + " (" + code.message() + ")";
                return false;
            }
            *size_out = static_cast<size_t>(byte_count);
            return true;
        }
    }

    //加载 glTF 文件（静态子集）
    bool Gltf_Loader::load_file(const string& gltf_path, Gltf_Asset& receiver)
    {
        //先清空输出（任何失败路径都保持空）
        receiver = Gltf_Asset{};
        //路径为空
        if (gltf_path.empty())
        {
            logger.warn("Gltf_Loader::glTF 路径为空");
            return false;
        }

        const path file_path = detail::string_to_path(gltf_path);
        uintmax_t byte_count = 0;
        error_code code;
        //文件不存在 / 不可读
        if (!detail::file_size_get(file_path, byte_count, code))
        {
            logger.warn("Gltf_Loader::无法读取 glTF 文件 ({})", gltf_path);
            return false;
        }
        //空文件或超限
        if (byte_count == 0 || byte_count > gltf_detail::max_file_bytes)
        {
            logger.warn("Gltf_Loader::glTF 体量非法（{} 字节，上限 {}）",
                byte_count, gltf_detail::max_file_bytes);
            return false;
        }

        //原始字节（按魔数分流 .glb / .gltf）
        vector<unsigned char> bytes;
        if (!detail::file_read_whole(file_path, bytes, code))
        {
            logger.warn("Gltf_Loader::glTF 读取失败 ({})", gltf_path);
            return false;
        }

        //GLB 魔数为 "glTF"
        const bool is_binary = bytes.size() >= 4
            && memcmp(bytes.data(), "glTF", 4) == 0;

        /*
        外部 buffer 基准目录
        tinygltf 会把 buffer 的 uri 拼到 base_dir 后交给文件系统回调；
        此处传 glTF 自身所在目录（UTF-8），相对路径按进程工作目录解析。
        */
        const path parent = file_path.parent_path();
        const string base_dir = parent.empty() ? string(".") : detail::path_to_string(parent);

        tinygltf::TinyGLTF loader;
        tinygltf::FsCallbacks callbacks;
        callbacks.FileExists = fs_exists;
        callbacks.ExpandFilePath = fs_expand;
        callbacks.ReadWholeFile = fs_read;
        callbacks.WriteWholeFile = fs_write;
        callbacks.GetFileSizeInBytes = fs_size;
        callbacks.user_data = nullptr;
        //解析错误 / 告警
        string parse_error;
        string parse_warning;
        loader.SetFsCallbacks(callbacks, &parse_error);

        tinygltf::Model model;
        bool parsed = false;
        try
        {
            parsed = is_binary
                ? loader.LoadBinaryFromMemory(&model, &parse_error, &parse_warning,
                    bytes.data(), static_cast<unsigned int>(bytes.size()), base_dir)
                : loader.LoadASCIIFromString(&model, &parse_error, &parse_warning,
                    reinterpret_cast<const char*>(bytes.data()),
                    static_cast<unsigned int>(bytes.size()), base_dir);
        }
        catch (const std::exception& exception)
        {
            parse_error = exception.what();
        }
        //解析失败
        if (!parsed)
        {
            logger.warn("Gltf_Loader::解析失败 ({})：{}", gltf_path, parse_error);
            return false;
        }
        //解析告警（不阻断）
        if (!parse_warning.empty())
            logger.warn("Gltf_Loader::解析告警 ({})：{}", gltf_path, parse_warning);

        //转换为引擎数据
        Gltf_Asset result;
        if (!gltf_detail::model_convert(model, result))
        {
            logger.warn("Gltf_Loader::模型转换失败 ({})", gltf_path);
            return false;
        }
        //整体交出结果
        receiver = std::move(result);
        return true;
    }
}
