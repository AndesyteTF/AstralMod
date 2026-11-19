float4 screen_time_opacity : register(c0); // width, height, time, opacity
float4 menu_bounds         : register(c1); // left, top, right, bottom (pixels)
float4 accent_colour       : register(c2); // rgb, glow strength

float hash21(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}

float4 astral(float2 uv : TEXCOORD0) : COLOR0
{
    float2 resolution = screen_time_opacity.xy;
    float time = screen_time_opacity.z;
    float opacity = screen_time_opacity.w;
    float2 pixel = uv * resolution;

    float2 field = (pixel - resolution * 0.5) / max(resolution.y, 1.0);
    float wave = sin(field.x * 8.0 + time * 0.18) +
                 sin(field.y * 11.0 - time * 0.13) +
                 sin((field.x + field.y) * 7.0 + time * 0.09);
    float nebula = saturate(wave * 0.13 + 0.34);
    nebula *= nebula;

    float2 star_cell = floor(pixel / 5.0);
    float star_seed = hash21(star_cell);
    float star = step(0.997, star_seed);
    star *= 0.45 + 0.55 * sin(time * (0.7 + star_seed) + star_seed * 21.0);

    float2 centre = (menu_bounds.xy + menu_bounds.zw) * 0.5;
    float2 half_size = max((menu_bounds.zw - menu_bounds.xy) * 0.5, 1.0);
    float2 q = abs(pixel - centre) - half_size;
    float outside = length(max(q, 0.0));
    float inside = step(max(q.x, q.y), 0.0);
    float glow = exp2(-outside * 0.045) * (1.0 - inside);
    glow += exp2(-outside * 0.012) * (1.0 - inside) * 0.18;

    float3 deep_space = float3(0.015, 0.025, 0.075);
    float3 nebula_colour = lerp(float3(0.05, 0.025, 0.16), accent_colour.rgb * 0.55, nebula);
    float3 colour = deep_space + nebula_colour * nebula * 0.55;
    colour += star * lerp(float3(0.65, 0.76, 1.0), accent_colour.rgb, star_seed) * 0.8;
    colour += accent_colour.rgb * glow * accent_colour.w;

    float alpha = opacity * saturate(0.10 + nebula * 0.10 + star * 0.28 + glow * 0.72);
    return float4(colour, alpha);
}
