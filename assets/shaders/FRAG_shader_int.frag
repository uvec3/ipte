#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding=0) uniform Params
    {
        bool mand;
        bool gradation;
        vec2 c;
        vec2 move;
        float zoom;
        int accuracy;
        int width;
        int height;
    } params;



layout(location = 0) out vec4 outColor;


void main()
{

//    dvec2 point= (gl_FragCoord.xy)/params.zoom/1000-params.move;
    vec2 point_f= ((gl_FragCoord.xy-vec2(params.width/2,params.height/2))/params.height*4)/params.zoom-params.move;

    ivec2 point=ivec2(point_f);


    int i=0;
    if (params.mand)
        {
        vec2 sp=vec2(0,0);
	    while(((sp.x*sp.x+sp.y*sp.y)<4.0)&&i<=params.accuracy)
        {
		    sp=vec2(sp.x*sp.x-sp.y*sp.y,2.0*sp.x*sp.y)+point;
            ++i;
         }
     }
     else
        {
             while(((point.x*point.x+point.y*point.y)<4.0)&&i<=params.accuracy)
             {
		        point=vec2(point.x*point.x-point.y*point.y,2.0*point.x*point.y)+params.c;
                ++i;
             }
        }
    

    float intesive;

    if(params.gradation)
    {
        intesive=i/float(params.accuracy);
        outColor = vec4(1,intesive,1-intesive,0);
    }
    else if(i>=params.accuracy)
        outColor = vec4(0,0,0,0);
    else
        outColor = vec4(0,0,1,0);

}
