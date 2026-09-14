//
// vulkan_pipeline.cpp
// As part of the Opal project
// Created by Max Van den Eynde in 2026
// --------------------------------------------------
// Description: Pipeline functions
// Copyright (c) 2026 Max Van den Eynde
//

#include "slang/external/vulkan/include/vulkan/vulkan_core.h"
#ifdef VULKAN
#include "diagnostics.h"
#include "opal/opal.h"
#include "vulkan_state.h"
#include <vulkan/vulkan.h>

namespace opal::vulkan {
VkBlendFactor blenderFuncToVk(BlendFunc func) {
    switch (func) {
    case BlendFunc::Zero:
        return VK_BLEND_FACTOR_ZERO;
    case BlendFunc::One:
        return VK_BLEND_FACTOR_ONE;
    case BlendFunc::SrcColor:
        return VK_BLEND_FACTOR_SRC_COLOR;
    case BlendFunc::OneMinusSrcColor:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case BlendFunc::DstColor:
        return VK_BLEND_FACTOR_DST_COLOR;
    case BlendFunc::OneMinusDstColor:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case BlendFunc::SrcAlpha:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case BlendFunc::OneMinusSrcAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case BlendFunc::DstAlpha:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case BlendFunc::OneMinusDstAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    default:
        return VK_BLEND_FACTOR_ONE;
    }
}

VkBlendOp blenderOpToVk(BlendEquation op) {
    switch (op) {
    case BlendEquation::Add:
        return VK_BLEND_OP_ADD;
    case BlendEquation::Subtract:
        return VK_BLEND_OP_SUBTRACT;
    case BlendEquation::ReverseSubtract:
        return VK_BLEND_OP_REVERSE_SUBTRACT;
    case BlendEquation::Min:
        return VK_BLEND_OP_MIN;
    case BlendEquation::Max:
        return VK_BLEND_OP_MAX;
    default:
        return VK_BLEND_OP_ADD;
    }
}

VkCompareOp compareOpToVk(CompareOp op) {
    switch (op) {
    case CompareOp::Never:
        return VK_COMPARE_OP_NEVER;
    case CompareOp::Less:
        return VK_COMPARE_OP_LESS;
    case CompareOp::Equal:
        return VK_COMPARE_OP_EQUAL;
    case CompareOp::LessEqual:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case CompareOp::Greater:
        return VK_COMPARE_OP_GREATER;
    case CompareOp::NotEqual:
        return VK_COMPARE_OP_NOT_EQUAL;
    case CompareOp::GreaterEqual:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case CompareOp::Always:
        return VK_COMPARE_OP_ALWAYS;
    default:
        return VK_COMPARE_OP_LESS;
    }
}

VkPrimitiveTopology primitiveStyleToVk(PrimitiveStyle style) {
    switch (style) {
    case PrimitiveStyle::Points:
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case PrimitiveStyle::Lines:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case PrimitiveStyle::LineStrip:
        return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case PrimitiveStyle::Triangles:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case PrimitiveStyle::TriangleStrip:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    default:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}

VkPolygonMode rasterizerModeToVk(RasterizerMode mode) {
    switch (mode) {
    case RasterizerMode::Fill:
        return VK_POLYGON_MODE_FILL;
    case RasterizerMode::Line:
        return VK_POLYGON_MODE_LINE;
    case RasterizerMode::Point:
        return VK_POLYGON_MODE_POINT;
    default:
        return VK_POLYGON_MODE_FILL;
    }
}

VkCullModeFlags cullModeToVk(CullMode mode) {
    switch (mode) {
    case CullMode::None:
        return VK_CULL_MODE_NONE;
    case CullMode::Front:
        return VK_CULL_MODE_FRONT_BIT;
    case CullMode::Back:
        return VK_CULL_MODE_BACK_BIT;
    case CullMode::FrontAndBack:
        return VK_CULL_MODE_FRONT_AND_BACK;
    default:
        return VK_CULL_MODE_BACK_BIT;
    }
}

VkFrontFace frontFaceToVk(FrontFace face) {
    switch (face) {
    case FrontFace::Clockwise:
        return VK_FRONT_FACE_CLOCKWISE;
    case FrontFace::CounterClockwise:
        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    default:
        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
}

VkFormat vertexAttributeFormatToVk(VertexAttributeType type, uint size,
                                   bool normalized) {
    switch (type) {
    case VertexAttributeType::Float:
        switch (size) {
        case 1:
            return VK_FORMAT_R32_SFLOAT;
        case 2:
            return VK_FORMAT_R32G32_SFLOAT;
        case 3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case 4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        default:
            throw std::runtime_error(
                "Unsupported vertex attribute size for float type");
        }
    case VertexAttributeType::Int:
        switch (size) {
        case 1:
            return VK_FORMAT_R32_SINT;
        case 2:
            return VK_FORMAT_R32G32_SINT;
        case 3:
            return VK_FORMAT_R32G32B32_SINT;
        case 4:
            return VK_FORMAT_R32G32B32A32_SINT;
        default:
            throw std::runtime_error(
                "Unsupported vertex attribute size for int type");
        }
    case VertexAttributeType::UnsignedInt:
        switch (size) {
        case 1:
            return VK_FORMAT_R32_UINT;
        case 2:
            return VK_FORMAT_R32G32_UINT;
        case 3:
            return VK_FORMAT_R32G32B32_UINT;
        case 4:
            return VK_FORMAT_R32G32B32A32_UINT;
        default:
            throw std::runtime_error(
                "Unsupported vertex attribute size for unsigned int type");
        }
    case VertexAttributeType::Short:
        if (normalized) {
            switch (size) {
            case 2:
                return VK_FORMAT_R16G16_SNORM;
            case 4:
                return VK_FORMAT_R16G16B16A16_SNORM;
            default:
                throw std::runtime_error("Unsupported vertex attribute size "
                                         "for normalized short type");
            }
        } else {
            switch (size) {
            case 2:
                return VK_FORMAT_R16G16_SINT;
            case 4:
                return VK_FORMAT_R16G16B16A16_SINT;
            default:
                throw std::runtime_error(
                    "Unsupported vertex attribute size for short type");
            }
        }
    case VertexAttributeType::UnsignedShort:
        if (normalized) {
            switch (size) {
            case 2:
                return VK_FORMAT_R16G16_UNORM;
            case 4:
                return VK_FORMAT_R16G16B16A16_UNORM;
            default:
                throw std::runtime_error("Unsupported vertex attribute size "
                                         "for normalized unsigned short type");
            }
        } else {
            switch (size) {
            case 2:
                return VK_FORMAT_R16G16_UINT;
            case 4:
                return VK_FORMAT_R16G16B16A16_UINT;
            default:
                throw std::runtime_error("Unsupported vertex attribute size "
                                         "for unsigned short type");
            }
        }
    case VertexAttributeType::Byte:
        if (normalized) {
            switch (size) {
            case 4:
                return VK_FORMAT_R8G8B8A8_SNORM;
            default:
                throw std::runtime_error("Unsupported vertex attribute size "
                                         "for normalized byte type");
            }
        } else {
            switch (size) {
            case 4:
                return VK_FORMAT_R8G8B8A8_SINT;
            default:
                throw std::runtime_error(
                    "Unsupported vertex attribute size for byte type");
            }
        }
    case VertexAttributeType::UnsignedByte:
        if (normalized) {
            switch (size) {
            case 4:
                return VK_FORMAT_R8G8B8A8_UNORM;
            default:
                throw std::runtime_error("Unsupported vertex attribute size "
                                         "for normalized unsigned byte type");
            }
        } else {
            switch (size) {
            case 4:
                return VK_FORMAT_R8G8B8A8_UINT;
            default:
                throw std::runtime_error(
                    "Unsupported vertex attribute size for unsigned byte type");
            }
        }
    case VertexAttributeType::Double:
        switch (size) {
        case 1:
            return VK_FORMAT_R64_SFLOAT;
        case 2:
            return VK_FORMAT_R64G64_SFLOAT;
        case 3:
            return VK_FORMAT_R64G64B64_SFLOAT;
        case 4:
            return VK_FORMAT_R64G64B64A64_SFLOAT;
        default:
            throw std::runtime_error(
                "Unsupported vertex attribute size for double type");
        }
    default:
        throw std::runtime_error("Unsupported vertex attribute type");
    }
}

VkVertexInputRate vertexBindingInputRateToVk(VertexBindingInputRate inputRate) {
    switch (inputRate) {
    case VertexBindingInputRate::Vertex:
        return VK_VERTEX_INPUT_RATE_VERTEX;
    case VertexBindingInputRate::Instance:
        return VK_VERTEX_INPUT_RATE_INSTANCE;
    default:
        return VK_VERTEX_INPUT_RATE_VERTEX;
    }
}

} // namespace opal::vulkan
#endif