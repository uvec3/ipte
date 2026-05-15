static const float2 corners[3]={float2(-1.0,3.0), float2(3.0, -1.0), float2(-1.0, -1.0)};

struct VS_OUTPUT {
    [[vk::location(0)]] float2 position:TEXCOORD0;
    float4 sv_pos : SV_Position;
};
VS_OUTPUT main(uint vID : SV_VertexID)
{
    VS_OUTPUT out;
    out.position=corners[gl_VertexIndex];
    out.sv_pos=float4(corners[gl_VertexIndex],0,1);
    return out;
}