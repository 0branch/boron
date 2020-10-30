// These are intermediate values used before dp_compile emits DPOpcode values.
enum DrawOperation
{
    DOP_NOP,
    DOP_END,
    DOP_CLEAR,
    DOP_ENABLE,
    DOP_DISABLE,
    DOP_CALL,
    DOP_IMAGE,
    DOP_PARTICLE,
    DOP_COLOR,
    DOP_VERTEX_ARRAY,
    DOP_POINTS,
    DOP_LINES,
    DOP_LINE_STRIP,
    DOP_TRIS,
    DOP_TRI_STRIP,
    DOP_TRI_FAN,
    DOP_QUADS,
    DOP_QUAD_STRIP,
    DOP_TRIS_INST,
    DOP_INSTANCED_PARTS,
    DOP_SPHERE,
    DOP_BOX,
    DOP_QUAD,
    DOP_CAMERA,
    DOP_PUSH_MODEL,
    DOP_VIEW_UNIFORM,
    DOP_PUSH_MUL,
    DOP_PUSH,
    DOP_POP,
    DOP_TRANSLATE,
    DOP_ROTATE,
    DOP_SCALE,
    DOP_FONT,
    DOP_TEXT,
    DOP_SHADER,
    DOP_UNIFORM,
    DOP_FRAMEBUFFER,
    DOP_FRAMEBUFFER_TEX,
    DOP_SHADOW_BEGIN,
    DOP_SHADOW_END,
    DOP_SAMPLES_QUERY,
    DOP_SAMPLES_BEGIN,
    DOP_BUFFER,
    DOP_BUFFER_INST,
    DOP_DEPTH_TEST,
    DOP_BLEND,
    DOP_CULL,
    DOP_COLOR_MASK,
    DOP_DEPTH_MASK,
    DOP_POINT_SIZE,
    DOP_READ_PIXELS,
    DOP_COUNT
};
