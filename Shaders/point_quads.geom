#version 410

layout(points) in;
//layout(triangle_strip, max_vertices = 24) out;
layout(triangle_strip, max_vertices = 4) out;

flat in lowp vec3 gColor[];
flat in int gFaceIdx[];

flat out lowp vec3 fColor;

uniform float voxSize = 0.25;
uniform mat4 mvp;

void AddQuad(vec4 center, vec4 dy, vec4 dx) {
    fColor = gColor[0];
    gl_Position = center + (dx - dy);
    EmitVertex();

    fColor = gColor[0];
    gl_Position = center + (-dx - dy);
    EmitVertex();

    fColor = gColor[0];
    gl_Position = center + (dx + dy);
    EmitVertex();

    fColor = gColor[0];
    gl_Position = center + (-dx + dy);
    EmitVertex();

    //EndPrimitive();
}

void main() {
    const vec3 dxs[6] = vec3[6](
        vec3(0, 1, 0),
        vec3(0, 0, 1),
        vec3(0, 0, 1),
        vec3(1, 0, 0),
        vec3(1, 0, 0),
        vec3(0, 1, 0)
    );

    const vec3 dys[6] = vec3[6](
        vec3(0, 0, 1),
        vec3(0, 1, 0),
        vec3(1, 0, 0),
        vec3(0, 0, 1),
        vec3(0, 1, 0),
        vec3(1, 0, 0)
    );

    vec4 center = gl_in[0].gl_Position;
    AddQuad(center, 
        mvp * vec4(dxs[gFaceIdx[0]] / 2.0f * voxSize, 0),
        mvp * vec4(dys[gFaceIdx[0]] / 2.0f * voxSize, 0));
}
