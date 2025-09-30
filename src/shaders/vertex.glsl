#version 330 core
out vec2 uv;
const vec2 verts[4] = vec2[](
    vec2(-1, -1), vec2(1, -1),
    vec2(-1,  1), vec2(1,  1)
);
void main() {
    gl_Position = vec4(verts[gl_VertexID], 0.0, 1.0);
    uv = (verts[gl_VertexID] + 1.0) * 0.5;
}
