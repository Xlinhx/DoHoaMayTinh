#version 330 core
in vec3 worldPosition, worldNormal;
in vec2 uv;
in vec4 lightPosition;
out vec4 fragColor;
uniform vec4 baseColor;
uniform vec3 emission, eye;
uniform vec3 studioBackground;
uniform float roughness, metallic;
uniform sampler2D colorTexture, shadowTexture;
uniform bool hasTexture, enableShadow, enableLight, isGlass, isStudio;
float shadowAmount(vec3 n, vec3 l) {
    vec3 p = lightPosition.xyz / lightPosition.w * .5 + .5;
    if (p.z > 1.0 || p.z < 0.0 || any(lessThan(p.xy, vec2(0))) || any(greaterThan(p.xy, vec2(1))))
        return 0.0;
    float bias = max(.0007 * (1.0 - dot(n, l)), .00015), s = 0.0;
    vec2 stepSize = 1.0 / vec2(textureSize(shadowTexture, 0));
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
            s += p.z - bias > texture(shadowTexture, p.xy + vec2(x, y) * stepSize).r ? 1.0 : 0.0;
    return s / 9.0;
}
vec3 filmic(vec3 x) {
    return clamp((x * (2.51 * x + .03)) / (x * (2.43 * x + .59) + .14), 0.0, 1.0);
}
void main() {
    vec4 base = baseColor * (hasTexture ? texture(colorTexture, uv) : vec4(1));
    vec3 n = normalize(worldNormal);
    if (isStudio) {
        // The authoring stage is a thick box. Only its top is a runtime shadow
        // catcher; its sides/underside must never become a wall across the view.
        if (n.y < .5 || !gl_FrontFacing)
            discard;
        vec3 lightDirection = normalize(vec3(3, 5, 4));
        float shadow = enableShadow ? shadowAmount(n, lightDirection) : 0.0;
        float grazing = smoothstep(0.0, .12, dot(n, normalize(eye - worldPosition)));
        float localFade = 1.0 - smoothstep(1.5, 3.0, length(worldPosition.xz));
        // This display-space color is shared with glClearColor, so there is no
        // differently lit plane or abrupt background switch at the horizon.
        fragColor = vec4(studioBackground * (1.0 - .24 * shadow * grazing * localFade), 1.0);
        return;
    }
    if (!gl_FrontFacing)
        n = -n;
    vec3 v = normalize(eye - worldPosition), l = normalize(vec3(3, 5, 4));
    float ndl = max(dot(n, l), 0), s = enableShadow ? shadowAmount(n, l) : 0;
    vec3 fill = normalize(vec3(-3, 2, 1));
    float ambient = .39 + .13 * max(n.y, 0.0);
    vec3 diffuse = base.rgb * (ambient + .65 * ndl * (1.0 - .68 * s) + .24 * max(dot(n, fill), 0));
    float shininess = mix(140.0, 7.0, roughness);
    vec3 specular = mix(vec3(.08), base.rgb, metallic) * pow(max(dot(n, normalize(l + v)), 0), shininess) *
                    (1.0 - .8 * s) * (1.0 - roughness * .65);
    vec3 color = enableLight ? filmic(diffuse + specular + emission * .45) : base.rgb;
    float alpha = base.a;
    if (isGlass) {
        float fresnel = pow(1.0 - max(dot(n, v), 0), 4.0);
        color = mix(vec3(.71, .84, .80), vec3(.98), fresnel);
        alpha = base.a + fresnel * .11;
    }
    fragColor = vec4(pow(max(color, vec3(0)), vec3(1.0 / 2.2)), alpha);
}
