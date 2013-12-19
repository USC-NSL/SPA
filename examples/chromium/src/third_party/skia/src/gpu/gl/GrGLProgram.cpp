/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "GrGLProgram.h"

#include "GrAllocator.h"
#include "GrEffect.h"
#include "GrDrawEffect.h"
#include "GrGLEffect.h"
#include "GrGpuGL.h"
#include "GrGLShaderVar.h"
#include "GrGLSL.h"
#include "SkTrace.h"
#include "SkXfermode.h"

#include "SkRTConf.h"

SK_DEFINE_INST_COUNT(GrGLProgram)

#define GL_CALL(X) GR_GL_CALL(fContext.interface(), X)
#define GL_CALL_RET(R, X) GR_GL_CALL_RET(fContext.interface(), R, X)

SK_CONF_DECLARE(bool, c_PrintShaders, "gpu.printShaders", false,
                "Print the source code for all shaders generated.");

#define COL_ATTR_NAME "aColor"
#define COV_ATTR_NAME "aCoverage"
#define EDGE_ATTR_NAME "aEdge"

namespace {
inline const char* declared_color_output_name() { return "fsColorOut"; }
inline const char* dual_source_output_name() { return "dualSourceOut"; }
}

GrGLProgram* GrGLProgram::Create(const GrGLContext& gl,
                                 const GrGLProgramDesc& desc,
                                 const GrEffectStage* stages[]) {
    GrGLProgram* program = SkNEW_ARGS(GrGLProgram, (gl, desc, stages));
    if (!program->succeeded()) {
        delete program;
        program = NULL;
    }
    return program;
}

GrGLProgram::GrGLProgram(const GrGLContext& gl,
                         const GrGLProgramDesc& desc,
                         const GrEffectStage* stages[])
: fContext(gl)
, fUniformManager(gl) {
    fDesc = desc;
    fVShaderID = 0;
    fGShaderID = 0;
    fFShaderID = 0;
    fProgramID = 0;

    fColor = GrColor_ILLEGAL;
    fColorFilterColor = GrColor_ILLEGAL;

    for (int s = 0; s < GrDrawState::kNumStages; ++s) {
        fEffects[s] = NULL;
    }

    this->genProgram(stages);
}

GrGLProgram::~GrGLProgram() {
    if (fVShaderID) {
        GL_CALL(DeleteShader(fVShaderID));
    }
    if (fGShaderID) {
        GL_CALL(DeleteShader(fGShaderID));
    }
    if (fFShaderID) {
        GL_CALL(DeleteShader(fFShaderID));
    }
    if (fProgramID) {
        GL_CALL(DeleteProgram(fProgramID));
    }

    for (int i = 0; i < GrDrawState::kNumStages; ++i) {
        delete fEffects[i];
    }
}

void GrGLProgram::abandon() {
    fVShaderID = 0;
    fGShaderID = 0;
    fFShaderID = 0;
    fProgramID = 0;
}

void GrGLProgram::overrideBlend(GrBlendCoeff* srcCoeff,
                                GrBlendCoeff* dstCoeff) const {
    switch (fDesc.fCoverageOutput) {
        case GrGLProgramDesc::kModulate_CoverageOutput:
            break;
        // The prog will write a coverage value to the secondary
        // output and the dst is blended by one minus that value.
        case GrGLProgramDesc::kSecondaryCoverage_CoverageOutput:
        case GrGLProgramDesc::kSecondaryCoverageISA_CoverageOutput:
        case GrGLProgramDesc::kSecondaryCoverageISC_CoverageOutput:
            *dstCoeff = (GrBlendCoeff)GrGpu::kIS2C_GrBlendCoeff;
            break;
        case GrGLProgramDesc::kCombineWithDst_CoverageOutput:
            // We should only have set this if the blend was specified as (1, 0)
            GrAssert(kOne_GrBlendCoeff == *srcCoeff && kZero_GrBlendCoeff == *dstCoeff);
            break;
        default:
            GrCrash("Unexpected coverage output");
            break;
    }
}

namespace {
// given two blend coefficients determine whether the src
// and/or dst computation can be omitted.
inline void need_blend_inputs(SkXfermode::Coeff srcCoeff,
                              SkXfermode::Coeff dstCoeff,
                              bool* needSrcValue,
                              bool* needDstValue) {
    if (SkXfermode::kZero_Coeff == srcCoeff) {
        switch (dstCoeff) {
            // these all read the src
            case SkXfermode::kSC_Coeff:
            case SkXfermode::kISC_Coeff:
            case SkXfermode::kSA_Coeff:
            case SkXfermode::kISA_Coeff:
                *needSrcValue = true;
                break;
            default:
                *needSrcValue = false;
                break;
        }
    } else {
        *needSrcValue = true;
    }
    if (SkXfermode::kZero_Coeff == dstCoeff) {
        switch (srcCoeff) {
            // these all read the dst
            case SkXfermode::kDC_Coeff:
            case SkXfermode::kIDC_Coeff:
            case SkXfermode::kDA_Coeff:
            case SkXfermode::kIDA_Coeff:
                *needDstValue = true;
                break;
            default:
                *needDstValue = false;
                break;
        }
    } else {
        *needDstValue = true;
    }
}

/**
 * Create a blend_coeff * value string to be used in shader code. Sets empty
 * string if result is trivially zero.
 */
inline void blend_term_string(SkString* str, SkXfermode::Coeff coeff,
                       const char* src, const char* dst,
                       const char* value) {
    switch (coeff) {
    case SkXfermode::kZero_Coeff:    /** 0 */
        *str = "";
        break;
    case SkXfermode::kOne_Coeff:     /** 1 */
        *str = value;
        break;
    case SkXfermode::kSC_Coeff:
        str->printf("(%s * %s)", src, value);
        break;
    case SkXfermode::kISC_Coeff:
        str->printf("((%s - %s) * %s)", GrGLSLOnesVecf(4), src, value);
        break;
    case SkXfermode::kDC_Coeff:
        str->printf("(%s * %s)", dst, value);
        break;
    case SkXfermode::kIDC_Coeff:
        str->printf("((%s - %s) * %s)", GrGLSLOnesVecf(4), dst, value);
        break;
    case SkXfermode::kSA_Coeff:      /** src alpha */
        str->printf("(%s.a * %s)", src, value);
        break;
    case SkXfermode::kISA_Coeff:     /** inverse src alpha (i.e. 1 - sa) */
        str->printf("((1.0 - %s.a) * %s)", src, value);
        break;
    case SkXfermode::kDA_Coeff:      /** dst alpha */
        str->printf("(%s.a * %s)", dst, value);
        break;
    case SkXfermode::kIDA_Coeff:     /** inverse dst alpha (i.e. 1 - da) */
        str->printf("((1.0 - %s.a) * %s)", dst, value);
        break;
    default:
        GrCrash("Unexpected xfer coeff.");
        break;
    }
}
/**
 * Adds a line to the fragment shader code which modifies the color by
 * the specified color filter.
 */
void add_color_filter(GrGLShaderBuilder* builder,
                      const char * outputVar,
                      SkXfermode::Coeff uniformCoeff,
                      SkXfermode::Coeff colorCoeff,
                      const char* filterColor,
                      const char* inColor) {
    SkString colorStr, constStr;
    blend_term_string(&colorStr, colorCoeff, filterColor, inColor, inColor);
    blend_term_string(&constStr, uniformCoeff, filterColor, inColor, filterColor);

    SkString sum;
    GrGLSLAddf<4>(&sum, colorStr.c_str(), constStr.c_str());
    builder->fsCodeAppendf("\t%s = %s;\n", outputVar, sum.c_str());
}
}

GrSLConstantVec GrGLProgram::genInputColor(GrGLShaderBuilder* builder, SkString* inColor) {
    switch (fDesc.fColorInput) {
        case GrGLProgramDesc::kAttribute_ColorInput: {
            builder->addAttribute(kVec4f_GrSLType, COL_ATTR_NAME);
            const char *vsName, *fsName;
            builder->addVarying(kVec4f_GrSLType, "Color", &vsName, &fsName);
            builder->vsCodeAppendf("\t%s = " COL_ATTR_NAME ";\n", vsName);
            *inColor = fsName;
            return kNone_GrSLConstantVec;
        }
        case GrGLProgramDesc::kUniform_ColorInput: {
            const char* name;
            fUniformHandles.fColorUni = builder->addUniform(GrGLShaderBuilder::kFragment_ShaderType,
                                                            kVec4f_GrSLType, "Color", &name);
            *inColor = name;
            return kNone_GrSLConstantVec;
        }
        case GrGLProgramDesc::kTransBlack_ColorInput:
            inColor->reset();
            return kZeros_GrSLConstantVec;
        case GrGLProgramDesc::kSolidWhite_ColorInput:
            inColor->reset();
            return kOnes_GrSLConstantVec;
        default:
            GrCrash("Unknown color type.");
            return kNone_GrSLConstantVec;
    }
}

GrSLConstantVec GrGLProgram::genInputCoverage(GrGLShaderBuilder* builder, SkString* inCoverage) {
    switch (fDesc.fCoverageInput) {
        case GrGLProgramDesc::kAttribute_ColorInput: {
            builder->addAttribute(kVec4f_GrSLType, COV_ATTR_NAME);
            const char *vsName, *fsName;
            builder->addVarying(kVec4f_GrSLType, "Coverage", &vsName, &fsName);
            builder->vsCodeAppendf("\t%s = " COV_ATTR_NAME ";\n", vsName);
            *inCoverage = fsName;
            return kNone_GrSLConstantVec;
        }
        case GrGLProgramDesc::kUniform_ColorInput: {
            const char* name;
            fUniformHandles.fCoverageUni =
                builder->addUniform(GrGLShaderBuilder::kFragment_ShaderType,
                                    kVec4f_GrSLType, "Coverage", &name);
            *inCoverage = name;
            return kNone_GrSLConstantVec;
        }
        case GrGLProgramDesc::kTransBlack_ColorInput:
            inCoverage->reset();
            return kZeros_GrSLConstantVec;
        case GrGLProgramDesc::kSolidWhite_ColorInput:
            inCoverage->reset();
            return kOnes_GrSLConstantVec;
        default:
            GrCrash("Unknown color type.");
            return kNone_GrSLConstantVec;
    }
}

void GrGLProgram::genGeometryShader(GrGLShaderBuilder* builder) const {
#if GR_GL_EXPERIMENTAL_GS
    // TODO: The builder should add all this glue code.
    if (fDesc.fExperimentalGS) {
        GrAssert(fContext.info().glslGeneration() >= k150_GrGLSLGeneration);
        builder->fGSHeader.append("layout(triangles) in;\n"
                                   "layout(triangle_strip, max_vertices = 6) out;\n");
        builder->gsCodeAppend("\tfor (int i = 0; i < 3; ++i) {\n"
                              "\t\tgl_Position = gl_in[i].gl_Position;\n");
        if (fDesc.fEmitsPointSize) {
            builder->gsCodeAppend("\t\tgl_PointSize = 1.0;\n");
        }
        GrAssert(builder->fGSInputs.count() == builder->fGSOutputs.count());
        int count = builder->fGSInputs.count();
        for (int i = 0; i < count; ++i) {
            builder->gsCodeAppendf("\t\t%s = %s[i];\n",
                                   builder->fGSOutputs[i].getName().c_str(),
                                   builder->fGSInputs[i].getName().c_str());
        }
        builder->gsCodeAppend("\t\tEmitVertex();\n"
                              "\t}\n"
                              "\tEndPrimitive();\n");
    }
#endif
}

const char* GrGLProgram::adjustInColor(const SkString& inColor) const {
    if (inColor.size()) {
          return inColor.c_str();
    } else {
        if (GrGLProgramDesc::kSolidWhite_ColorInput == fDesc.fColorInput) {
            return GrGLSLOnesVecf(4);
        } else {
            return GrGLSLZerosVecf(4);
        }
    }
}

namespace {
// prints a shader using params similar to glShaderSource
void print_shader(GrGLint stringCnt,
                  const GrGLchar** strings,
                  GrGLint* stringLengths) {
    for (int i = 0; i < stringCnt; ++i) {
        if (NULL == stringLengths || stringLengths[i] < 0) {
            GrPrintf(strings[i]);
        } else {
            GrPrintf("%.*s", stringLengths[i], strings[i]);
        }
    }
}

// Compiles a GL shader, returns shader ID or 0 if failed params have same meaning as glShaderSource
GrGLuint compile_shader(const GrGLContext& gl,
                        GrGLenum type,
                        int stringCnt,
                        const char** strings,
                        int* stringLengths) {
    SK_TRACE_EVENT1("GrGLProgram::CompileShader",
                    "stringCount", SkStringPrintf("%i", stringCnt).c_str());

    GrGLuint shader;
    GR_GL_CALL_RET(gl.interface(), shader, CreateShader(type));
    if (0 == shader) {
        return 0;
    }

    const GrGLInterface* gli = gl.interface();
    GrGLint compiled = GR_GL_INIT_ZERO;
    GR_GL_CALL(gli, ShaderSource(shader, stringCnt, strings, stringLengths));
    GR_GL_CALL(gli, CompileShader(shader));
    GR_GL_CALL(gli, GetShaderiv(shader, GR_GL_COMPILE_STATUS, &compiled));

    if (!compiled) {
        GrGLint infoLen = GR_GL_INIT_ZERO;
        GR_GL_CALL(gli, GetShaderiv(shader, GR_GL_INFO_LOG_LENGTH, &infoLen));
        SkAutoMalloc log(sizeof(char)*(infoLen+1)); // outside if for debugger
        if (infoLen > 0) {
            // retrieve length even though we don't need it to workaround bug in chrome cmd buffer
            // param validation.
            GrGLsizei length = GR_GL_INIT_ZERO;
            GR_GL_CALL(gli, GetShaderInfoLog(shader, infoLen+1,
                                             &length, (char*)log.get()));
            print_shader(stringCnt, strings, stringLengths);
            GrPrintf("\n%s", log.get());
        }
        GrAssert(!"Shader compilation failed!");
        GR_GL_CALL(gli, DeleteShader(shader));
        return 0;
    }
    return shader;
}

// helper version of above for when shader is already flattened into a single SkString
GrGLuint compile_shader(const GrGLContext& gl, GrGLenum type, const SkString& shader) {
    const GrGLchar* str = shader.c_str();
    int length = shader.size();
    return compile_shader(gl, type, 1, &str, &length);
}

void expand_known_value4f(SkString* string, GrSLConstantVec vec) {
    GrAssert(string->isEmpty() == (vec != kNone_GrSLConstantVec));
    switch (vec) {
        case kNone_GrSLConstantVec:
            break;
        case kZeros_GrSLConstantVec:
            *string = GrGLSLZerosVecf(4);
            break;
        case kOnes_GrSLConstantVec:
            *string = GrGLSLOnesVecf(4);
            break;
    }
}

}

// compiles all the shaders from builder and stores the shader IDs
bool GrGLProgram::compileShaders(const GrGLShaderBuilder& builder) {

    SkString shader;

    builder.getShader(GrGLShaderBuilder::kVertex_ShaderType, &shader);
    if (c_PrintShaders) {
        GrPrintf(shader.c_str());
        GrPrintf("\n");
    }

    if (!(fVShaderID = compile_shader(fContext, GR_GL_VERTEX_SHADER, shader))) {
        return false;
    }

    fGShaderID = 0;
#if GR_GL_EXPERIMENTAL_GS
    if (fDesc.fExperimentalGS) {
        builder.getShader(GrGLShaderBuilder::kGeometry_ShaderType, &shader);
        if (c_PrintShaders) {
            GrPrintf(shader.c_str());
            GrPrintf("\n");
        }
        if (!(fGShaderID = compile_shader(fContext, GR_GL_GEOMETRY_SHADER, shader))) {
            return false;
        }
    }
#endif

    builder.getShader(GrGLShaderBuilder::kFragment_ShaderType, &shader);
    if (c_PrintShaders) {
        GrPrintf(shader.c_str());
        GrPrintf("\n");
    }
    if (!(fFShaderID = compile_shader(fContext, GR_GL_FRAGMENT_SHADER, shader))) {
        return false;
    }

    return true;
}

bool GrGLProgram::genProgram(const GrEffectStage* stages[]) {
    GrAssert(0 == fProgramID);

    GrGLShaderBuilder builder(fContext.info(), fUniformManager, fDesc);

    // the dual source output has no canonical var name, have to
    // declare an output, which is incompatible with gl_FragColor/gl_FragData.
    bool dualSourceOutputWritten = false;

    GrGLShaderVar colorOutput;
    bool isColorDeclared = GrGLSLSetupFSColorOuput(fContext.info().glslGeneration(),
                                                   declared_color_output_name(),
                                                   &colorOutput);
    if (isColorDeclared) {
        builder.fFSOutputs.push_back(colorOutput);
    }

    const char* viewMName;
    fUniformHandles.fViewMatrixUni = builder.addUniform(GrGLShaderBuilder::kVertex_ShaderType,
                                                        kMat33f_GrSLType, "ViewM", &viewMName);


    builder.vsCodeAppendf("\tvec3 pos3 = %s * vec3(%s, 1);\n"
                          "\tgl_Position = vec4(pos3.xy, 0, pos3.z);\n",
                          viewMName, builder.positionAttribute().getName().c_str());

    // incoming color to current stage being processed.
    SkString inColor;
    GrSLConstantVec knownColorValue = this->genInputColor(&builder, &inColor);

    // we output point size in the GS if present
    if (fDesc.fEmitsPointSize
#if GR_GL_EXPERIMENTAL_GS
        && !fDesc.fExperimentalGS
#endif
        ) {
        builder.vsCodeAppend("\tgl_PointSize = 1.0;\n");
    }

    // Get the coeffs for the Mode-based color filter, determine if color is needed.
    SkXfermode::Coeff colorCoeff;
    SkXfermode::Coeff filterColorCoeff;
    SkAssertResult(
        SkXfermode::ModeAsCoeff(static_cast<SkXfermode::Mode>(fDesc.fColorFilterXfermode),
                                &filterColorCoeff,
                                &colorCoeff));
    bool needColor, needFilterColor;
    need_blend_inputs(filterColorCoeff, colorCoeff, &needFilterColor, &needColor);

    if (needColor) {
        ///////////////////////////////////////////////////////////////////////////
        // compute the color
        // if we have color stages string them together, feeding the output color
        // of each to the next and generating code for each stage.
        SkString outColor;
        for (int s = 0; s < fDesc.fFirstCoverageStage; ++s) {
            if (GrGLEffect::kNoEffectKey != fDesc.fEffectKeys[s]) {
                if (kZeros_GrSLConstantVec == knownColorValue) {
                    // Effects have no way to communicate zeros, they treat an empty string as ones.
                    inColor = "initialColor";
                    builder.fsCodeAppendf("\tvec4 %s = %s;\n", inColor.c_str(), GrGLSLZerosVecf(4));
                }
                // create var to hold stage result
                outColor = "color";
                outColor.appendS32(s);
                builder.fsCodeAppendf("\tvec4 %s;\n", outColor.c_str());

                builder.setCurrentStage(s);
                fEffects[s] = builder.createAndEmitGLEffect(*stages[s],
                                                            fDesc.fEffectKeys[s],
                                                            inColor.size() ? inColor.c_str() : NULL,
                                                            outColor.c_str(),
                                                            &fUniformHandles.fEffectSamplerUnis[s]);
                builder.setNonStage();
                inColor = outColor;
                knownColorValue = kNone_GrSLConstantVec;
            }
        }
    }

    // Insert the color filter. This will soon be replaced by a color effect.
    if (SkXfermode::kDst_Mode != fDesc.fColorFilterXfermode) {
        const char* colorFilterColorUniName = NULL;
        fUniformHandles.fColorFilterUni = builder.addUniform(GrGLShaderBuilder::kFragment_ShaderType,
                                                             kVec4f_GrSLType, "FilterColor",
                                                             &colorFilterColorUniName);

        builder.fsCodeAppend("\tvec4 filteredColor;\n");
        const char* color;
        // add_color_filter requires a real input string.
        if (knownColorValue == kOnes_GrSLConstantVec) {
            color = GrGLSLOnesVecf(4);
        } else if (knownColorValue == kZeros_GrSLConstantVec) {
            color = GrGLSLZerosVecf(4);
        } else {
            color = inColor.c_str();
        }
        add_color_filter(&builder, "filteredColor", filterColorCoeff,
                         colorCoeff, colorFilterColorUniName, color);
        inColor = "filteredColor";
    }

    ///////////////////////////////////////////////////////////////////////////
    // compute the partial coverage

    // incoming coverage to current stage being processed.
    SkString inCoverage;
    GrSLConstantVec knownCoverageValue = this->genInputCoverage(&builder, &inCoverage);

    SkString outCoverage;
    for (int s = fDesc.fFirstCoverageStage; s < GrDrawState::kNumStages; ++s) {
        if (fDesc.fEffectKeys[s]) {
            if (kZeros_GrSLConstantVec == knownCoverageValue) {
                // Effects have no way to communicate zeros, they treat an empty string as ones.
                inCoverage = "initialCoverage";
                builder.fsCodeAppendf("\tvec4 %s = %s;\n", inCoverage.c_str(), GrGLSLZerosVecf(4));
            }
            // create var to hold stage output
            outCoverage = "coverage";
            outCoverage.appendS32(s);
            builder.fsCodeAppendf("\tvec4 %s;\n", outCoverage.c_str());

            builder.setCurrentStage(s);
            fEffects[s] = builder.createAndEmitGLEffect(
                                            *stages[s],
                                            fDesc.fEffectKeys[s],
                                            inCoverage.size() ? inCoverage.c_str() : NULL,
                                            outCoverage.c_str(),
                                            &fUniformHandles.fEffectSamplerUnis[s]);
            builder.setNonStage();
            inCoverage = outCoverage;
            knownCoverageValue = kNone_GrSLConstantVec;
        }
    }

    // discard if coverage is zero
    if (fDesc.fDiscardIfZeroCoverage && kOnes_GrSLConstantVec != knownCoverageValue) {
        if (kZeros_GrSLConstantVec == knownCoverageValue) {
            // This is unfortunate.
            builder.fsCodeAppend("\tdiscard;\n");
        } else {
            builder.fsCodeAppendf("\tif (all(lessThanEqual(%s, vec4(0.0)))) {\n\t\tdiscard;\n\t}\n",
                                  inCoverage.c_str());
        }
    }

    GrGLProgramDesc::CoverageOutput coverageOutput =
        static_cast<GrGLProgramDesc::CoverageOutput>(fDesc.fCoverageOutput);
    if (GrGLProgramDesc::CoverageOutputUsesSecondaryOutput(coverageOutput)) {
        builder.fFSOutputs.push_back().set(kVec4f_GrSLType,
                                           GrGLShaderVar::kOut_TypeModifier,
                                           dual_source_output_name());
        // default coeff to ones for kCoverage_DualSrcOutput
        SkString coeff;
        GrSLConstantVec knownCoeffValue = kOnes_GrSLConstantVec;
        if (GrGLProgramDesc::kSecondaryCoverageISA_CoverageOutput == fDesc.fCoverageOutput) {
            // Get (1-A) into coeff
            SkString inColorAlpha;
            GrGLSLGetComponent4f(&inColorAlpha,
                                    inColor.c_str(),
                                    kA_GrColorComponentFlag,
                                    knownColorValue,
                                    true);
            knownCoeffValue = GrGLSLSubtractf<1>(&coeff,
                                                 NULL,
                                                 inColorAlpha.c_str(),
                                                 kOnes_GrSLConstantVec,
                                                 knownColorValue,
                                                 true);
        } else if (GrGLProgramDesc::kSecondaryCoverageISC_CoverageOutput == coverageOutput) {
            // Get (1-RGBA) into coeff
            knownCoeffValue = GrGLSLSubtractf<4>(&coeff,
                                                 NULL,
                                                 inColor.c_str(),
                                                 kOnes_GrSLConstantVec,
                                                 knownColorValue,
                                                 true);
        }
        // Get coeff * coverage into modulate and then write that to the dual source output.
        SkString modulate;
        GrGLSLModulatef<4>(&modulate,
                           coeff.c_str(),
                           inCoverage.c_str(),
                           knownCoeffValue,
                           knownCoverageValue,
                           false);
        builder.fsCodeAppendf("\t%s = %s;\n", dual_source_output_name(), modulate.c_str());
        dualSourceOutputWritten = true;
    }

    ///////////////////////////////////////////////////////////////////////////
    // combine color and coverage as frag color

    // Get "color * coverage" into fragColor
    SkString fragColor;
    GrSLConstantVec knownFragColorValue = GrGLSLModulatef<4>(&fragColor,
                                                             inColor.c_str(),
                                                             inCoverage.c_str(),
                                                             knownColorValue,
                                                             knownCoverageValue,
                                                             true);
    // Now tack on "+(1-coverage)dst onto the frag color if we were asked to do so.
    if (GrGLProgramDesc::kCombineWithDst_CoverageOutput == coverageOutput) {
        SkString dstCoeff;
        GrSLConstantVec knownDstCoeffValue = GrGLSLSubtractf<4>(&dstCoeff,
                                                                NULL,
                                                                inCoverage.c_str(),
                                                                kOnes_GrSLConstantVec,
                                                                knownCoverageValue,
                                                                true);
        SkString dstContribution;
        GrSLConstantVec knownDstContributionValue = GrGLSLModulatef<4>(&dstContribution,
                                                                       dstCoeff.c_str(),
                                                                       builder.dstColor(),
                                                                       knownDstCoeffValue,
                                                                       kNone_GrSLConstantVec,
                                                                       true);
        SkString oldFragColor = fragColor;
        fragColor.reset();
        GrGLSLAddf<4>(&fragColor,
                      oldFragColor.c_str(),
                      dstContribution.c_str(),
                      knownFragColorValue,
                      knownDstContributionValue,
                      false);
    } else {
        expand_known_value4f(&fragColor, knownFragColorValue);
    }
    builder.fsCodeAppendf("\t%s = %s;\n", colorOutput.getName().c_str(), fragColor.c_str());

    ///////////////////////////////////////////////////////////////////////////
    // insert GS
#if GR_DEBUG
    this->genGeometryShader(&builder);
#endif

    ///////////////////////////////////////////////////////////////////////////
    // compile and setup attribs and unis

    if (!this->compileShaders(builder)) {
        return false;
    }

    if (!this->bindOutputsAttribsAndLinkProgram(builder,
                                                isColorDeclared,
                                                dualSourceOutputWritten)) {
        return false;
    }

    builder.finished(fProgramID);
    fUniformHandles.fRTHeightUni = builder.getRTHeightUniform();
    fUniformHandles.fDstCopyTopLeftUni = builder.getDstCopyTopLeftUniform();
    fUniformHandles.fDstCopyScaleUni = builder.getDstCopyScaleUniform();
    fUniformHandles.fDstCopySamplerUni = builder.getDstCopySamplerUniform();
    // This must be called after we set fDstCopySamplerUni above.
    this->initSamplerUniforms();

    return true;
}

bool GrGLProgram::bindOutputsAttribsAndLinkProgram(const GrGLShaderBuilder& builder,
                                                   bool bindColorOut,
                                                   bool bindDualSrcOut) {
    GL_CALL_RET(fProgramID, CreateProgram());
    if (!fProgramID) {
        return false;
    }

    GL_CALL(AttachShader(fProgramID, fVShaderID));
    if (fGShaderID) {
        GL_CALL(AttachShader(fProgramID, fGShaderID));
    }
    GL_CALL(AttachShader(fProgramID, fFShaderID));

    if (bindColorOut) {
        GL_CALL(BindFragDataLocation(fProgramID, 0, declared_color_output_name()));
    }
    if (bindDualSrcOut) {
        GL_CALL(BindFragDataLocationIndexed(fProgramID, 0, 1, dual_source_output_name()));
    }

    // Bind the attrib locations to same values for all shaders
    GL_CALL(BindAttribLocation(fProgramID,
                               fDesc.fPositionAttributeIndex,
                               builder.positionAttribute().c_str()));
    if (-1 != fDesc.fLocalCoordAttributeIndex) {
        GL_CALL(BindAttribLocation(fProgramID,
                                   fDesc.fLocalCoordAttributeIndex,
                                   builder.localCoordsAttribute().c_str()));
    }
    if (-1 != fDesc.fColorAttributeIndex) {
        GL_CALL(BindAttribLocation(fProgramID, fDesc.fColorAttributeIndex, COL_ATTR_NAME));
    }
    if (-1 != fDesc.fCoverageAttributeIndex) {
        GL_CALL(BindAttribLocation(fProgramID, fDesc.fCoverageAttributeIndex, COV_ATTR_NAME));
    }

    const GrGLShaderBuilder::AttributePair* attribEnd = builder.getEffectAttributes().end();
    for (const GrGLShaderBuilder::AttributePair* attrib = builder.getEffectAttributes().begin();
         attrib != attribEnd;
         ++attrib) {
         GL_CALL(BindAttribLocation(fProgramID, attrib->fIndex, attrib->fName.c_str()));
    }

    GL_CALL(LinkProgram(fProgramID));

    GrGLint linked = GR_GL_INIT_ZERO;
    GL_CALL(GetProgramiv(fProgramID, GR_GL_LINK_STATUS, &linked));
    if (!linked) {
        GrGLint infoLen = GR_GL_INIT_ZERO;
        GL_CALL(GetProgramiv(fProgramID, GR_GL_INFO_LOG_LENGTH, &infoLen));
        SkAutoMalloc log(sizeof(char)*(infoLen+1));  // outside if for debugger
        if (infoLen > 0) {
            // retrieve length even though we don't need it to workaround
            // bug in chrome cmd buffer param validation.
            GrGLsizei length = GR_GL_INIT_ZERO;
            GL_CALL(GetProgramInfoLog(fProgramID,
                                      infoLen+1,
                                      &length,
                                      (char*)log.get()));
            GrPrintf((char*)log.get());
        }
        GrAssert(!"Error linking program");
        GL_CALL(DeleteProgram(fProgramID));
        fProgramID = 0;
        return false;
    }
    return true;
}

void GrGLProgram::initSamplerUniforms() {
    GL_CALL(UseProgram(fProgramID));
    // We simply bind the uniforms to successive texture units beginning at 0. setData() assumes
    // this behavior.
    GrGLint texUnitIdx = 0;
    if (GrGLUniformManager::kInvalidUniformHandle != fUniformHandles.fDstCopySamplerUni) {
        fUniformManager.setSampler(fUniformHandles.fDstCopySamplerUni, texUnitIdx);
        ++texUnitIdx;
    }

    for (int s = 0; s < GrDrawState::kNumStages; ++s) {
        int numSamplers = fUniformHandles.fEffectSamplerUnis[s].count();
        for (int u = 0; u < numSamplers; ++u) {
            UniformHandle handle = fUniformHandles.fEffectSamplerUnis[s][u];
            if (GrGLUniformManager::kInvalidUniformHandle != handle) {
                fUniformManager.setSampler(handle, texUnitIdx);
                ++texUnitIdx;
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////

void GrGLProgram::setData(GrGpuGL* gpu,
                          GrColor color,
                          GrColor coverage,
                          const GrDeviceCoordTexture* dstCopy,
                          SharedGLState* sharedState) {
    const GrDrawState& drawState = gpu->getDrawState();

    this->setColor(drawState, color, sharedState);
    this->setCoverage(drawState, coverage, sharedState);
    this->setMatrixAndRenderTargetHeight(drawState);

    // Setup the SkXfermode::Mode-based colorfilter uniform if necessary
    if (GrGLUniformManager::kInvalidUniformHandle != fUniformHandles.fColorFilterUni &&
        fColorFilterColor != drawState.getColorFilterColor()) {
        GrGLfloat c[4];
        GrColorToRGBAFloat(drawState.getColorFilterColor(), c);
        fUniformManager.set4fv(fUniformHandles.fColorFilterUni, 0, 1, c);
        fColorFilterColor = drawState.getColorFilterColor();
    }

    GrGLint texUnitIdx = 0;
    if (NULL != dstCopy) {
        if (GrGLUniformManager::kInvalidUniformHandle != fUniformHandles.fDstCopyTopLeftUni) {
            GrAssert(GrGLUniformManager::kInvalidUniformHandle != fUniformHandles.fDstCopyScaleUni);
            GrAssert(GrGLUniformManager::kInvalidUniformHandle !=
                     fUniformHandles.fDstCopySamplerUni);
            fUniformManager.set2f(fUniformHandles.fDstCopyTopLeftUni,
                                  static_cast<GrGLfloat>(dstCopy->offset().fX),
                                  static_cast<GrGLfloat>(dstCopy->offset().fY));
            fUniformManager.set2f(fUniformHandles.fDstCopyScaleUni,
                                  1.f / dstCopy->texture()->width(),
                                  1.f / dstCopy->texture()->height());
            GrGLTexture* texture = static_cast<GrGLTexture*>(dstCopy->texture());
            static GrTextureParams kParams; // the default is clamp, nearest filtering.
            gpu->bindTexture(texUnitIdx, kParams, texture);
            ++texUnitIdx;
        } else {
            GrAssert(GrGLUniformManager::kInvalidUniformHandle ==
                    fUniformHandles.fDstCopyScaleUni);
            GrAssert(GrGLUniformManager::kInvalidUniformHandle ==
                    fUniformHandles.fDstCopySamplerUni);
        }
    } else {
        GrAssert(GrGLUniformManager::kInvalidUniformHandle ==
                    fUniformHandles.fDstCopyTopLeftUni);
        GrAssert(GrGLUniformManager::kInvalidUniformHandle ==
                    fUniformHandles.fDstCopyScaleUni);
        GrAssert(GrGLUniformManager::kInvalidUniformHandle ==
                    fUniformHandles.fDstCopySamplerUni);
    }
    for (int s = 0; s < GrDrawState::kNumStages; ++s) {
        if (NULL != fEffects[s]) {
            const GrEffectStage& stage = drawState.getStage(s);
            GrAssert(NULL != stage.getEffect());

            bool explicitLocalCoords = -1 != fDesc.fLocalCoordAttributeIndex;
            GrDrawEffect drawEffect(stage, explicitLocalCoords);
            fEffects[s]->setData(fUniformManager, drawEffect);
            int numSamplers = fUniformHandles.fEffectSamplerUnis[s].count();
            for (int u = 0; u < numSamplers; ++u) {
                UniformHandle handle = fUniformHandles.fEffectSamplerUnis[s][u];
                if (GrGLUniformManager::kInvalidUniformHandle != handle) {
                    const GrTextureAccess& access = (*stage.getEffect())->textureAccess(u);
                    GrGLTexture* texture = static_cast<GrGLTexture*>(access.getTexture());
                    gpu->bindTexture(texUnitIdx, access.getParams(), texture);
                    ++texUnitIdx;
                }
            }
        }
    }
}

void GrGLProgram::setColor(const GrDrawState& drawState,
                           GrColor color,
                           SharedGLState* sharedState) {
    if (!drawState.hasColorVertexAttribute()) {
        switch (fDesc.fColorInput) {
            case GrGLProgramDesc::kAttribute_ColorInput:
                GrAssert(-1 != fDesc.fColorAttributeIndex);
                if (sharedState->fConstAttribColor != color ||
                    sharedState->fConstAttribColorIndex != fDesc.fColorAttributeIndex) {
                    // OpenGL ES only supports the float varieties of glVertexAttrib
                    GrGLfloat c[4];
                    GrColorToRGBAFloat(color, c);
                    GL_CALL(VertexAttrib4fv(fDesc.fColorAttributeIndex, c));
                    sharedState->fConstAttribColor = color;
                    sharedState->fConstAttribColorIndex = fDesc.fColorAttributeIndex;
                }
                break;
            case GrGLProgramDesc::kUniform_ColorInput:
                if (fColor != color) {
                    // OpenGL ES doesn't support unsigned byte varieties of glUniform
                    GrGLfloat c[4];
                    GrColorToRGBAFloat(color, c);
                    GrAssert(GrGLUniformManager::kInvalidUniformHandle !=
                             fUniformHandles.fColorUni);
                    fUniformManager.set4fv(fUniformHandles.fColorUni, 0, 1, c);
                    fColor = color;
                }
                sharedState->fConstAttribColorIndex = -1;
                break;
            case GrGLProgramDesc::kSolidWhite_ColorInput:
            case GrGLProgramDesc::kTransBlack_ColorInput:
                sharedState->fConstAttribColorIndex = -1;
                break;
            default:
                GrCrash("Unknown color type.");
        }
    } else {
        sharedState->fConstAttribColorIndex = -1;
    }
}

void GrGLProgram::setCoverage(const GrDrawState& drawState,
                              GrColor coverage,
                              SharedGLState* sharedState) {
    if (!drawState.hasCoverageVertexAttribute()) {
        switch (fDesc.fCoverageInput) {
            case GrGLProgramDesc::kAttribute_ColorInput:
                if (sharedState->fConstAttribCoverage != coverage ||
                    sharedState->fConstAttribCoverageIndex != fDesc.fCoverageAttributeIndex) {
                    // OpenGL ES only supports the float varieties of  glVertexAttrib
                    GrGLfloat c[4];
                    GrColorToRGBAFloat(coverage, c);
                    GL_CALL(VertexAttrib4fv(fDesc.fCoverageAttributeIndex, c));
                    sharedState->fConstAttribCoverage = coverage;
                    sharedState->fConstAttribCoverageIndex = fDesc.fCoverageAttributeIndex;
                }
                break;
            case GrGLProgramDesc::kUniform_ColorInput:
                if (fCoverage != coverage) {
                    // OpenGL ES doesn't support unsigned byte varieties of glUniform
                    GrGLfloat c[4];
                    GrColorToRGBAFloat(coverage, c);
                    GrAssert(GrGLUniformManager::kInvalidUniformHandle !=
                             fUniformHandles.fCoverageUni);
                    fUniformManager.set4fv(fUniformHandles.fCoverageUni, 0, 1, c);
                    fCoverage = coverage;
                }
                sharedState->fConstAttribCoverageIndex = -1;
                break;
            case GrGLProgramDesc::kSolidWhite_ColorInput:
            case GrGLProgramDesc::kTransBlack_ColorInput:
                sharedState->fConstAttribCoverageIndex = -1;
                break;
            default:
                GrCrash("Unknown coverage type.");
        }
    } else {
        sharedState->fConstAttribCoverageIndex = -1;
    }
}

void GrGLProgram::setMatrixAndRenderTargetHeight(const GrDrawState& drawState) {
    const GrRenderTarget* rt = drawState.getRenderTarget();
    SkISize size;
    size.set(rt->width(), rt->height());

    // Load the RT height uniform if it is needed to y-flip gl_FragCoord.
    if (GrGLUniformManager::kInvalidUniformHandle != fUniformHandles.fRTHeightUni &&
        fMatrixState.fRenderTargetSize.fHeight != size.fHeight) {
        fUniformManager.set1f(fUniformHandles.fRTHeightUni, SkIntToScalar(size.fHeight));
    }

    if (fMatrixState.fRenderTargetOrigin != rt->origin() ||
        !fMatrixState.fViewMatrix.cheapEqualTo(drawState.getViewMatrix()) ||
        fMatrixState.fRenderTargetSize != size) {
        SkMatrix m;
        if (kBottomLeft_GrSurfaceOrigin == rt->origin()) {
            m.setAll(
                SkIntToScalar(2) / size.fWidth, 0, -SK_Scalar1,
                0,-SkIntToScalar(2) / size.fHeight, SK_Scalar1,
            0, 0, SkMatrix::I()[8]);
        } else {
            m.setAll(
                SkIntToScalar(2) / size.fWidth, 0, -SK_Scalar1,
                0, SkIntToScalar(2) / size.fHeight,-SK_Scalar1,
            0, 0, SkMatrix::I()[8]);
        }
        m.setConcat(m, drawState.getViewMatrix());

        // ES doesn't allow you to pass true to the transpose param so we do our own transpose.
        GrGLfloat mt[]  = {
            SkScalarToFloat(m[SkMatrix::kMScaleX]),
            SkScalarToFloat(m[SkMatrix::kMSkewY]),
            SkScalarToFloat(m[SkMatrix::kMPersp0]),
            SkScalarToFloat(m[SkMatrix::kMSkewX]),
            SkScalarToFloat(m[SkMatrix::kMScaleY]),
            SkScalarToFloat(m[SkMatrix::kMPersp1]),
            SkScalarToFloat(m[SkMatrix::kMTransX]),
            SkScalarToFloat(m[SkMatrix::kMTransY]),
            SkScalarToFloat(m[SkMatrix::kMPersp2])
        };
        fUniformManager.setMatrix3f(fUniformHandles.fViewMatrixUni, mt);
        fMatrixState.fViewMatrix = drawState.getViewMatrix();
        fMatrixState.fRenderTargetSize = size;
        fMatrixState.fRenderTargetOrigin = rt->origin();
    }
}
