/* FXAA (after Timothy Lottes' FXAA 2) as a pixel shader (HLSL), from the Direct3D 11 renderer of ZiNc EX */
R"HLSL(
/* ---------------------------------------------------------------- FXAA (after Timothy Lottes' FXAA 2: edge detection by luma and a blur along the edge) */
float fxLuma(float3 c) { return dot(c, float3(0.299, 0.587, 0.114)); }
float4 ps_fxaa(DSO v) : SV_Target {
    float2 px = ss.zw;   /* one pixel in uv units */
    float3 nw = srcTex.SampleLevel(smp, v.uv + float2(-1.0, -1.0) * px, 0).rgb;
    float3 ne = srcTex.SampleLevel(smp, v.uv + float2( 1.0, -1.0) * px, 0).rgb;
    float3 sw = srcTex.SampleLevel(smp, v.uv + float2(-1.0,  1.0) * px, 0).rgb;
    float3 se = srcTex.SampleLevel(smp, v.uv + float2( 1.0,  1.0) * px, 0).rgb;
    float3 m  = srcTex.SampleLevel(smp, v.uv, 0).rgb;
    float lNW = fxLuma(nw), lNE = fxLuma(ne), lSW = fxLuma(sw), lSE = fxLuma(se), lM = fxLuma(m);
    float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
    float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));
    float2 dir;
    dir.x = -((lNW + lNE) - (lSW + lSE));
    dir.y =  ((lNW + lSW) - (lNE + lSE));
    float dirReduce = max((lNW + lNE + lSW + lSE) * (0.25 * (1.0 / 8.0)), 1.0 / 128.0);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(float2(8.0, 8.0), max(float2(-8.0, -8.0), dir * rcpDirMin)) * px;
    float3 rgbA = 0.5 * (srcTex.SampleLevel(smp, v.uv + dir * (1.0 / 3.0 - 0.5), 0).rgb + srcTex.SampleLevel(smp, v.uv + dir * (2.0 / 3.0 - 0.5), 0).rgb);
    float3 rgbB = rgbA * 0.5 + 0.25 * (srcTex.SampleLevel(smp, v.uv + dir * -0.5, 0).rgb + srcTex.SampleLevel(smp, v.uv + dir * 0.5, 0).rgb);
    float lB = fxLuma(rgbB);
    return float4((lB < lMin || lB > lMax) ? rgbA : rgbB, 1.0);
}

)HLSL"
