//获取模块局部命名空间使用
#include "../局部命名空间使用.h"
//路径字符串转换（UTF-8 → 文件系统路径）
#include "src/detail/methods/路径字符串转换.h"
//文件路径校验与体量获取
#include "src/detail/methods/文件路径可用性校验.h"
//文本行解析工具
#include "src/detail/methods/package/文本处理工具.h"
//日志系统
#include "src/log/日志系统运行包.h"

namespace engine
{
    //模块私有实现
    namespace
    {
        //MTL 文件体量上限（16 MiB；材质库远小于网格）
        constexpr uint64_t max_file_bytes = 16ull * 1024 * 1024;
        //材质数量上限
        constexpr uint32_t max_materials = 4096u;

        //把数值钳到 [0, 1]（颜色 / 不透明度）
        float unit_clamp(float value)
        {
            if (!std::isfinite(value))
                return 0.0f;
            if (value < 0.0f)
                return 0.0f;
            if (value > 1.0f)
                return 1.0f;
            return value;
        }

        //读三通道颜色（缺分量按 0；越界钳到 [0,1]）
        void color_read(istringstream& stream, float (&color)[3])
        {
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            stream >> r;
            stream >> g;
            stream >> b;
            color[0] = unit_clamp(r);
            color[1] = unit_clamp(g);
            color[2] = unit_clamp(b);
        }

        /*
        贴图路径提取
        MTL 允许「map_Bump -bm 1.0 tex.png」这类带选项写法，此处取最后一个非选项标记作为路径。
        */
        string texture_path_read(istringstream& stream)
        {
            string token;
            string result;
            while (stream >> token)
            {
                //选项标记（-bm / -s / -o 等）跳过
                if (!token.empty() && token[0] == '-')
                    continue;
                result = token;
            }
            return result;
        }
    }

    //从 MTL 文件加载材质库
    bool Mtl_Loader::load_file(const string& mtl_path, vector<Mtl_Material>& receiver)
    {
        //先清空输出（任何失败路径都保持空）
        receiver.clear();
        //路径为空
        if (mtl_path.empty())
        {
            logger.warn("Mtl_Loader::MTL 路径为空");
            return false;
        }

        const path file_path = detail::string_to_path(mtl_path);
        uintmax_t byte_count = 0;
        error_code code;
        //文件不存在 / 不可读
        if (!detail::file_size_get(file_path, byte_count, code))
        {
            logger.warn("Mtl_Loader::无法读取 MTL 文件 ({})", mtl_path);
            return false;
        }
        //空文件或超限
        if (byte_count == 0 || byte_count > max_file_bytes)
        {
            logger.warn("Mtl_Loader::MTL 体量非法（{} 字节，上限 {}）", byte_count, max_file_bytes);
            return false;
        }

        ifstream file(file_path, ios::binary);
        if (!file)
        {
            logger.warn("Mtl_Loader::MTL 打开失败 ({})", mtl_path);
            return false;
        }

        //当前正在填写的材质与「是否已出现 newmtl」
        Mtl_Material material;
        bool in_material = false;

        string line;
        while (getline(file, line))
        {
            //去掉行尾回车
            detail::carriage_return_trim(line);
            istringstream stream(line);
            string tag;
            //空行
            if (!(stream >> tag))
                continue;
            //注释行
            if (tag[0] == '#')
                continue;

            //新材质
            if (tag == "newmtl")
            {
                //先把上一项收进输出
                if (in_material)
                    receiver.push_back(material);
                if (receiver.size() >= max_materials)
                {
                    logger.warn("Mtl_Loader::MTL 材质数量超限 ({})", mtl_path);
                    receiver.clear();
                    return false;
                }
                //重置为新材质（默认值来自 Mtl_Material 的成员初始化）
                material = Mtl_Material{};
                string name;
                getline(stream, name);
                material.name = detail::text_trim(name);
                in_material = true;
                continue;
            }
            //未出现 newmtl：其余行无效
            if (!in_material)
                continue;

            //漫反射颜色
            if (tag == "Kd")
                color_read(stream, material.diffuse);
            //镜面颜色
            else if (tag == "Ks")
                color_read(stream, material.specular);
            //自发光颜色
            else if (tag == "Ke")
                color_read(stream, material.emissive);
            //镜面指数（0..1000）
            else if (tag == "Ns")
            {
                float value = 0.0f;
                stream >> value;
                //非有限值按 0
                if (std::isfinite(value))
                {
                    if (value < 0.0f)
                        value = 0.0f;
                    if (value > 1000.0f)
                        value = 1000.0f;
                    material.shininess = value;
                }
            }
            //不透明度
            else if (tag == "d")
            {
                float value = 0.0f;
                stream >> value;
                material.opacity = unit_clamp(value);
            }
            //透明度（Tr = 1 − d）
            else if (tag == "Tr")
            {
                float value = 0.0f;
                stream >> value;
                material.opacity = 1.0f - unit_clamp(value);
            }
            //漫反射贴图
            else if (tag == "map_Kd")
                material.diffuse_map = texture_path_read(stream);
            //法线 / 凹凸贴图（map_Bump / bump / norm / map_bump 视同）
            else if (tag == "map_Bump" || tag == "map_bump" || tag == "bump" || tag == "norm")
                material.normal_map = texture_path_read(stream);
            //其余行（Ka / Ni / illum / 参数等）忽略
        }

        //收尾最后一项
        if (in_material)
            receiver.push_back(material);

        //一个材质都没读到
        if (receiver.empty())
        {
            logger.warn("Mtl_Loader::MTL 未包含任何 newmtl ({})", mtl_path);
            return false;
        }
        return true;
    }
}
