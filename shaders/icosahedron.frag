#version 450
layout(location=0) in vec3 worldPosition;
layout(location=1) in vec3 worldNormal;
layout(location=2) in vec3 vertexColor;
layout(location=0) out vec4 outColor;
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
struct PointLight { vec4 position; vec4 colorIntensity; };
struct SpotLight { vec4 position; vec4 colorIntensity; vec4 directionInnerCos; vec4 outerCosPadding; };
// Массивы в шейдере без фиксированного размера (storage buffers, std430).
layout(std430,set=0,binding=1) readonly buffer Points { PointLight lights[]; } points;
layout(std430,set=0,binding=2) readonly buffer Spots { SpotLight lights[]; } spots;
vec3 N,V,baseColor;
// Диффузная и зеркальная составляющие Блинна-Фонга.
vec3 blinnPhong(vec3 L,vec3 lightColor){
    float diffuse=max(dot(N,L),0.0);
    if(diffuse<=0.0)return vec3(0.0);
    vec3 H=normalize(L+V);
    float spec=pow(max(dot(N,H),0.0),material.specular.w);
    return lightColor*(baseColor*diffuse+material.specular.rgb*spec);
}
void main(){
    N=normalize(worldNormal);
    V=normalize(scene.cameraPosition.xyz-worldPosition);
    baseColor=material.albedo.rgb*vertexColor;
    vec3 result=scene.ambientColorIntensity.rgb*scene.ambientColorIntensity.a*baseColor;
    vec3 direction=-scene.directionalDirection.xyz;
    if(dot(direction,direction)>0.00001)
        result+=blinnPhong(normalize(direction),
            scene.directionalColorIntensity.rgb*scene.directionalColorIntensity.a);
    for(int i=0;i<scene.lightCounts.x;++i){
        vec3 delta=points.lights[i].position.xyz-worldPosition;
        float d2=max(dot(delta,delta),0.01);
        vec3 color=points.lights[i].colorIntensity.rgb*points.lights[i].colorIntensity.a/d2;
        result+=blinnPhong(normalize(delta),color);
    }
    for(int i=0;i<scene.lightCounts.y;++i){
        vec3 delta=spots.lights[i].position.xyz-worldPosition;
        float d2=max(dot(delta,delta),0.01);
        vec3 dir=spots.lights[i].directionInnerCos.xyz;
        if(dot(dir,dir)<0.00001)continue;
        // cos угла между осью прожектора и направлением к освещаемой точке.
        float theta=dot(normalize(-delta),normalize(dir));
        float inner=spots.lights[i].directionInnerCos.w;
        float outer=spots.lights[i].outerCosPadding.x;
        float softness=clamp((theta-outer)/max(inner-outer,0.00001),0.0,1.0);
        vec3 color=spots.lights[i].colorIntensity.rgb*spots.lights[i].colorIntensity.a*softness/d2;
        result+=blinnPhong(normalize(delta),color);
    }
    outColor=vec4(result,1.0);
}
