//获取模块局部命名空间使用
#include "../局部命名空间使用.h"
//解析限制
#include "gltf配置.h"
//模型转换声明
#include "gltf转换.h"
//日志系统
#include "src/log/日志系统运行包.h"

namespace engine
{
    //模块私有实现
    namespace
    {
        //图元转换结果
        enum class Primitive_Result
        {
            //转换成功
            ok,
            //不支持的图元（跳过）
            skip,
            //数据非法（整份失败）
            fail
        };

        //组件类型字节数（不支持的组件返回 0）
        size_t component_bytes(int component_type)
        {
            switch (component_type)
            {
            case TINYGLTF_COMPONENT_TYPE_BYTE:
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                return 1u;
            case TINYGLTF_COMPONENT_TYPE_SHORT:
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                return 2u;
            case TINYGLTF_COMPONENT_TYPE_INT:
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            case TINYGLTF_COMPONENT_TYPE_FLOAT:
                return 4u;
            default:
                return 0u;
            }
        }

        //访问器类型对应的分量数（不支持的类型返回 0）
        int type_components(int accessor_type)
        {
            switch (accessor_type)
            {
            case TINYGLTF_TYPE_SCALAR:
                return 1;
            case TINYGLTF_TYPE_VEC2:
                return 2;
            case TINYGLTF_TYPE_VEC3:
                return 3;
            case TINYGLTF_TYPE_VEC4:
                return 4;
            default:
                return 0;
            }
        }

        /*
        访问器位置解析
        把 bufferView 起点、访问器偏移、元素步长与缓冲区体量都校验一遍；
        任一项越界即失败，避免后续按错误偏移读取内存。
        */
        bool accessor_range_get(const tinygltf::Model& model, const tinygltf::Accessor& accessor,
            int expected_components, size_t& start_out, size_t& stride_out,
            size_t& element_size_out, const unsigned char*& buffer_out, string& error)
        {
            //访问器未挂 bufferView（稀疏访问器）
            if (accessor.bufferView < 0
                || static_cast<size_t>(accessor.bufferView) >= model.bufferViews.size())
            {
                error = "访问器缺少有效 bufferView（稀疏访问器不支持）";
                return false;
            }
            const tinygltf::BufferView& buffer_view = model.bufferViews[accessor.bufferView];
            //bufferView 指向的缓冲区非法
            if (buffer_view.buffer < 0
                || static_cast<size_t>(buffer_view.buffer) >= model.buffers.size())
            {
                error = "bufferView 指向的缓冲区非法";
                return false;
            }
            //分量数与期望不符
            const int components = type_components(accessor.type);
            if (components != expected_components)
            {
                error = "访问器分量数与期望不符";
                return false;
            }
            //组件类型不支持
            const size_t component_size = component_bytes(accessor.componentType);
            if (component_size == 0)
            {
                error = "访问器组件类型不支持";
                return false;
            }
            const size_t element_size = component_size * static_cast<size_t>(components);
            //步长为 0 表示紧排
            const size_t stride = buffer_view.byteStride == 0
                ? element_size : buffer_view.byteStride;
            const size_t start = buffer_view.byteOffset + accessor.byteOffset;
            const size_t buffer_size = model.buffers[buffer_view.buffer].data.size();
            //末元素越界
            const size_t last_end = start
                + stride * static_cast<size_t>(accessor.count == 0 ? 0 : accessor.count - 1)
                + element_size;
            if (start > buffer_size || last_end > buffer_size)
            {
                error = "访问器数据越界";
                return false;
            }
            start_out = start;
            stride_out = stride;
            element_size_out = element_size;
            buffer_out = model.buffers[buffer_view.buffer].data.data();
            return true;
        }

        //单个分量读取（整数分量按 normalized 归一化）
        bool component_read(const unsigned char* pointer, int component_type, bool normalized,
            float& value_out)
        {
            switch (component_type)
            {
            case TINYGLTF_COMPONENT_TYPE_FLOAT:
            {
                float value = 0.0f;
                memcpy(&value, pointer, sizeof(float));
                value_out = value;
                return true;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            {
                uint8_t value = 0;
                memcpy(&value, pointer, sizeof(uint8_t));
                value_out = normalized ? static_cast<float>(value) / 255.0f
                    : static_cast<float>(value);
                return true;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            {
                uint16_t value = 0;
                memcpy(&value, pointer, sizeof(uint16_t));
                value_out = normalized ? static_cast<float>(value) / 65535.0f
                    : static_cast<float>(value);
                return true;
            }
            case TINYGLTF_COMPONENT_TYPE_BYTE:
            {
                int8_t value = 0;
                memcpy(&value, pointer, sizeof(int8_t));
                //归一化时负数下限钳到 −1（−128 / 127 会略小于 −1）
                value_out = normalized
                    ? (std::max)(-1.0f, static_cast<float>(value) / 127.0f)
                    : static_cast<float>(value);
                return true;
            }
            case TINYGLTF_COMPONENT_TYPE_SHORT:
            {
                int16_t value = 0;
                memcpy(&value, pointer, sizeof(int16_t));
                //归一化时负数下限钳到 −1（−32768 / 32767 会略小于 −1）
                value_out = normalized
                    ? (std::max)(-1.0f, static_cast<float>(value) / 32767.0f)
                    : static_cast<float>(value);
                return true;
            }
            default:
                return false;
            }
        }

        //浮点访问器读取（顶点属性）
        bool accessor_float_read(const tinygltf::Model& model, int accessor_index,
            int expected_components, vector<float>& out, string& error)
        {
            //下标非法
            if (accessor_index < 0
                || static_cast<size_t>(accessor_index) >= model.accessors.size())
            {
                error = "访问器下标非法";
                return false;
            }
            const tinygltf::Accessor& accessor = model.accessors[accessor_index];
            //顶点数量超限
            if (accessor.count > gltf_detail::max_attributes)
            {
                error = "访问器元素数量超限";
                return false;
            }
            size_t start = 0;
            size_t stride = 0;
            size_t element_size = 0;
            const unsigned char* buffer = nullptr;
            if (!accessor_range_get(model, accessor, expected_components, start, stride,
                element_size, buffer, error))
                return false;

            const size_t component_size = element_size / static_cast<size_t>(expected_components);
            out.resize(static_cast<size_t>(accessor.count) * static_cast<size_t>(expected_components));
            for (size_t element = 0; element < static_cast<size_t>(accessor.count); ++element)
            {
                const unsigned char* base = buffer + start + stride * element;
                for (int component = 0; component < expected_components; ++component)
                {
                    float value = 0.0f;
                    if (!component_read(base + component_size * static_cast<size_t>(component),
                        accessor.componentType, accessor.normalized, value))
                    {
                        error = "访问器组件读取失败";
                        return false;
                    }
                    out[element * static_cast<size_t>(expected_components)
                        + static_cast<size_t>(component)] = value;
                }
            }
            return true;
        }

        //无符号整数访问器读取（索引）
        bool accessor_index_read(const tinygltf::Model& model, int accessor_index,
            vector<uint32_t>& out, string& error)
        {
            //下标非法
            if (accessor_index < 0
                || static_cast<size_t>(accessor_index) >= model.accessors.size())
            {
                error = "索引访问器下标非法";
                return false;
            }
            const tinygltf::Accessor& accessor = model.accessors[accessor_index];
            //索引数量超限
            if (accessor.count > gltf_detail::max_indices)
            {
                error = "索引数量超限";
                return false;
            }
            //只接受标量
            if (accessor.type != TINYGLTF_TYPE_SCALAR)
            {
                error = "索引访问器必须为标量";
                return false;
            }
            size_t start = 0;
            size_t stride = 0;
            size_t element_size = 0;
            const unsigned char* buffer = nullptr;
            if (!accessor_range_get(model, accessor, 1, start, stride, element_size,
                buffer, error))
                return false;

            out.resize(static_cast<size_t>(accessor.count));
            for (size_t element = 0; element < out.size(); ++element)
            {
                const unsigned char* pointer = buffer + start + stride * element;
                switch (accessor.componentType)
                {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                {
                    uint8_t value = 0;
                    memcpy(&value, pointer, sizeof(uint8_t));
                    out[element] = value;
                    break;
                }
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                {
                    uint16_t value = 0;
                    memcpy(&value, pointer, sizeof(uint16_t));
                    out[element] = value;
                    break;
                }
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                {
                    uint32_t value = 0;
                    memcpy(&value, pointer, sizeof(uint32_t));
                    out[element] = value;
                    break;
                }
                default:
                    error = "索引组件类型不支持";
                    return false;
                }
            }
            return true;
        }

        //三维向量叉积（a × b）
        array<float, 3> vector_cross(const array<float, 3>& a, const array<float, 3>& b)
        {
            return {
                a[1] * b[2] - a[2] * b[1],
                a[2] * b[0] - a[0] * b[2],
                a[0] * b[1] - a[1] * b[0],
            };
        }

        /*
        补全缺失法线（按三角形面积加权）
        与 Mesh_Loader 同口径：叉积模长即两倍面积，累加后归一；
        退化面（零长度）回落 +Z，保证着色器不会拿到零法线。
        */
        void normals_fill(Mesh_Data& data)
        {
            //是否存在缺法线顶点
            bool missing = false;
            for (const Vertex& vertex : data.vertices)
            {
                if (vertex.normal[0] == 0.0f && vertex.normal[1] == 0.0f
                    && vertex.normal[2] == 0.0f)
                {
                    missing = true;
                    break;
                }
            }
            //全部已有法线：不做事
            if (!missing)
                return;
            //逐顶点累加法线（初始为零）
            vector<array<float, 3>> accumulated(data.vertices.size());
            for (size_t index = 0; index + 2 < data.indices.size(); index += 3)
            {
                const Vertex& a = data.vertices[data.indices[index]];
                const Vertex& b = data.vertices[data.indices[index + 1]];
                const Vertex& c = data.vertices[data.indices[index + 2]];
                const array<float, 3> edge1 = { b.position[0] - a.position[0],
                    b.position[1] - a.position[1], b.position[2] - a.position[2] };
                const array<float, 3> edge2 = { c.position[0] - a.position[0],
                    c.position[1] - a.position[1], c.position[2] - a.position[2] };
                const array<float, 3> face = vector_cross(edge1, edge2);
                //三个顶点各累加一次
                for (size_t corner = 0; corner < 3; ++corner)
                {
                    array<float, 3>& target = accumulated[data.indices[index + corner]];
                    target[0] += face[0];
                    target[1] += face[1];
                    target[2] += face[2];
                }
            }
            //写回缺法线的顶点
            for (size_t index = 0; index < data.vertices.size(); ++index)
            {
                Vertex& vertex = data.vertices[index];
                //已有法线：保持
                if (vertex.normal[0] != 0.0f || vertex.normal[1] != 0.0f
                    || vertex.normal[2] != 0.0f)
                    continue;
                const array<float, 3>& sum = accumulated[index];
                const float length = std::sqrt(sum[0] * sum[0] + sum[1] * sum[1]
                    + sum[2] * sum[2]);
                //退化（零长度）：回落 +Z
                if (length <= 1.0e-6f)
                {
                    vertex.normal[0] = 0.0f;
                    vertex.normal[1] = 0.0f;
                    vertex.normal[2] = 1.0f;
                    continue;
                }
                vertex.normal[0] = sum[0] / length;
                vertex.normal[1] = sum[1] / length;
                vertex.normal[2] = sum[2] / length;
            }
        }

        /*
        节点局部变换合成
        matrix 存在时原样拷贝（列主序）；否则按 T * R * S 合成：
        旋转先按四元数展开，再按列乘缩放，最后写入平移。
        */
        void node_matrix_build(const tinygltf::Node& node, float (&matrix)[16])
        {
            //平移
            double translation[3] = { 0.0, 0.0, 0.0 };
            if (node.translation.size() == 3)
                for (size_t index = 0; index < 3; ++index)
                    translation[index] = node.translation[index];
            //旋转（四元数 x / y / z / w）
            double x = 0.0;
            double y = 0.0;
            double z = 0.0;
            double w = 1.0;
            if (node.rotation.size() == 4)
            {
                x = node.rotation[0];
                y = node.rotation[1];
                z = node.rotation[2];
                w = node.rotation[3];
            }
            //缩放
            double scale[3] = { 1.0, 1.0, 1.0 };
            if (node.scale.size() == 3)
                for (size_t index = 0; index < 3; ++index)
                    scale[index] = node.scale[index];

            //显式矩阵优先
            if (node.matrix.size() == 16)
            {
                for (size_t index = 0; index < 16; ++index)
                    matrix[index] = static_cast<float>(node.matrix[index]);
                return;
            }

            //四元数展开为行主序旋转矩阵
            const double xx = x * x;
            const double yy = y * y;
            const double zz = z * z;
            const double xy = x * y;
            const double xz = x * z;
            const double yz = y * z;
            const double xw = x * w;
            const double yw = y * w;
            const double zw = z * w;
            const double r0 = 1.0 - 2.0 * (yy + zz);
            const double r1 = 2.0 * (xy - zw);
            const double r2 = 2.0 * (xz + yw);
            const double r3 = 2.0 * (xy + zw);
            const double r4 = 1.0 - 2.0 * (xx + zz);
            const double r5 = 2.0 * (yz - xw);
            const double r6 = 2.0 * (xz - yw);
            const double r7 = 2.0 * (yz + xw);
            const double r8 = 1.0 - 2.0 * (xx + yy);

            //列主序写入：旋转列乘缩放，末列写平移
            matrix[0] = static_cast<float>(r0 * scale[0]);
            matrix[1] = static_cast<float>(r3 * scale[0]);
            matrix[2] = static_cast<float>(r6 * scale[0]);
            matrix[3] = 0.0f;
            matrix[4] = static_cast<float>(r1 * scale[1]);
            matrix[5] = static_cast<float>(r4 * scale[1]);
            matrix[6] = static_cast<float>(r7 * scale[1]);
            matrix[7] = 0.0f;
            matrix[8] = static_cast<float>(r2 * scale[2]);
            matrix[9] = static_cast<float>(r5 * scale[2]);
            matrix[10] = static_cast<float>(r8 * scale[2]);
            matrix[11] = 0.0f;
            matrix[12] = static_cast<float>(translation[0]);
            matrix[13] = static_cast<float>(translation[1]);
            matrix[14] = static_cast<float>(translation[2]);
            matrix[15] = 1.0f;
        }

        //单个图元转换（三角面）
        Primitive_Result primitive_convert(const tinygltf::Model& model,
            const tinygltf::Primitive& primitive, Gltf_Primitive& out, string& error)
        {
            //仅支持三角面
            if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
            {
                logger.warn("Gltf_Loader::跳过非三角面图元（mode={}）", primitive.mode);
                return Primitive_Result::skip;
            }
            //缺少位置属性
            auto position_found = primitive.attributes.find("POSITION");
            if (position_found == primitive.attributes.end())
            {
                logger.warn("Gltf_Loader::跳过缺少 POSITION 的图元");
                return Primitive_Result::skip;
            }

            //位置
            vector<float> positions;
            if (!accessor_float_read(model, position_found->second, 3, positions, error))
                return Primitive_Result::fail;
            const size_t vertex_count = positions.size() / 3u;
            //空图元
            if (vertex_count == 0)
            {
                logger.warn("Gltf_Loader::跳过顶点数为零的图元");
                return Primitive_Result::skip;
            }
            //顶点数量超限
            if (vertex_count > gltf_detail::max_attributes)
            {
                error = "图元顶点数量超限";
                return Primitive_Result::fail;
            }

            //纹理坐标（可缺）
            vector<float> texcoords;
            auto texcoord_found = primitive.attributes.find("TEXCOORD_0");
            if (texcoord_found != primitive.attributes.end())
            {
                if (!accessor_float_read(model, texcoord_found->second, 2, texcoords, error))
                    return Primitive_Result::fail;
                //分量数与顶点数不符
                if (texcoords.size() / 2u != vertex_count)
                {
                    error = "纹理坐标数量与顶点数不符";
                    return Primitive_Result::fail;
                }
            }

            //法线（可缺，缺时统一补齐）
            vector<float> normals;
            auto normal_found = primitive.attributes.find("NORMAL");
            if (normal_found != primitive.attributes.end())
            {
                if (!accessor_float_read(model, normal_found->second, 3, normals, error))
                    return Primitive_Result::fail;
                //分量数与顶点数不符
                if (normals.size() / 3u != vertex_count)
                {
                    error = "法线数量与顶点数不符";
                    return Primitive_Result::fail;
                }
            }

            //索引（缺省时自动生成）
            vector<uint32_t> indices;
            if (primitive.indices >= 0)
            {
                if (!accessor_index_read(model, primitive.indices, indices, error))
                    return Primitive_Result::fail;
            }
            else
            {
                indices.resize(vertex_count);
                for (size_t index = 0; index < vertex_count; ++index)
                    indices[index] = static_cast<uint32_t>(index);
            }
            //索引数量超限或不是完整三角形
            if (indices.size() > gltf_detail::max_indices)
            {
                error = "图元索引数量超限";
                return Primitive_Result::fail;
            }
            if (indices.size() % 3u != 0)
            {
                error = "图元索引数不是三的倍数";
                return Primitive_Result::fail;
            }

            //逐顶点装配
            Mesh_Data data;
            data.vertices.resize(vertex_count);
            for (size_t index = 0; index < vertex_count; ++index)
                {
                Vertex& vertex = data.vertices[index];
                vertex.position[0] = positions[index * 3u];
                vertex.position[1] = positions[index * 3u + 1u];
                vertex.position[2] = positions[index * 3u + 2u];
                //纹理坐标缺省为 (0,0)
                if (!texcoords.empty())
                {
                    vertex.uv[0] = texcoords[index * 2u];
                    vertex.uv[1] = texcoords[index * 2u + 1u];
                }
                //法线缺省为零向量（后续统一补齐）
                if (!normals.empty())
                {
                    vertex.normal[0] = normals[index * 3u];
                    vertex.normal[1] = normals[index * 3u + 1u];
                    vertex.normal[2] = normals[index * 3u + 2u];
                }
            }

            //索引越界
            for (const uint32_t index : indices)
            {
                if (index >= vertex_count)
                {
                    error = "图元索引越界";
                    return Primitive_Result::fail;
                }
            }
            data.indices = std::move(indices);

            //补全缺失法线
            normals_fill(data);

            out.data = std::move(data);
            out.material = primitive.material;
            return Primitive_Result::ok;
        }

        //alphaMode 文本 → 枚举（0 不透明 / 1 遮罩 / 2 混合）
        int alpha_mode_map(const string& mode)
        {
            if (mode == "MASK")
                return 1;
            if (mode == "BLEND")
                return 2;
            return 0;
        }
    }

    //第三方模型 → 引擎数据
    bool gltf_detail::model_convert(const tinygltf::Model& model, Gltf_Asset& receiver)
    {
        receiver = Gltf_Asset{};
        //集合数量超限
        if (model.nodes.size() > gltf_detail::max_objects
            || model.meshes.size() > gltf_detail::max_objects
            || model.materials.size() > gltf_detail::max_objects
            || model.textures.size() > gltf_detail::max_objects
            || model.images.size() > gltf_detail::max_objects
            || model.samplers.size() > gltf_detail::max_objects
            || model.scenes.size() > gltf_detail::max_objects)
        {
            logger.warn("Gltf_Loader::集合数量超限");
            return false;
        }

        // ---------- 采样器 ----------
        receiver.samplers.resize(model.samplers.size());
        for (size_t index = 0; index < model.samplers.size(); ++index)
        {
            const tinygltf::Sampler& sampler = model.samplers[index];
            Gltf_Sampler& target = receiver.samplers[index];
            target.mag_filter = sampler.magFilter;
            target.min_filter = sampler.minFilter;
            target.wrap_s = sampler.wrapS;
            target.wrap_t = sampler.wrapT;
        }

        // ---------- 图像引用 ----------
        receiver.images.resize(model.images.size());
        for (size_t index = 0; index < model.images.size(); ++index)
        {
            const tinygltf::Image& image = model.images[index];
            Gltf_Image& target = receiver.images[index];
            target.name = image.name;
            target.uri = image.uri;
            target.mime_type = image.mimeType;
            target.buffer_view = image.bufferView;
            //内嵌图像的 bufferView 必须合法
            if (target.buffer_view >= 0
                && static_cast<size_t>(target.buffer_view) >= model.bufferViews.size())
            {
                logger.warn("Gltf_Loader::图像 bufferView 非法");
                return false;
            }
        }

        // ---------- 纹理 ----------
        receiver.textures.resize(model.textures.size());
        for (size_t index = 0; index < model.textures.size(); ++index)
        {
            const tinygltf::Texture& texture = model.textures[index];
            Gltf_Texture& target = receiver.textures[index];
            target.source = texture.source;
            target.sampler = texture.sampler;
            //图像 / 采样器下标必须合法
            if (target.source >= 0
                && static_cast<size_t>(target.source) >= receiver.images.size())
            {
                logger.warn("Gltf_Loader::纹理 source 非法");
                return false;
            }
            if (target.sampler >= 0
                && static_cast<size_t>(target.sampler) >= receiver.samplers.size())
            {
                logger.warn("Gltf_Loader::纹理 sampler 非法");
                return false;
            }
        }

        // ---------- 材质 ----------
        receiver.materials.resize(model.materials.size());
        for (size_t index = 0; index < model.materials.size(); ++index)
        {
            const tinygltf::Material& material = model.materials[index];
            Gltf_Material& target = receiver.materials[index];
            target.name = material.name;
            //基础颜色
            if (material.pbrMetallicRoughness.baseColorFactor.size() == 4)
                for (size_t component = 0; component < 4; ++component)
                    target.base_color[component] = static_cast<float>(
                        material.pbrMetallicRoughness.baseColorFactor[component]);
            //金属度 / 粗糙度
            target.metallic = static_cast<float>(
                material.pbrMetallicRoughness.metallicFactor);
            target.roughness = static_cast<float>(
                material.pbrMetallicRoughness.roughnessFactor);
            //自发光
            if (material.emissiveFactor.size() == 3)
                for (size_t component = 0; component < 3; ++component)
                    target.emissive[component] = static_cast<float>(
                        material.emissiveFactor[component]);
            //透明模式
            target.alpha_mode = alpha_mode_map(material.alphaMode);
            target.alpha_cutoff = static_cast<float>(material.alphaCutoff);
            target.double_sided = material.doubleSided;
            //贴图引用与强度
            target.base_color_texture = material.pbrMetallicRoughness.baseColorTexture.index;
            target.metallic_roughness_texture =
                material.pbrMetallicRoughness.metallicRoughnessTexture.index;
            target.normal_texture = material.normalTexture.index;
            target.normal_scale = static_cast<float>(material.normalTexture.scale);
            target.occlusion_texture = material.occlusionTexture.index;
            target.occlusion_strength = static_cast<float>(
                material.occlusionTexture.strength);
            target.emissive_texture = material.emissiveTexture.index;
            //贴图下标必须合法
            const int texture_indices[5] = { target.base_color_texture,
                target.metallic_roughness_texture, target.normal_texture,
                target.occlusion_texture, target.emissive_texture };
            for (const int texture_index : texture_indices)
            {
                if (texture_index >= 0
                    && static_cast<size_t>(texture_index) >= receiver.textures.size())
                {
                    logger.warn("Gltf_Loader::材质贴图下标非法");
                    return false;
                }
            }
        }

        // ---------- 网格 ----------
        receiver.meshes.resize(model.meshes.size());
        for (size_t index = 0; index < model.meshes.size(); ++index)
        {
            const tinygltf::Mesh& mesh = model.meshes[index];
            Gltf_Mesh& target = receiver.meshes[index];
            target.name = mesh.name;
            target.primitives.reserve(mesh.primitives.size());
            for (const tinygltf::Primitive& primitive : mesh.primitives)
            {
                //材质下标必须合法
                if (primitive.material >= 0
                    && static_cast<size_t>(primitive.material) >= receiver.materials.size())
                {
                    logger.warn("Gltf_Loader::图元材质下标非法");
                    return false;
                }
                Gltf_Primitive converted;
                string error;
                const Primitive_Result result = primitive_convert(model, primitive,
                    converted, error);
                //数据非法：整份失败
                if (result == Primitive_Result::fail)
                {
                    logger.warn("Gltf_Loader::图元数据非法：{}", error);
                    return false;
                }
                //不支持的图元：跳过
                if (result == Primitive_Result::skip)
                    continue;
                target.primitives.push_back(std::move(converted));
            }
        }

        // ---------- 节点 ----------
        receiver.nodes.resize(model.nodes.size());
        for (size_t index = 0; index < model.nodes.size(); ++index)
        {
            const tinygltf::Node& node = model.nodes[index];
            Gltf_Node& target = receiver.nodes[index];
            target.name = node.name;
            //网格引用
            target.mesh = node.mesh;
            if (target.mesh >= 0
                && static_cast<size_t>(target.mesh) >= receiver.meshes.size())
            {
                logger.warn("Gltf_Loader::节点网格下标非法");
                return false;
            }
            //子节点引用
            target.children = node.children;
            for (const int child : target.children)
            {
                if (child < 0 || static_cast<size_t>(child) >= receiver.nodes.size())
                {
                    logger.warn("Gltf_Loader::节点子下标非法");
                    return false;
                }
            }
            //变换数组长度必须合法
            if ((node.matrix.size() != 0 && node.matrix.size() != 16)
                || (node.translation.size() != 0 && node.translation.size() != 3)
                || (node.rotation.size() != 0 && node.rotation.size() != 4)
                || (node.scale.size() != 0 && node.scale.size() != 3))
            {
                logger.warn("Gltf_Loader::节点变换数组长度非法");
                return false;
            }
            node_matrix_build(node, target.matrix);
        }
        //父节点回填（同一节点只允许一个父节点）
        for (size_t parent = 0; parent < receiver.nodes.size(); ++parent)
        {
            for (const int child : receiver.nodes[parent].children)
            {
                //自引用或重复父节点
                if (static_cast<size_t>(child) == parent
                    || receiver.nodes[child].parent != -1)
                {
                    logger.warn("Gltf_Loader::节点层级非法（自引用或重复父节点）");
                    return false;
                }
                receiver.nodes[child].parent = static_cast<int>(parent);
            }
        }

        // ---------- 场景 ----------
        receiver.scenes.resize(model.scenes.size());
        for (size_t index = 0; index < model.scenes.size(); ++index)
        {
            const tinygltf::Scene& scene = model.scenes[index];
            Gltf_Scene& target = receiver.scenes[index];
            target.name = scene.name;
            target.nodes = scene.nodes;
            for (const int node : target.nodes)
            {
                if (node < 0 || static_cast<size_t>(node) >= receiver.nodes.size())
                {
                    logger.warn("Gltf_Loader::场景根节点下标非法");
                    return false;
                }
            }
        }
        //默认场景
        if (model.defaultScene >= 0
            && static_cast<size_t>(model.defaultScene) >= receiver.scenes.size())
        {
            logger.warn("Gltf_Loader::默认场景下标非法");
            return false;
        }
        receiver.default_scene = model.defaultScene;
        return true;
    }
}
