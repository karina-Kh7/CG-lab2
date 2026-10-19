#version 450
layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inColor;
layout(location=2) in vec3 inNormal;
layout(location=0) out vec3 worldPosition;
layout(location=1) out vec3 worldNormal;
layout(location=2) out vec3 vertexColor;
layout(std140,set=0,binding=0) uniform Scene {
    mat4 viewProjection;
    vec4 cameraPosition;
    vec4 directionalDirection;
    vec4 directionalColorIntensity;
    vec4 ambientColorIntensity;
    ivec4 lightCounts;
} scene;
layout(std140,set=1,binding=0) uniform Model {
    mat4 model;
    vec4 albedo;
    vec4 specular;
    mat4 normalMatrix;
} material;
void main(){
    vec4 p=material.model*vec4(inPosition,1.0);
    worldPosition=p.xyz;
    worldNormal=normalize(mat3(material.normalMatrix)*inNormal);
    vertexColor=inColor;
    gl_Position=scene.viewProjection*p;
}
