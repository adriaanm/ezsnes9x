#include <metal_stdlib>
using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float2 texCoord;
};

struct TexScale {
    float scaleX;
    float scaleY;
};

vertex VertexOut vertexShader(uint vid [[vertex_id]],
                               constant TexScale &scale [[buffer(0)]]) {
    float2 positions[] = {
        float2(-1, -1), float2( 1, -1), float2(-1,  1),
        float2(-1,  1), float2( 1, -1), float2( 1,  1)
    };
    float2 texCoords[] = {
        float2(0, 1), float2(1, 1), float2(0, 0),
        float2(0, 0), float2(1, 1), float2(1, 0)
    };
    VertexOut out;
    out.position = float4(positions[vid], 0, 1);
    out.texCoord = texCoords[vid] * float2(scale.scaleX, scale.scaleY);
    return out;
}

fragment float4 fragmentShader(VertexOut in [[stage_in]],
                               texture2d<float> tex [[texture(0)]],
                               sampler samp [[sampler(0)]]) {
    return tex.sample(samp, in.texCoord);
}

// Overlay shader for solid color rectangles (rewind progress bar)
struct ColorVertex {
    float2 position [[attribute(0)]];
    float4 color [[attribute(1)]];
};

struct ColorVertexOut {
    float4 position [[position]];
    float4 color;
};

vertex ColorVertexOut colorVertexShader(uint vid [[vertex_id]],
                                         constant ColorVertex *vertices [[buffer(0)]]) {
    ColorVertexOut out;
    out.position = float4(vertices[vid].position, 0, 1);
    out.color = vertices[vid].color;
    return out;
}

fragment float4 colorFragmentShader(ColorVertexOut in [[stage_in]]) {
    return in.color;
}
