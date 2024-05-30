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
    float brightness ;
    float t ;
    float ringSize;
};

static float2 uv=float2(0,0);
static float4 primitivesColor=1;

cbuffer ParamBuffer : register(b0)
{
    Params params;
};


float random (float2 st)
{
    return frac(sin(dot(st.xy,
                         float2(12.9898,78.233)))
                 * 43758.5453123);
}

void applyColor(float4 color)
{
    primitivesColor=primitivesColor*(1-color.a)+color*color.a;
}

void drawLine(float2 p1, float2 p2,float w, float4 color= 1)
{
	float2 n=normalize(p2-p1);
	float len=length(p2-p1);
	float2 pn= float2(-n.y,n.x);

	float d=dot(uv-p1,pn);
	float res=smoothstep(-w,0,d)-smoothstep(0,w,d);

	d=dot(uv-p1,normalize(p2-p1));
	res*=step(0,d)-step(len,d);
	applyColor(res*color);
}

void grid()
{
    float4 color=float4(1,0,1,0.4);
    float w=0.01;

    float2 fr= frac(uv);
    float res=step(-w,fr.x)-step(w,fr.x);
    res+=step(-w,fr.y)-step(w,fr.y);
    applyColor(res*color);
}



float noise(float2 uv)
{
    float density=params.accuracy;
    int2 p=floor(uv*density);
    float2 f=frac(uv*density);
    f= f*f*(3.0-2.0*f);

    float r=random(p);
    float rx=random(p+int2(1,0));
    float ry=random(p+int2(0,1));
    float rxy=random(p+int2(1,1));


    float res=lerp(lerp(r,rx,f.x), lerp(ry,rxy,f.x),f.y);
    return res;
}

float doubleNoise(float2 uv)
{
    float lowerScale=0.3;
    float lowerFrequency=5;


    return noise(uv)+noise((uv+12)*lowerFrequency)*lowerScale;
}

void circle(float2 pos, float r, float4 color=float4(1,0,0,1))
{
    float2 d=uv-pos;
    float d_sqr=dot(d,d);
     applyColor(color*(1-step(r*r,d_sqr)));
}

float4 main(float4 position) : SV_TARGET
{
    uv = float2(position.x/params.height*params.width, position.y)*2 / params.zoom - params.move;
    uv.y=-uv.y;

    float t=params.t/100;

	applyColor(noise(uv));

    grid();
    return primitivesColor;
}