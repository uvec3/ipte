struct Params
{
    bool mand;
    bool gradation;
    float2 c;
    float2 move;
    float zoom;
    int accuracy;
    int width;
    int height;
};

cbuffer ParamBuffer : register(b0)
{
    Params params;
};

float4 main(float4 position) : SV_TARGET
{
    float2 uv = float2(position.x/params.height*params.width, position.y)*2 / params.zoom - params.move;


    int i = 0;
    if (params.mand)
    {
        float2 sp = float2(0, 0);
        while (((sp.x * sp.x + sp.y * sp.y) < 4.0) && i <= params.accuracy)
        {
            sp = float2(sp.x * sp.x - sp.y * sp.y, 2.0 * sp.x * sp.y) + uv;
            ++i;
        }
    }
    else
    {
        while (((uv.x * uv.x + uv.y * uv.y) < 4.0) && i <= params.accuracy)
        {
            uv = float2(uv.x * uv.x - uv.y * uv.y, 2.0 * uv.x * uv.y) + params.c;
            ++i;
        }
    }

    float intensity;

    if (params.gradation)
    {
        intensity = i / float(params.accuracy);
        return float4(0, intensity, 1 - intensity, 0);
    }
    else if (i >= params.accuracy)
        return float4(0, 0, 0, 0);
    else
        return position;
}