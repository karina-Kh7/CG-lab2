#include "application.hpp"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace application {
    namespace {

        // 1. Простая математика для матриц 4x4
        struct Mat4 {
            float m[16]{};
        };
        // Создаёт единичную матрицу 4x4 — основу для остальных преобразований.
        Mat4 identity() {
            Mat4 result{};
            for (int i = 0; i < 4; ++i)
                result.m[i * 5] = 1.0f;
            return result;
        }
        // Перемножает две матрицы 4x4, чтобы объединять несколько преобразований в одно.
        Mat4 multiply(const Mat4& a, const Mat4& b) {
            Mat4 result{};
            for (int column = 0; column < 4; ++column)
                for (int row = 0; row < 4; ++row)
                    for (int k = 0; k < 4; ++k)
                        result.m[column * 4 + row] +=
                        a.m[k * 4 + row] * b.m[column * 4 + k];
            return result;
        }
        // Создаёт матрицу переноса объекта на заданное смещение по X, Y и Z.
        Mat4 translate(float x, float y, float z) {
            Mat4 result = identity();
            result.m[12] = x;
            result.m[13] = y;
            result.m[14] = z;
            return result;
        }
        // Создаёт матрицу масштабирования по трём осям.
        Mat4 scale(float x, float y, float z) {
            Mat4 result = identity();
            result.m[0] = x;
            result.m[5] = y;
            result.m[10] = z;
            return result;
        }
        // Создаёт матрицу поворота вокруг оси X.
        Mat4 rotateX(float angle) {
            Mat4 result = identity();
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            result.m[5] = c;
            result.m[6] = s;
            result.m[9] = -s;
            result.m[10] = c;
            return result;
        }
        // Создаёт матрицу поворота вокруг оси Y.
        Mat4 rotateY(float angle) {
            Mat4 result = identity();
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            result.m[0] = c;
            result.m[2] = -s;
            result.m[8] = s;
            result.m[10] = c;
            return result;
        }
        // Создаёт матрицу поворота вокруг оси Z.
        Mat4 rotateZ(float angle) {
            Mat4 result = identity();
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            result.m[0] = c;
            result.m[1] = s;
            result.m[4] = -s;
            result.m[5] = c;
            return result;
        }
        constexpr float pi = 3.14159265358979323846f;
        // Переводит угол из градусов в радианы, так как sin и cos работают с радианами.
        float radians(float degrees) {
            return degrees * pi / 180.0f;
        }
        // Создаёт матрицу перспективной проекции: дальние части объекта выглядят меньше.
        Mat4 perspective(float aspect, float fov) {
            constexpr float nearPlane = 0.1f;
            constexpr float farPlane = 100.0f;
            const float f = 1.0f / std::tan(radians(fov) / 2.0f);
            Mat4 result{};
            result.m[0] = f / aspect;
            result.m[5] = f;
            result.m[10] = farPlane / (nearPlane - farPlane);
            result.m[11] = -1.0f;
            result.m[14] = (farPlane * nearPlane) / (nearPlane - farPlane);
            return result;
        }
        // Создаёт ортографическую проекцию: размер объекта не зависит от расстояния до камеры.
        Mat4 orthographic(float aspect) {
            const float height = 4.0f;
            const float width = height * aspect;
            const float nearPlane = 0.1f;
            const float farPlane = 100.0f;
            Mat4 result = identity();
            result.m[0] = 2.0f / width;
            result.m[5] = 2.0f / height;
            result.m[10] = 1.0f / (nearPlane - farPlane);
            result.m[14] = nearPlane / (nearPlane - farPlane);
            return result;
        }

        // 2. Икосаэдр: нормали граней генерируются из векторного произведения.
        struct Vertex { float position[3]; float color[3]; float normal[3]; };
        constexpr float phi = 1.61803398875f;
        constexpr float positions[12][3] = {
            { 0,1,phi },{ 0,-1,phi },{ 0,1,-phi },{ 0,-1,-phi },
            { 1,phi,0 },{ -1,phi,0 },{ 1,-phi,0 },{ -1,-phi,0 },
            { phi,0,1 },{ phi,0,-1 },{ -phi,0,1 },{ -phi,0,-1 }
        };
        constexpr uint16_t faces[60] = {
            0,1,8, 0,10,1, 0,4,5, 0,8,4, 0,5,10,
            1,10,7, 1,7,6, 1,6,8,
            2,3,11, 2,9,3, 2,5,4, 2,11,5, 2,4,9,
            3,6,7, 3,9,6, 3,7,11,
            4,8,9, 5,11,10, 6,9,8, 7,10,11
        };
        // Дублируем вершины по граням, чтобы у каждой плоской грани была своя нормаль.
        const std::array<Vertex, 60> vertices = [] {
            std::array<Vertex, 60> result{};
            for (int f = 0; f < 20; ++f) {
                const auto& a = positions[faces[3 * f]];
                const auto& b = positions[faces[3 * f + 1]];
                const auto& c = positions[faces[3 * f + 2]];
                const float ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
                const float vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
                float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
                // Всегда направляем нормаль наружу икосаэдра.
                if (nx * (a[0] + b[0] + c[0]) + ny * (a[1] + b[1] + c[1]) + nz * (a[2] + b[2] + c[2]) < 0) {
                    nx = -nx; ny = -ny; nz = -nz;
                }
                const float inv = 1.0f / std::sqrt(nx * nx + ny * ny + nz * nz);
                for (int j = 0;j < 3;++j) {
                    const auto& pos = positions[faces[3 * f + j]];
                    auto& v = result[3 * f + j];
                    for (int k = 0;k < 3;++k) {
                        v.position[k] = pos[k];
                        v.color[k] = 0.25f + 0.75f * (pos[k] + phi) / (2.0f * phi);
                    }
                    v.normal[0] = nx * inv; v.normal[1] = ny * inv; v.normal[2] = nz * inv;
                }
            }
            return result;
            }();
        const std::array<uint16_t, 60> indices = [] {
            std::array<uint16_t, 60> result{};
            for (uint16_t i = 0;i < 60;++i) result[i] = i;
            return result;
            }();

        // 3. Vulkan-данные
        // std140/std430: vec4 обеспечивает одинаковое выравнивание в C++ и GLSL.
        struct alignas(16) SceneUniform {
            Mat4 viewProjection;
            float cameraPosition[4];
            float directionalDirection[4];
            float directionalColorIntensity[4];
            float ambientColorIntensity[4];
            int lightCounts[4];
        };
        struct alignas(16) ModelUniform {
            Mat4 model;
            float albedo[4];
            float specular[4]; // xyz - цвет блика, w - степень блеска
            float normalMatrix[16]; // inverse(transpose(model)): масштаб корректно учитывается
        };
        struct alignas(16) PointLight {
            float position[4];
            float colorIntensity[4];
        };
        struct alignas(16) SpotLight {
            float position[4];
            float colorIntensity[4];
            float directionInnerCos[4];
            float outerCosPadding[4];
        };
        struct Buffer { VkBuffer handle = VK_NULL_HANDLE; VmaAllocation allocation = VK_NULL_HANDLE; };
        constexpr size_t maxLights = 64; // вместимость CPU/GPU буфера, НЕ размер массива в GLSL
        Buffer vertexBuffer, indexBuffer, sceneUniformBuffer, modelUniformBuffer;
        Buffer pointBuffer, spotBuffer;
        VkDescriptorSetLayout sceneLayout = VK_NULL_HANDLE, modelLayout = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet sceneSet = VK_NULL_HANDLE, modelSet = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        std::vector<PointLight> pointLights = {
            { { 2.3f,1.5f,2.0f,0.0f },{ 1.0f,0.35f,0.25f,12.0f } }
        };
        std::vector<SpotLight> spotLights = {
            { { -2.5f,2.5f,2.0f,0.0f },{ 0.30f,0.55f,1.0f,22.0f },
             { 0.5f,-0.7f,-0.5f,16.0f },{ 32.0f,0,0,0 } }
        };
        float cameraPosition[3] = { 0.0f,0.0f,8.0f };
        float cameraRotation[3] = { 0.0f,0.0f,0.0f };
        float cameraFov = 55.0f;
        float directionalDirection[3] = { -0.6f,-1.0f,-0.5f };
        float directionalColor[3] = { 1.0f,1.0f,0.95f };
        float directionalIntensity = 1.6f;
        float ambientColor[3] = { 1.0f,1.0f,1.0f };
        float ambientIntensity = 0.12f;
        float albedo[3] = { 0.85f,0.45f,0.25f };
        float specularColor[3] = { 1.0f,1.0f,1.0f };
        float shininess = 48.0f;

        // 4. Параметры фигуры и анимации
        float position[3] = { 0.0f, 0.0f, 0.0f };
        float rotation[3] = { 18.0f, 25.0f, 0.0f };
        float objectScale[3] = { 1.0f, 1.0f, 1.0f };
        float colorMultiplier[3] = { 1.0f, 1.0f, 1.0f };
        float orbitRadius = 1.1f;
        float animationSpeed = 1.0f;
        float animationPhase = 0.0f;
        bool animationPaused = false;
        bool usePerspective = true;
        double previousTime = -1.0;

        // 5. Работа с буферами
        // Создаёт Vulkan-буфер нужного размера и выделяет для него память через VMA.
        bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, Buffer& target) {
            auto& context = graphics::internal::context;
            const VkBufferCreateInfo bufferInfo{
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = size,
                .usage = usage,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE
            };
            const VmaAllocationCreateInfo allocationInfo{
                .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                .usage = VMA_MEMORY_USAGE_AUTO
            };
            return vmaCreateBuffer(
                context.allocator, &bufferInfo, &allocationInfo,
                &target.handle, &target.allocation, nullptr
            ) == VK_SUCCESS;
        }
        // Освобождает Vulkan-буфер и связанную с ним память.
        void destroyBuffer(Buffer& buffer) {
            if (buffer.handle != VK_NULL_HANDLE) {
                vmaDestroyBuffer(
                    graphics::internal::context.allocator,
                    buffer.handle,
                    buffer.allocation
                );
            }
            buffer = {};
        }
        // Копирует данные из обычной памяти CPU в память Vulkan-буфера.
        bool writeBuffer(const Buffer& buffer, const void* data, size_t size) {
            auto allocator = graphics::internal::context.allocator;
            void* mappedMemory = nullptr;
            if (vmaMapMemory(allocator, buffer.allocation, &mappedMemory) != VK_SUCCESS)
                return false;
            std::memcpy(mappedMemory, data, size);
            vmaFlushAllocation(allocator, buffer.allocation, 0, size);
            vmaUnmapMemory(allocator, buffer.allocation);
            return true;
        }

        // 6. Загрузка шейдеров
        // Загружает с диска уже скомпилированный SPIR-V shader-файл.
        std::vector<char> loadShaderFile(const char* path) {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file)
                throw std::runtime_error(std::string("Cannot open shader: ") + path);
            const auto fileSize = file.tellg();
            if (fileSize <= 0 || fileSize % 4 != 0)
                throw std::runtime_error("Invalid SPIR-V file");
            std::vector<char> bytes(static_cast<size_t>(fileSize));
            file.seekg(0);
            file.read(bytes.data(), fileSize);
            if (!file)
                throw std::runtime_error("Cannot read SPIR-V file");
            return bytes;
        }
        // Создаёт Vulkan shader module из загруженного SPIR-V-кода.
        VkShaderModule createShader(const char* path) {
            auto& context = graphics::internal::context;
            const auto code = loadShaderFile(path);
            const VkShaderModuleCreateInfo shaderInfo{
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = code.size(),
                .pCode = reinterpret_cast<const uint32_t*>(code.data())
            };
            VkShaderModule shaderModule = VK_NULL_HANDLE;
            if (vkCreateShaderModule(context.device, &shaderInfo, nullptr, &shaderModule) != VK_SUCCESS)
                throw std::runtime_error(std::string("Cannot create shader: ") + path);
            return shaderModule;
        }

        // 7. Graphics pipeline
        // Настраивает graphics pipeline: шейдеры, вершины, треугольники, depth test и другие этапы отрисовки.
        bool createPipeline() {
            auto& context = graphics::internal::context;
            VkShaderModule vertexShader = VK_NULL_HANDLE;
            VkShaderModule fragmentShader = VK_NULL_HANDLE;
            try {
                vertexShader = createShader("shaders/icosahedron.vert.spv");
                fragmentShader = createShader("shaders/icosahedron.frag.spv");
            }
            catch (const std::exception& error) {
                std::cerr << error.what() << '\n';
                if (vertexShader)
                    vkDestroyShaderModule(context.device, vertexShader, nullptr);
                return false;
            }
            const VkPipelineShaderStageCreateInfo shaderStages[] = {
                {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_VERTEX_BIT,
                    .module = vertexShader,
                    .pName = "main"
                },
                {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                    .module = fragmentShader,
                    .pName = "main"
                }
            };
            // Описываем, как GPU должен читать одну структуру Vertex из vertex buffer.
            const VkVertexInputBindingDescription vertexBinding{
                .binding = 0,
                .stride = sizeof(Vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            };
            // Связываем position и color из Vertex с location 0 и 1 в vertex shader.
            const VkVertexInputAttributeDescription vertexAttributes[] = {
                { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, position)) },
                { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, color)) },
                { 2, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, normal)) }
            };
            const VkPipelineVertexInputStateCreateInfo vertexInput{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = 1,
                .pVertexBindingDescriptions = &vertexBinding,
                .vertexAttributeDescriptionCount = 3,
                .pVertexAttributeDescriptions = vertexAttributes
            };
            // Каждые три индекса образуют отдельный треугольник.
            const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
            };
            const VkPipelineViewportStateCreateInfo viewportState{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .viewportCount = 1,
                .scissorCount = 1
            };
            // Растеризатор превращает треугольники в набор фрагментов для последующей закраски.
            const VkPipelineRasterizationStateCreateInfo rasterizer{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_NONE,
                .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .lineWidth = 1.0f
            };
            const VkPipelineMultisampleStateCreateInfo multisampling{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
            };
            // Depth test оставляет видимыми ближайшие к камере поверхности.
            const VkPipelineDepthStencilStateCreateInfo depthStencil{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                .depthTestEnable = VK_TRUE,
                .depthWriteEnable = VK_TRUE,
                .depthCompareOp = VK_COMPARE_OP_LESS
            };
            const VkPipelineColorBlendAttachmentState blendAttachment{
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
            };
            const VkPipelineColorBlendStateCreateInfo colorBlending{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .attachmentCount = 1,
                .pAttachments = &blendAttachment
            };
            const VkDynamicState dynamicStates[] = {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR
            };
            const VkPipelineDynamicStateCreateInfo dynamicState{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .dynamicStateCount = 2,
                .pDynamicStates = dynamicStates
            };
            const VkGraphicsPipelineCreateInfo pipelineInfo{
                .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .stageCount = 2,
                .pStages = shaderStages,
                .pVertexInputState = &vertexInput,
                .pInputAssemblyState = &inputAssembly,
                .pViewportState = &viewportState,
                .pRasterizationState = &rasterizer,
                .pMultisampleState = &multisampling,
                .pDepthStencilState = &depthStencil,
                .pColorBlendState = &colorBlending,
                .pDynamicState = &dynamicState,
                .layout = pipelineLayout,
                .renderPass = context.render_pass,
                .subpass = 0
            };
            const VkResult result = vkCreateGraphicsPipelines(
                context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline
            );
            vkDestroyShaderModule(context.device, fragmentShader, nullptr);
            vkDestroyShaderModule(context.device, vertexShader, nullptr);
            return result == VK_SUCCESS;
        }
    } // namespace

    // 8. Инициализация Vulkan-буферов и двух наборов дескрипторов.
    bool initialize() {
        auto& c = graphics::internal::context;
        if (!createBuffer(sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer) ||
            !createBuffer(sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer) ||
            !createBuffer(sizeof(SceneUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sceneUniformBuffer) ||
            !createBuffer(sizeof(ModelUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, modelUniformBuffer) ||
            !createBuffer(maxLights * sizeof(PointLight), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, pointBuffer) ||
            !createBuffer(maxLights * sizeof(SpotLight), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, spotBuffer) ||
            !writeBuffer(vertexBuffer, vertices.data(), sizeof(vertices)) ||
            !writeBuffer(indexBuffer, indices.data(), sizeof(indices))) return (shutdown(), false);
        const VkDescriptorSetLayoutBinding sceneBindings[] = {
            { 0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,nullptr },
            { 1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr },
            { 2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr }
        };
        const VkDescriptorSetLayoutBinding modelBinding{
            0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,nullptr
        };
        VkDescriptorSetLayoutCreateInfo desc{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 3,.pBindings = sceneBindings };
        if (vkCreateDescriptorSetLayout(c.device, &desc, nullptr, &sceneLayout) != VK_SUCCESS)return (shutdown(), false);
        desc.bindingCount = 1; desc.pBindings = &modelBinding;
        if (vkCreateDescriptorSetLayout(c.device, &desc, nullptr, &modelLayout) != VK_SUCCESS)return (shutdown(), false);
        const VkDescriptorSetLayout layouts[] = { sceneLayout,modelLayout };
        const VkPipelineLayoutCreateInfo pl{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 2,.pSetLayouts = layouts };
        if (vkCreatePipelineLayout(c.device, &pl, nullptr, &pipelineLayout) != VK_SUCCESS)return (shutdown(), false);
        const VkDescriptorPoolSize sizes[] = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,2 },{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2 }
        };
        const VkDescriptorPoolCreateInfo pool{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 2,.poolSizeCount = 2,.pPoolSizes = sizes };
        if (vkCreateDescriptorPool(c.device, &pool, nullptr, &descriptorPool) != VK_SUCCESS)return (shutdown(), false);
        const VkDescriptorSetAllocateInfo alloc{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = descriptorPool,.descriptorSetCount = 2,.pSetLayouts = layouts };
        VkDescriptorSet sets[2]{};
        if (vkAllocateDescriptorSets(c.device, &alloc, sets) != VK_SUCCESS)return (shutdown(), false);
        sceneSet = sets[0];modelSet = sets[1];
        const VkDescriptorBufferInfo infos[] = {
            { sceneUniformBuffer.handle,0,sizeof(SceneUniform) },
            { pointBuffer.handle,0,maxLights * sizeof(PointLight) },
            { spotBuffer.handle,0,maxLights * sizeof(SpotLight) },
            { modelUniformBuffer.handle,0,sizeof(ModelUniform) }
        };
        VkWriteDescriptorSet writes[4]{};
        for (int i = 0;i < 4;++i) {
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = i == 3 ? modelSet : sceneSet;
            writes[i].dstBinding = i == 3 ? 0 : static_cast<uint32_t>(i);
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = (i == 1 || i == 2) ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[i].pBufferInfo = &infos[i];
        }
        vkUpdateDescriptorSets(c.device, 4, writes, 0, nullptr);
        if (!createPipeline())return (shutdown(), false);
        return true;
    }

    // 9. Освобождение ресурсов.
    void shutdown() {
        auto& c = graphics::internal::context;
        if (c.graphics_queue)vkQueueWaitIdle(c.graphics_queue);
        if (pipeline)vkDestroyPipeline(c.device, pipeline, nullptr);
        if (pipelineLayout)vkDestroyPipelineLayout(c.device, pipelineLayout, nullptr);
        if (descriptorPool)vkDestroyDescriptorPool(c.device, descriptorPool, nullptr);
        if (sceneLayout)vkDestroyDescriptorSetLayout(c.device, sceneLayout, nullptr);
        if (modelLayout)vkDestroyDescriptorSetLayout(c.device, modelLayout, nullptr);
        pipeline = VK_NULL_HANDLE;pipelineLayout = VK_NULL_HANDLE;
        descriptorPool = VK_NULL_HANDLE;sceneLayout = VK_NULL_HANDLE;modelLayout = VK_NULL_HANDLE;
        destroyBuffer(spotBuffer);destroyBuffer(pointBuffer);
        destroyBuffer(modelUniformBuffer);destroyBuffer(sceneUniformBuffer);
        destroyBuffer(indexBuffer);destroyBuffer(vertexBuffer);
    }

    // 10. Управление сценой через ImGui в реальном времени.
    void update(double time) {
        if (previousTime < 0.0)previousTime = time;
        const double delta = std::clamp(time - previousTime, 0.0, 0.1);previousTime = time;
        if (!animationPaused)animationPhase += animationSpeed * static_cast<float>(delta);
        // Делаем панель достаточно широкой, чтобы названия и ползунки не обрезались.
        ImGui::SetNextWindowSize(ImVec2(540.0f, 650.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(450.0f, 340.0f), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::Begin("Lab 2: camera and lighting (variant 11)");
        ImGui::PushItemWidth(-180.0f);
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Camera position", cameraPosition, 0.05f);
            ImGui::SliderFloat3("Camera rotation XYZ", cameraRotation, -180.0f, 180.0f);
            ImGui::SliderFloat("Field of view", &cameraFov, 20.0f, 110.0f);
            ImGui::Checkbox("Perspective projection", &usePerspective);
        }
        if (ImGui::CollapsingHeader("Object and material", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Object position", position, 0.02f);
            ImGui::DragFloat3("Object rotation", rotation, 0.5f);
            ImGui::DragFloat3("Object scale", objectScale, 0.01f, 0.1f, 4.0f);
            ImGui::ColorEdit3("Vertex color multiplier", colorMultiplier);
            ImGui::ColorEdit3("Albedo", albedo);
            ImGui::ColorEdit3("Specular color", specularColor);
            ImGui::SliderFloat("Shininess", &shininess, 1.0f, 256.0f);
            ImGui::Checkbox("Pause animation", &animationPaused);
            ImGui::SliderFloat("Orbit speed", &animationSpeed, 0.0f, 4.0f);
            ImGui::SliderFloat("Orbit radius", &orbitRadius, 0.0f, 2.0f);
        }
        if (ImGui::CollapsingHeader("Directional and ambient light", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Directional vector", directionalDirection, 0.02f);
            ImGui::ColorEdit3("Directional color", directionalColor);
            ImGui::SliderFloat("Directional intensity", &directionalIntensity, 0.0f, 15.0f);
            ImGui::ColorEdit3("Ambient color", ambientColor);
            ImGui::SliderFloat("Ambient intensity", &ambientIntensity, 0.0f, 1.0f);
        }
        if (ImGui::CollapsingHeader("Point lights", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (pointLights.size() < maxLights && ImGui::Button("Add point light"))
                pointLights.push_back({ { 0,2,2,0 },{ 1,0.8f,0.5f,12 } });
            for (size_t i = 0;i < pointLights.size();) {
                ImGui::PushID(static_cast<int>(i) + 1000);
                ImGui::Separator();ImGui::Text("Point %d", static_cast<int>(i) + 1);
                ImGui::DragFloat3("Position", pointLights[i].position, 0.05f);
                ImGui::ColorEdit3("Color", pointLights[i].colorIntensity);
                ImGui::SliderFloat("Intensity", &pointLights[i].colorIntensity[3], 0.0f, 100.0f);
                bool remove = ImGui::Button("Delete point light");ImGui::PopID();
                if (remove)pointLights.erase(pointLights.begin() + static_cast<std::ptrdiff_t>(i));else ++i;
            }
        }
        if (ImGui::CollapsingHeader("Spot lights", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (spotLights.size() < maxLights && ImGui::Button("Add spot light"))
                spotLights.push_back({ { 0,3,2,0 },{ 0.6f,0.7f,1,20 },{ 0,-1,-0.5f,16 },{ 28,0,0,0 } });
            for (size_t i = 0;i < spotLights.size();) {
                auto& l = spotLights[i];ImGui::PushID(static_cast<int>(i) + 2000);
                ImGui::Separator();ImGui::Text("Spot %d", static_cast<int>(i) + 1);
                ImGui::DragFloat3("Position", l.position, 0.05f);
                ImGui::DragFloat3("Direction", l.directionInnerCos, 0.02f);
                ImGui::ColorEdit3("Color", l.colorIntensity);
                ImGui::SliderFloat("Intensity", &l.colorIntensity[3], 0.0f, 100.0f);
                // Углы храним в градусах для понятного управления.
                ImGui::SliderFloat("Inner angle", &l.directionInnerCos[3], 1.0f, 85.0f);
                ImGui::SliderFloat("Outer angle", &l.outerCosPadding[0], l.directionInnerCos[3], 89.0f);
                l.outerCosPadding[0] = std::max(l.outerCosPadding[0], l.directionInnerCos[3]);
                bool remove = ImGui::Button("Delete spot light");ImGui::PopID();
                if (remove)spotLights.erase(spotLights.begin() + static_cast<std::ptrdiff_t>(i));else ++i;
            }
        }
        ImGui::PopItemWidth();
        ImGui::End();
    }

    // 11. Отрисовка

    // Формирует матрицы текущего кадра и записывает команды отрисовки икосаэдра.
    void render(const graphics::internal::FrameData& fd) {
        auto& context = graphics::internal::context;
        if (!fd.command_buffer || !fd.framebuffer)
            return;
        // Отношение ширины окна к высоте нужно для правильной формы проекции.
        const float aspect =
            static_cast<float>(context.swapchain_extent.width) /
            static_cast<float>(context.swapchain_extent.height);
        // Доп. №1: выбираем тип проекции.
        const Mat4 projection = usePerspective ? perspective(aspect, cameraFov) : orthographic(aspect);
        // View = обратная матрица перемещения и поворота камеры.
        const Mat4 view = multiply(
            multiply(rotateX(radians(-cameraRotation[0])), rotateY(radians(-cameraRotation[1]))),
            multiply(rotateZ(radians(-cameraRotation[2])),
                translate(-cameraPosition[0], -cameraPosition[1], -cameraPosition[2])));
        // Доп. №3: движение по сложной траектории.
        const Mat4 orbit = translate(
            orbitRadius * std::cos(animationPhase),
            0.35f * std::sin(2.0f * animationPhase),
            orbitRadius * std::sin(animationPhase)
        );
        const Mat4 userTranslation = translate(position[0], position[1], position[2]);
        const Mat4 rotationZ = rotateZ(radians(rotation[2] + animationPhase * 30.0f));
        const Mat4 rotationY = rotateY(radians(rotation[1] + animationPhase * 50.0f));
        const Mat4 rotationX = rotateX(radians(rotation[0]));
        const Mat4 scaling = scale(objectScale[0], objectScale[1], objectScale[2]);
        // Model-матрица объединяет масштабирование, поворот, пользовательский перенос и движение по траектории.
        const Mat4 model = multiply(
            multiply(userTranslation, orbit),
            multiply(
                multiply(rotationZ, rotationY),
                multiply(rotationX, scaling)
            )
        );
        SceneUniform scene{};
        scene.viewProjection = multiply(projection, view);
        for (int k = 0;k < 3;++k) {
            scene.cameraPosition[k] = cameraPosition[k];
            scene.directionalDirection[k] = directionalDirection[k];
            scene.directionalColorIntensity[k] = directionalColor[k];
            scene.ambientColorIntensity[k] = ambientColor[k];
        }
        scene.directionalColorIntensity[3] = directionalIntensity;
        scene.ambientColorIntensity[3] = ambientIntensity;
        scene.lightCounts[0] = static_cast<int>(pointLights.size());
        scene.lightCounts[1] = static_cast<int>(spotLights.size());
        ModelUniform material{};material.model = model;
        for (int k = 0;k < 3;++k) {
            material.albedo[k] = albedo[k] * colorMultiplier[k];
            material.specular[k] = specularColor[k];
        }
        material.specular[3] = shininess;
        // Normal matrix = транспонированная обратная от матрицы модели.
        // Для T*R*S нормали преобразуются R*inverse(S); перенос на них не действует.
        const Mat4 rotationOnly = multiply(rotationZ, multiply(rotationY, rotationX));
        const Mat4 normalMatrix = multiply(rotationOnly, scale(
            1.0f / objectScale[0], 1.0f / objectScale[1], 1.0f / objectScale[2]));
        std::memcpy(material.normalMatrix, normalMatrix.m, sizeof(material.normalMatrix));
        // std430 buffer: передаём только активную часть массива, длина в shader не фиксирована.
        std::vector<SpotLight> gpuSpots = spotLights;
        for (auto& l : gpuSpots) {
            l.directionInnerCos[3] = std::cos(radians(l.directionInnerCos[3]));
            l.outerCosPadding[0] = std::cos(radians(l.outerCosPadding[0]));
        }
        if (!writeBuffer(sceneUniformBuffer, &scene, sizeof(scene)) ||
            !writeBuffer(modelUniformBuffer, &material, sizeof(material)) ||
            (!pointLights.empty() && !writeBuffer(pointBuffer, pointLights.data(), pointLights.size() * sizeof(PointLight))) ||
            (!gpuSpots.empty() && !writeBuffer(spotBuffer, gpuSpots.data(), gpuSpots.size() * sizeof(SpotLight))))
            std::cerr << "Vulkan buffer update failed\n";
        // Дальше идёт обычная запись команд Vulkan.
        // Начинаем заново записывать команды для текущего кадра.
        vkResetCommandBuffer(fd.command_buffer, 0);
        const VkCommandBufferBeginInfo commandBufferBegin{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        };
        vkBeginCommandBuffer(fd.command_buffer, &commandBufferBegin);
        const VkClearValue clearValues[] = {
            { .color = { { 0.07f, 0.09f, 0.13f, 1.0f } } },
            { .depthStencil = { 1.0f, 0 } }
        };
        const VkRenderPassBeginInfo renderPassBegin{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = context.render_pass,
            .framebuffer = fd.framebuffer,
            .renderArea = { { 0, 0 }, context.swapchain_extent },
            .clearValueCount = 2,
            .pClearValues = clearValues
        };
        // Начинаем render pass и выбираем созданный graphics pipeline.
        vkCmdBeginRenderPass(fd.command_buffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        // Отрицательная высота переворачивает ось Y для Vulkan.
        const VkViewport viewport{
            0.0f,
            static_cast<float>(context.swapchain_extent.height),
            static_cast<float>(context.swapchain_extent.width),
            -static_cast<float>(context.swapchain_extent.height),
            0.0f,
            1.0f
        };
        const VkRect2D scissor{ { 0, 0 }, context.swapchain_extent };
        vkCmdSetViewport(fd.command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(fd.command_buffer, 0, 1, &scissor);
        const VkDeviceSize vertexOffset = 0;
        // Подключаем буферы геометрии: координаты/цвета вершин и индексы треугольников.
        vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, &vertexBuffer.handle, &vertexOffset);
        vkCmdBindIndexBuffer(fd.command_buffer, indexBuffer.handle, 0, VK_INDEX_TYPE_UINT16);
        // Подключаем descriptor set, через который shader получает MVP-матрицу и цвет.
        vkCmdBindDescriptorSets(
            fd.command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            0,
            1,
            &sceneSet,
            0,
            nullptr
        );
        vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout, 1, 1, &modelSet, 0, nullptr);
        // Запускаем отрисовку 60 индексов, то есть 20 треугольных граней икосаэдра.
        vkCmdDrawIndexed(fd.command_buffer, 60, 1, 0, 0, 0);
        vkCmdEndRenderPass(fd.command_buffer);
        vkEndCommandBuffer(fd.command_buffer);
    }
} // namespace application