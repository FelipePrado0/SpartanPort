// SPDX-License-Identifier: GPL-2.0-or-later
// The rect/quad-list TCS reads only what the vertex shader writes (#1408), and a saved TCS can be
// emitted again from its own SPIR-V. Writes the modules to argv[1] (default .) for spirv-val:
//   for f in out/quad-rect-tcs/*.spv; do spirv-val --target-env vulkan1.2 "$f"; done
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>
#include <sirit/sirit.h>
#include "shader_recompiler/backend/spirv/emit_spirv_quad_rect.h"
#include "shader_recompiler/info.h"

using namespace Shader;
using namespace Shader::Backend::SPIRV;

// Independent reader: location -> component count of non-builtin variables of one storage class.
// For arrays (TCS inputs are arrays of 32 control points) the element type is used.
std::map<u32, u32> Interface(const std::vector<u32>& spv, spv::StorageClass sc) {
    std::map<u32, u32> loc, vec, arr, ptr, flt;
    std::map<u32, bool> builtin;
    std::vector<std::pair<u32, u32>> vars;
    for (size_t pos = 5; pos < spv.size();) {
        const u32 n = spv[pos] >> 16, op = spv[pos] & 0xffff;
        const u32* w = &spv[pos + 1];
        if (op == u32(spv::Op::OpDecorate) && w[1] == u32(spv::Decoration::Location))
            loc[w[0]] = w[2];
        if (op == u32(spv::Op::OpDecorate) && w[1] == u32(spv::Decoration::BuiltIn))
            builtin[w[0]] = true;
        if (op == u32(spv::Op::OpTypeFloat))
            flt[w[0]] = 1;
        if (op == u32(spv::Op::OpTypeVector))
            vec[w[0]] = w[2];
        if (op == u32(spv::Op::OpTypeArray))
            arr[w[0]] = w[1];
        if (op == u32(spv::Op::OpTypePointer) && w[1] == u32(sc))
            ptr[w[0]] = w[2];
        if (op == u32(spv::Op::OpVariable) && w[2] == u32(sc))
            vars.push_back({w[1], w[0]});
        pos += n;
    }
    std::map<u32, u32> out;
    for (auto [id, type] : vars) {
        if (!loc.contains(id) || builtin.contains(id))
            continue;
        u32 t = ptr.at(type);
        if (arr.contains(t))
            t = arr[t];
        out[loc[id]] = flt.contains(t) ? 1 : vec.at(t);
    }
    return out;
}

// A vertex shader with its outputs declared the way EmitContext::DefineOutputs declares them.
std::vector<u32> MakeVertexShader(const std::map<u32, u32>& params, bool clip_emulation) {
    Sirit::Module m{0x00010500};
    m.AddCapability(spv::Capability::Shader);
    const Sirit::Id f32 = m.TypeFloat(32);
    std::vector<Sirit::Id> interfaces;
    const auto out = [&](u32 comps, u32 location, const std::string& name) {
        const Sirit::Id type = comps == 1 ? f32 : m.TypeVector(f32, comps);
        const Sirit::Id v = m.AddGlobalVariable(m.TypePointer(spv::StorageClass::Output, type),
                                                spv::StorageClass::Output);
        m.Decorate(v, spv::Decoration::Location, location);
        m.Name(v, name);
        interfaces.push_back(v);
    };
    // gl_Position as a builtin output, never a parameter.
    const Sirit::Id pos = m.AddGlobalVariable(
        m.TypePointer(spv::StorageClass::Output, m.TypeVector(f32, 4)), spv::StorageClass::Output);
    m.Decorate(pos, spv::Decoration::BuiltIn, spv::BuiltIn::Position);
    interfaces.push_back(pos);
    for (auto [param, comps] : params) {
        out(comps, param + (clip_emulation ? 1 : 0), "out_attr" + std::to_string(param));
    }
    if (clip_emulation) {
        out(4, 0, "cldist_attr0");
    }
    const Sirit::Id void_id = m.TypeVoid();
    const Sirit::Id main =
        m.OpFunction(void_id, spv::FunctionControlMask::MaskNone, m.TypeFunction(void_id));
    m.AddLabel(m.OpLabel());
    m.OpReturn();
    m.OpFunctionEnd();
    m.AddEntryPoint(spv::ExecutionModel::Vertex, main, "main", interfaces);
    return m.Assemble();
}

HwFragmentRuntimeInfo Fs(std::initializer_list<int> params) {
    HwFragmentRuntimeInfo fs{};
    for (int p : params) {
        auto& in = fs.inputs[fs.num_inputs++];
        in.param_index = u8(p < 0 ? 7 : p);
        in.is_default = p < 0; // a default input: no TCS variable
    }
    return fs;
}

int main(int argc, char** argv) {
    const std::filesystem::path dir = argc > 1 ? argv[1] : ".";
    std::filesystem::create_directories(dir);
    // The fragment shader reads params 0, 1, 2 and 5 (plus one default input); the VS writes 0, 1
    // and 5 (5 as a vec2).
    const auto fs = Fs({0, 1, -1, 2, 5});
    VsOutputs vs{};
    vs[0] = 4;
    vs[1] = 4;
    vs[5] = 2;
    using Locs = std::map<u32, u32>;

    for (auto type : {AuxShaderType::RectListTCS, AuxShaderType::QuadListTCS}) {
        // Before: every fragment input is a vec4 input, including location 2 the VS never writes.
        const auto legacy = EmitAuxilaryTessShader(type, fs);
        assert((Interface(legacy, spv::StorageClass::Input) ==
                Locs{{0, 4}, {1, 4}, {2, 4}, {5, 4}}));

        // After: the inputs mirror the VS; the outputs (read by the TES) stay vec4 everywhere.
        const auto tcs = EmitAuxilaryTessShader(type, fs, vs);
        assert((Interface(tcs, spv::StorageClass::Input) == Locs{{0, 4}, {1, 4}, {5, 2}}));
        assert((Interface(tcs, spv::StorageClass::Output) == Locs{{0, 4}, {1, 4}, {2, 4}, {5, 4}}));

        // A saved (legacy) TCS gives back its fragment inputs, and emitting from them matches.
        HwFragmentRuntimeInfo stored{};
        assert(AuxTessInputsFromSpirv(legacy, stored));
        assert(stored.num_inputs == 4);
        assert(stored.inputs[0].param_index == 0 && stored.inputs[1].param_index == 1 &&
               stored.inputs[2].param_index == 2 && stored.inputs[3].param_index == 5);
        assert(EmitAuxilaryTessShader(type, stored, vs) == tcs);
        assert(EmitAuxilaryTessShader(type, stored) == legacy);
        // A TCS that already mirrors the VS gives the same inputs (its outputs are unchanged).
        HwFragmentRuntimeInfo again{};
        assert(AuxTessInputsFromSpirv(tcs, again) && again.num_inputs == 4);
        assert(EmitAuxilaryTessShader(type, again, vs) == tcs);
    }
    // The TES is unchanged.
    assert(EmitAuxilaryTessShader(AuxShaderType::PassthroughTES, fs) ==
           EmitAuxilaryTessShader(AuxShaderType::PassthroughTES, fs, vs));
    // Garbage is refused.
    HwFragmentRuntimeInfo junk{};
    assert(!AuxTessInputsFromSpirv(std::vector<u32>{1, 2, 3}, junk));
    assert(!AuxTessInputsFromSpirv(std::vector<u32>{}, junk));

    // Vertex stores come back from the SPIR-V, with and without emulated clip distances.
    for (bool clip : {false, true}) {
        const auto spv = MakeVertexShader({{0, 4}, {1, 4}, {5, 2}}, clip);
        Info info{};
        info.hw_stage = HwStage::Vertex;
        info.sw_stage = SwStage::Vertex;
        info.output_stores_known = false; // as Info::Deserialize leaves it
        RestoreVertexOutputStores(info, spv);
        assert(info.output_stores_known);
        for (int i = 0; i < int(IR::NumParams); i++) {
            const bool expect = i == 0 || i == 1 || i == 5;
            assert(info.stores.GetAny(IR::Attribute::Param0 + i) == expect);
        }
        assert(info.stores.Get(IR::Attribute::Param0 + 5, 1));
        assert(!info.stores.Get(IR::Attribute::Param0 + 5, 2));
        assert(info.stores.GetAny(IR::Attribute::ClipDistance) == clip);
        assert(!info.stores.GetAny(IR::Attribute::Position0));

        // Every location the TCS reads is one the VS outputs, with the same component count.
        VsOutputs restored{};
        const u32 base = clip ? 1 : 0;
        if (clip) {
            restored[0] = 4;
        }
        restored[base + 0] = 4;
        restored[base + 1] = 4;
        restored[base + 5] = 2;
        const auto vs_out = Interface(spv, spv::StorageClass::Output);
        const auto tcs_in =
            Interface(EmitAuxilaryTessShader(AuxShaderType::RectListTCS, fs, restored),
                      spv::StorageClass::Input);
        for (auto [location, comps] : tcs_in) {
            assert(vs_out.contains(location) && vs_out.at(location) == comps);
        }
    }
    // A vertex shader that writes no parameters is known to write none, so the TCS reads nothing
    // from it; one whose SPIR-V cannot be read stays unknown (the TCS keeps every vec4 input).
    {
        Info none{};
        none.hw_stage = HwStage::Vertex;
        none.sw_stage = SwStage::Vertex;
        none.output_stores_known = false;
        RestoreVertexOutputStores(none, MakeVertexShader({}, false));
        assert(none.output_stores_known);
        for (int i = 0; i < int(IR::NumParams); i++) {
            assert(!none.stores.GetAny(IR::Attribute::Param0 + i));
        }
        assert(Interface(EmitAuxilaryTessShader(AuxShaderType::RectListTCS, fs, VsOutputs{}),
                         spv::StorageClass::Input)
                   .empty());

        Info unreadable{};
        unreadable.hw_stage = HwStage::Vertex;
        unreadable.sw_stage = SwStage::Vertex;
        unreadable.output_stores_known = false;
        RestoreVertexOutputStores(unreadable, std::vector<u32>{1, 2, 3});
        assert(!unreadable.output_stores_known);
    }
    // Other stages are left alone.
    Info fragment{};
    fragment.hw_stage = HwStage::Fragment;
    fragment.sw_stage = SwStage::Fragment;
    fragment.output_stores_known = false;
    RestoreVertexOutputStores(fragment, MakeVertexShader({{0, 4}}, false));
    assert(!fragment.stores.GetAny(IR::Attribute::Param0));
    assert(!fragment.output_stores_known);

    // Modules for spirv-val: every TCS shape (vec4, vec2, scalar, absent inputs) and the TES.
    VsOutputs mixed{};
    mixed[0] = 4;
    mixed[1] = 1;
    mixed[5] = 3;
    const auto dump = [&](const char* name, const std::vector<u32>& spv) {
        FILE* f = std::fopen((dir / name).string().c_str(), "wb");
        assert(f);
        std::fwrite(spv.data(), 4, spv.size(), f);
        std::fclose(f);
    };
    dump("rect_legacy.spv", EmitAuxilaryTessShader(AuxShaderType::RectListTCS, fs));
    dump("rect_vs.spv", EmitAuxilaryTessShader(AuxShaderType::RectListTCS, fs, vs));
    dump("rect_mixed.spv", EmitAuxilaryTessShader(AuxShaderType::RectListTCS, fs, mixed));
    dump("quad_vs.spv", EmitAuxilaryTessShader(AuxShaderType::QuadListTCS, fs, vs));
    dump("quad_mixed.spv", EmitAuxilaryTessShader(AuxShaderType::QuadListTCS, fs, mixed));
    dump("rect_none.spv", EmitAuxilaryTessShader(AuxShaderType::RectListTCS, fs, VsOutputs{}));
    dump("tes.spv", EmitAuxilaryTessShader(AuxShaderType::PassthroughTES, fs, vs));

    std::puts("quad/rect TCS tests passed");
    return 0;
}
