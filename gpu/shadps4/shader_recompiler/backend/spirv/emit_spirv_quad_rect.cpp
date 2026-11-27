// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>
#include <string_view>
#include <unordered_map>
#include <sirit/sirit.h>
#include "shader_recompiler/backend/spirv/emit_spirv_quad_rect.h"
#include "shader_recompiler/info.h"
#include "shader_recompiler/runtime_info.h"

namespace Shader::Backend::SPIRV {

using Sirit::Id;

constexpr u32 SPIRV_VERSION_1_5 = 0x00010500;

struct QuadRectListEmitter : public Sirit::Module {
    explicit QuadRectListEmitter(const HwFragmentRuntimeInfo& fs_info_,
                                 const VsOutputs& vs_outputs_)
        : Sirit::Module{SPIRV_VERSION_1_5}, fs_info{fs_info_}, vs_outputs{vs_outputs_} {
        void_id = TypeVoid();
        bool_id = TypeBool();
        float_id = TypeFloat(32);
        uint_id = TypeUInt(32U);
        int_id = TypeInt(32U, true);
        bvec2_id = TypeVector(bool_id, 2);
        vec2_id = TypeVector(float_id, 2);
        vec3_id = TypeVector(float_id, 3);
        vec4_id = TypeVector(float_id, 4);

        float_one = Constant(float_id, 1.0f);
        float_min_one = Constant(float_id, -1.0f);
        float_zero = Constant(float_id, 0.0f);
        int_zero = Constant(int_id, 0);
        vec4_zero = ConstantComposite(vec4_id, float_zero, float_zero, float_zero, float_zero);

        const Id float_arr{TypeArray(float_id, Constant(uint_id, 1U))};
        gl_per_vertex_type = TypeStruct(vec4_id, float_id, float_arr, float_arr);
        Decorate(gl_per_vertex_type, spv::Decoration::Block);
        MemberDecorate(gl_per_vertex_type, 0U, spv::Decoration::BuiltIn,
                       static_cast<u32>(spv::BuiltIn::Position));
        MemberDecorate(gl_per_vertex_type, 1U, spv::Decoration::BuiltIn,
                       static_cast<u32>(spv::BuiltIn::PointSize));
        MemberDecorate(gl_per_vertex_type, 2U, spv::Decoration::BuiltIn,
                       static_cast<u32>(spv::BuiltIn::ClipDistance));
        MemberDecorate(gl_per_vertex_type, 3U, spv::Decoration::BuiltIn,
                       static_cast<u32>(spv::BuiltIn::CullDistance));
    }

    /// Emits tessellation control shader for interpolating the 4th vertex of rectange primitive
    void EmitRectListTCS() {
        DefineEntry(spv::ExecutionModel::TessellationControl);

        // Set passthrough tessellation factors
        const Id output_float_id{TypePointer(spv::StorageClass::Output, float_id)};
        for (int i = 0; i < 4; i++) {
            const Id ptr{OpAccessChain(output_float_id, gl_tess_level_outer, Int(i))};
            OpStore(ptr, float_one);
        }
        for (int i = 0; i < 2; i++) {
            const Id ptr{OpAccessChain(output_float_id, gl_tess_level_inner, Int(i))};
            OpStore(ptr, float_one);
        }

        const Id input_vec4{TypePointer(spv::StorageClass::Input, vec4_id)};
        const Id output_vec4{TypePointer(spv::StorageClass::Output, vec4_id)};

        // Emit interpolation block of the 4th vertex in rect.
        // Load positions
        std::array<Id, 3> pos;
        for (int i = 0; i < 3; i++) {
            pos[i] = OpLoad(vec4_id, OpAccessChain(input_vec4, gl_in, Int(i), int_zero));
        }

        std::array<Id, 3> point_coord_equal;
        for (int i = 0; i < 3; i++) {
            // point_coord_equal[i] = equal(gl_in[i].gl_Position.xy, gl_in[(i + 1) %
            // 3].gl_Position.xy);
            const Id pos_l_xy{OpVectorShuffle(vec2_id, pos[i], pos[i], 0, 1)};
            const Id pos_r_xy{OpVectorShuffle(vec2_id, pos[(i + 1) % 3], pos[(i + 1) % 3], 0, 1)};
            point_coord_equal[i] = OpFOrdEqual(bvec2_id, pos_l_xy, pos_r_xy);
        }

        std::array<Id, 3> bary_coord;
        std::array<Id, 3> is_edge_vertex;
        for (int i = 0; i < 3; i++) {
            // bool xy_equal = point_coord_equal[i].x && point_coord_equal[(i + 2) % 3].y;
            const Id xy_equal{
                OpLogicalAnd(bool_id, OpCompositeExtract(bool_id, point_coord_equal[i], 0),
                             OpCompositeExtract(bool_id, point_coord_equal[(i + 2) % 3], 1))};
            // bool yx_equal = point_coord_equal[i].y && point_coord_equal[(i + 2) % 3].x;
            const Id yx_equal{
                OpLogicalAnd(bool_id, OpCompositeExtract(bool_id, point_coord_equal[i], 1),
                             OpCompositeExtract(bool_id, point_coord_equal[(i + 2) % 3], 0))};
            // bary_coord[i] = (xy_equal || yx_equal) ? -1.f : 1.f;
            is_edge_vertex[i] = OpLogicalOr(bool_id, xy_equal, yx_equal);
            bary_coord[i] = OpSelect(float_id, is_edge_vertex[i], float_min_one, float_one);
        }

        const auto interpolate = [&](Id v0, Id v1, Id v2) {
            // return v0 * bary_coord.x + v1 * bary_coord.y + v2 * bary_coord.z;
            const Id p0{OpVectorTimesScalar(vec4_id, v0, bary_coord[0])};
            const Id p1{OpVectorTimesScalar(vec4_id, v1, bary_coord[1])};
            const Id p2{OpVectorTimesScalar(vec4_id, v2, bary_coord[2])};
            return OpFAdd(vec4_id, p0, OpFAdd(vec4_id, p1, p2));
        };

        // int vertex_index_id = is_edge_vertex[1] ? 1 : (is_edge_vertex[2] ? 2 : 0);
        Id vertex_index{OpSelect(int_id, is_edge_vertex[2], Int(2), Int(0))};
        vertex_index = OpSelect(int_id, is_edge_vertex[1], Int(1), vertex_index);

        // int index = (vertex_index_id + gl_InvocationID) % 3;
        const Id invocation_id{OpLoad(int_id, gl_invocation_id)};
        const Id invocation_3{OpIEqual(bool_id, invocation_id, Int(3))};
        const Id index{OpSMod(int_id, OpIAdd(int_id, vertex_index, invocation_id), Int(3))};

        // gl_out[gl_InvocationID].gl_Position = gl_InvocationID == 3 ? pos3 :
        // gl_in[index].gl_Position;
        const Id pos3{interpolate(pos[0], pos[1], pos[2])};
        const Id in_ptr{OpAccessChain(input_vec4, gl_in, index, Int(0))};
        const Id position{OpSelect(vec4_id, invocation_3, pos3, OpLoad(vec4_id, in_ptr))};
        OpStore(OpAccessChain(output_vec4, gl_out, invocation_id, Int(0)), position);

        // Set attributes
        for (int i = 0; i < inputs.size(); i++) {
            // vec4 in_paramN3 = interpolate(bary_coord, in_paramN[0], in_paramN[1], in_paramN[2]);
            const Id v0{LoadInput(i, Int(0))};
            const Id v1{LoadInput(i, Int(1))};
            const Id v2{LoadInput(i, Int(2))};
            const Id in_param3{interpolate(v0, v1, v2)};
            // out_paramN[gl_InvocationID] = gl_InvocationID == 3 ? in_paramN3 : in_paramN[index];
            const Id in_param{LoadInput(i, index)};
            const Id out_param{OpSelect(vec4_id, invocation_3, in_param3, in_param)};
            OpStore(OpAccessChain(output_vec4, outputs[i], invocation_id), out_param);
        }

        OpReturn();
        OpFunctionEnd();
    }

    /// Emits a passthrough quad tessellation control shader that outputs 4 control points.
    void EmitQuadListTCS() {
        DefineEntry(spv::ExecutionModel::TessellationControl);
        const Id array_type{TypeArray(int_id, Int(4))};
        const Id values{ConstantComposite(array_type, Int(1), Int(2), Int(0), Int(3))};
        const Id indices{AddLocalVariable(TypePointer(spv::StorageClass::Function, array_type),
                                          spv::StorageClass::Function, values)};

        // Set passthrough tessellation factors
        const Id output_float{TypePointer(spv::StorageClass::Output, float_id)};
        for (int i = 0; i < 4; i++) {
            const Id ptr{OpAccessChain(output_float, gl_tess_level_outer, Int(i))};
            OpStore(ptr, float_one);
        }
        for (int i = 0; i < 2; i++) {
            const Id ptr{OpAccessChain(output_float, gl_tess_level_inner, Int(i))};
            OpStore(ptr, float_one);
        }

        const Id input_vec4{TypePointer(spv::StorageClass::Input, vec4_id)};
        const Id output_vec4{TypePointer(spv::StorageClass::Output, vec4_id)};
        const Id func_int{TypePointer(spv::StorageClass::Function, int_id)};
        const Id invocation_id{OpLoad(int_id, gl_invocation_id)};
        const Id index{OpLoad(int_id, OpAccessChain(func_int, indices, invocation_id))};

        // gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
        const Id in_position{OpLoad(vec4_id, OpAccessChain(input_vec4, gl_in, index, Int(0)))};
        OpStore(OpAccessChain(output_vec4, gl_out, invocation_id, Int(0)), in_position);

        for (int i = 0; i < inputs.size(); i++) {
            // out_paramN[gl_InvocationID] = in_paramN[gl_InvocationID];
            const Id in_param{LoadInput(i, index)};
            OpStore(OpAccessChain(output_vec4, outputs[i], invocation_id), in_param);
        }

        OpReturn();
        OpFunctionEnd();
    }

    /// Emits a passthrough quad tessellation evaluation shader that outputs 4 control points.
    void EmitPassthroughTES() {
        DefineEntry(spv::ExecutionModel::TessellationEvaluation);

        // const int index = int(gl_TessCoord.y) * 2 + int(gl_TessCoord.x);
        const Id input_float{TypePointer(spv::StorageClass::Input, float_id)};
        const Id tess_coord_x{OpLoad(float_id, OpAccessChain(input_float, gl_tess_coord, Int(0)))};
        const Id tess_coord_y{OpLoad(float_id, OpAccessChain(input_float, gl_tess_coord, Int(1)))};
        const Id index{OpIAdd(int_id, OpIMul(int_id, OpConvertFToS(int_id, tess_coord_y), Int(2)),
                              OpConvertFToS(int_id, tess_coord_x))};

        // gl_Position = gl_in[index].gl_Position;
        const Id input_vec4{TypePointer(spv::StorageClass::Input, vec4_id)};
        const Id output_vec4{TypePointer(spv::StorageClass::Output, vec4_id)};
        const Id position{OpLoad(vec4_id, OpAccessChain(input_vec4, gl_in, index, Int(0)))};
        OpStore(OpAccessChain(output_vec4, gl_per_vertex, Int(0)), position);

        // out_paramN = in_paramN[index];
        for (int i = 0; i < inputs.size(); i++) {
            const Id param{OpLoad(vec4_id, OpAccessChain(input_vec4, inputs[i], index))};
            OpStore(outputs[i], param);
        }

        OpReturn();
        OpFunctionEnd();
    }

private:
    Id Int(s32 value) {
        return Constant(int_id, value);
    }

    Id AddInput(Id type) {
        const Id input{AddGlobalVariable(TypePointer(spv::StorageClass::Input, type),
                                         spv::StorageClass::Input)};
        interfaces.push_back(input);
        return input;
    }

    Id AddOutput(Id type) {
        const Id output{AddGlobalVariable(TypePointer(spv::StorageClass::Output, type),
                                          spv::StorageClass::Output)};
        interfaces.push_back(output);
        return output;
    }

    void DefineEntry(spv::ExecutionModel model) {
        AddCapability(spv::Capability::Shader);
        AddCapability(spv::Capability::Tessellation);
        const Id void_function{TypeFunction(void_id)};
        main = OpFunction(void_id, spv::FunctionControlMask::MaskNone, void_function);
        if (model == spv::ExecutionModel::TessellationControl) {
            AddExecutionMode(main, spv::ExecutionMode::OutputVertices, 4U);
        } else {
            AddExecutionMode(main, spv::ExecutionMode::Quads);
            AddExecutionMode(main, spv::ExecutionMode::SpacingEqual);
            AddExecutionMode(main, spv::ExecutionMode::VertexOrderCw);
        }
        DefineInputs(model);
        DefineOutputs(model);
        AddEntryPoint(model, main, "main", interfaces);
        AddLabel(OpLabel());
    }

    void DefineOutputs(spv::ExecutionModel model) {
        if (model == spv::ExecutionModel::TessellationControl) {
            const Id gl_per_vertex_array{TypeArray(gl_per_vertex_type, Constant(uint_id, 4U))};
            gl_out = AddOutput(gl_per_vertex_array);

            const Id arr2_id{TypeArray(float_id, Constant(uint_id, 2U))};
            gl_tess_level_inner = AddOutput(arr2_id);
            Decorate(gl_tess_level_inner, spv::Decoration::BuiltIn, spv::BuiltIn::TessLevelInner);
            Decorate(gl_tess_level_inner, spv::Decoration::Patch);

            const Id arr4_id{TypeArray(float_id, Constant(uint_id, 4U))};
            gl_tess_level_outer = AddOutput(arr4_id);
            Decorate(gl_tess_level_outer, spv::Decoration::BuiltIn, spv::BuiltIn::TessLevelOuter);
            Decorate(gl_tess_level_outer, spv::Decoration::Patch);
        } else {
            gl_per_vertex = AddOutput(gl_per_vertex_type);
        }
        outputs.reserve(fs_info.num_inputs);
        for (int i = 0; i < fs_info.num_inputs; i++) {
            const auto& input = fs_info.inputs[i];
            if (input.IsDefault()) {
                continue;
            }
            outputs.emplace_back(AddOutput(model == spv::ExecutionModel::TessellationControl
                                               ? TypeArray(vec4_id, Int(4))
                                               : vec4_id));
            Decorate(outputs.back(), spv::Decoration::Location, input.param_index);
        }
    }

    void DefineInputs(spv::ExecutionModel model) {
        if (model == spv::ExecutionModel::TessellationEvaluation) {
            gl_tess_coord = AddInput(vec3_id);
            Decorate(gl_tess_coord, spv::Decoration::BuiltIn, spv::BuiltIn::TessCoord);
        } else {
            gl_invocation_id = AddInput(int_id);
            Decorate(gl_invocation_id, spv::Decoration::BuiltIn, spv::BuiltIn::InvocationId);
        }
        const Id gl_per_vertex_array{TypeArray(gl_per_vertex_type, Constant(uint_id, 32U))};
        gl_in = AddInput(gl_per_vertex_array);
        inputs.reserve(fs_info.num_inputs);
        input_components.reserve(fs_info.num_inputs);
        for (int i = 0; i < fs_info.num_inputs; i++) {
            const auto& input = fs_info.inputs[i];
            if (input.IsDefault()) {
                continue;
            }
            // spartanport: the TES reads the TCS outputs, which are always vec4. The TCS reads the
            // vertex shader outputs, so it mirrors them: same location, same component count, and
            // no input at all where the vertex shader writes nothing (LoadInput gives zero there).
            u32 components = 4;
            if (model == spv::ExecutionModel::TessellationControl) {
                components = input.param_index < vs_outputs.size()
                                 ? std::min<u32>(vs_outputs[input.param_index], 4)
                                 : 0;
            }
            input_components.push_back(components);
            if (components == 0) {
                inputs.push_back(vec4_zero);
                continue;
            }
            inputs.emplace_back(AddInput(TypeArray(ComponentType(components), Int(32))));
            Decorate(inputs.back(), spv::Decoration::Location, input.param_index);
        }
    }

    Id ComponentType(u32 components) {
        return components == 1 ? float_id : TypeVector(float_id, components);
    }

    /// spartanport: input i at control point `index`, as a vec4. Inputs narrower than a vec4 are
    /// padded with (0, 0, 0, 1); inputs the vertex shader does not write read as zero.
    Id LoadInput(size_t i, Id index) {
        const u32 components = input_components[i];
        if (components == 0) {
            return vec4_zero;
        }
        const Id type{ComponentType(components)};
        const Id pointer{TypePointer(spv::StorageClass::Input, type)};
        const Id value{OpLoad(type, OpAccessChain(pointer, inputs[i], index))};
        if (components == 4) {
            return value;
        }
        std::array<Id, 4> parts;
        for (u32 c = 0; c < 4; c++) {
            if (c < components) {
                parts[c] = components == 1 ? value : OpCompositeExtract(float_id, value, c);
            } else {
                parts[c] = c == 3 ? float_one : float_zero;
            }
        }
        return OpCompositeConstruct(vec4_id, parts);
    }

private:
    HwFragmentRuntimeInfo fs_info;
    VsOutputs vs_outputs;
    Id main;
    Id void_id;
    Id bool_id;
    Id float_id;
    Id uint_id;
    Id int_id;
    Id bvec2_id;
    Id vec2_id;
    Id vec3_id;
    Id vec4_id;
    Id float_one;
    Id float_min_one;
    Id float_zero;
    Id int_zero;
    Id vec4_zero;
    Id gl_per_vertex_type;
    Id gl_in;
    union {
        Id gl_out;
        Id gl_per_vertex;
    };
    Id gl_tess_level_inner;
    Id gl_tess_level_outer;
    union {
        Id gl_tess_coord;
        Id gl_invocation_id;
    };
    std::vector<Id> inputs;
    std::vector<u32> input_components; ///< One per entry of `inputs`; 0 = not written by the VS.
    std::vector<Id> outputs;
    std::vector<Id> interfaces;
};

std::vector<u32> EmitAuxilaryTessShader(AuxShaderType type, const HwFragmentRuntimeInfo& fs_info,
                                        const VsOutputs& vs_outputs) {
    QuadRectListEmitter ctx{fs_info, vs_outputs};
    switch (type) {
    case AuxShaderType::RectListTCS:
        ctx.EmitRectListTCS();
        break;
    case AuxShaderType::QuadListTCS:
        ctx.EmitQuadListTCS();
        break;
    case AuxShaderType::PassthroughTES:
        ctx.EmitPassthroughTES();
        break;
    }
    return ctx.Assemble();
}

namespace {

// spartanport: the few SPIR-V module facts the helpers below need, read in one pass.
struct ModuleScan {
    struct Variable {
        u32 id;
        u32 pointer_type;
    };
    std::unordered_map<u32, std::string_view> names;
    std::unordered_map<u32, u32> locations;
    std::unordered_map<u32, bool> builtins;
    std::unordered_map<u32, u32> pointee_types; ///< OpTypePointer -> pointee
    std::unordered_map<u32, u32> vector_sizes;  ///< OpTypeVector -> component count
    std::unordered_map<u32, bool> float_types;
    std::vector<Variable> outputs; ///< Output storage class variables, in module order.
};

bool ScanModule(std::span<const u32> spv, ModuleScan& scan) {
    constexpr u32 HeaderWords = 5;
    if (spv.size() < HeaderWords || spv[0] != spv::MagicNumber) {
        return false;
    }
    for (size_t pos = HeaderWords; pos < spv.size();) {
        const u32 count = spv[pos] >> 16;
        const auto op = spv::Op(spv[pos] & 0xffff);
        if (count == 0 || pos + count > spv.size()) {
            return false;
        }
        const auto words = spv.subspan(pos + 1, count - 1);
        switch (op) {
        case spv::Op::OpName:
            if (words.size() >= 2) {
                const auto* chars = reinterpret_cast<const char*>(words.data() + 1);
                const size_t max_len = (words.size() - 1) * sizeof(u32);
                scan.names[words[0]] = std::string_view{chars, strnlen(chars, max_len)};
            }
            break;
        case spv::Op::OpDecorate:
            if (words.size() >= 3 && spv::Decoration(words[1]) == spv::Decoration::Location) {
                scan.locations[words[0]] = words[2];
            } else if (words.size() >= 2 && spv::Decoration(words[1]) == spv::Decoration::BuiltIn) {
                scan.builtins[words[0]] = true;
            }
            break;
        case spv::Op::OpTypeFloat:
            if (words.size() >= 2 && words[1] == 32) {
                scan.float_types[words[0]] = true;
            }
            break;
        case spv::Op::OpTypeVector:
            if (words.size() >= 3) {
                scan.vector_sizes[words[0]] = words[2];
            }
            break;
        case spv::Op::OpTypePointer:
            if (words.size() >= 3) {
                scan.pointee_types[words[0]] = words[2];
            }
            break;
        case spv::Op::OpVariable:
            if (words.size() >= 3 && spv::StorageClass(words[2]) == spv::StorageClass::Output) {
                scan.outputs.push_back({words[1], words[0]});
            }
            break;
        case spv::Op::OpFunction:
            return true; // the global declarations are all read
        default:
            break;
        }
        pos += count;
    }
    return true;
}

/// Components of a float scalar or vector output; 0 for any other type.
u32 FloatComponents(const ModuleScan& scan, u32 pointer_type) {
    const auto pointee = scan.pointee_types.find(pointer_type);
    if (pointee == scan.pointee_types.end()) {
        return 0;
    }
    if (scan.float_types.contains(pointee->second)) {
        return 1;
    }
    const auto vector = scan.vector_sizes.find(pointee->second);
    return vector != scan.vector_sizes.end() ? std::min<u32>(vector->second, 4) : 0;
}

/// The N of a name "<prefix>N", or -1.
s32 NameIndex(std::string_view name, std::string_view prefix) {
    if (!name.starts_with(prefix) || name.size() == prefix.size() ||
        name.size() > prefix.size() + 2) {
        return -1;
    }
    s32 index = 0;
    for (const char c : name.substr(prefix.size())) {
        if (c < '0' || c > '9') {
            return -1;
        }
        index = index * 10 + (c - '0');
    }
    return index;
}

} // namespace

bool AuxTessInputsFromSpirv(std::span<const u32> tcs, HwFragmentRuntimeInfo& fs_info) {
    ModuleScan scan;
    if (!ScanModule(tcs, scan)) {
        return false;
    }
    // The auxiliary TCS has one output per non-default fragment input, at its param_index, in the
    // order of the fragment inputs; the other outputs (gl_out, tessellation levels) have none.
    fs_info = {};
    for (const auto& output : scan.outputs) {
        const auto location = scan.locations.find(output.id);
        if (location == scan.locations.end() || scan.builtins.contains(output.id)) {
            continue;
        }
        if (fs_info.num_inputs == fs_info.inputs.size() || location->second > 0xff) {
            return false;
        }
        auto& input = fs_info.inputs[fs_info.num_inputs++];
        input.param_index = u8(location->second);
        input.is_default = false;
        input.is_flat = false;
    }
    return true;
}

void RestoreVertexOutputStores(Info& info, std::span<const u32> spv) {
    if (info.hw_stage != HwStage::Vertex || info.sw_stage != SwStage::Vertex) {
        return;
    }
    ModuleScan scan;
    if (!ScanModule(spv, scan)) {
        return;
    }
    // The vertex shader names its outputs out_attr<param> (and cldist_attr0 for the emulated clip
    // distances), see EmitContext::DefineOutputs.
    for (const auto& output : scan.outputs) {
        const auto name = scan.names.find(output.id);
        if (name == scan.names.end()) {
            continue;
        }
        const u32 components = FloatComponents(scan, output.pointer_type);
        if (const s32 param = NameIndex(name->second, "out_attr");
            param >= 0 && u32(param) < IR::NumParams) {
            for (u32 c = 0; c < components; c++) {
                info.stores.Set(IR::Attribute::Param0 + param, c);
            }
        } else if (NameIndex(name->second, "cldist_attr") == 0) {
            info.stores.Set(IR::Attribute::ClipDistance);
        }
    }
    info.output_stores_known = true; // even with no parameter outputs: the VS writes none
}

} // namespace Shader::Backend::SPIRV
