//glsl version
#version 450
//???
#extension GL_ARB_separate_shader_objects : enable


vec2 positions[4] = vec2[](
    vec2(-1,-1),
    vec2(1, -1),
    vec2(-1, 1),
    vec2(1, 1)
);

layout(location = 0) out vec4 positionX;
//main called for each vertex
void main() 
{
	//gl_VertexIndex -- current vertex index
	//gl_Position -- built-in output variable( output vertex)
    position = vec4(positions[gl_VertexIndex], 0, 1);
    gl_Position =vec4(positions[gl_VertexIndex], 0, 1);
}
