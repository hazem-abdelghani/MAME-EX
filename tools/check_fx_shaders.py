"""Checks the HLSL of the filter window (src/filter.cpp, fx_xbrz.inc, fx_fxaa.inc) for errors without Windows: python3 -m pip install slangpy; python3 tools/check_fx_shaders.py"""
import os
SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src")
import re, sys
import slangpy as spy
s=open(SRC+'/filter.cpp').read()
def raw(name):
    return re.search(r'static const char \*'+name+r' = R"HLSL\((.*?)\)HLSL"',s,re.S).group(1)
def inc(f):
    return re.search(r'R"HLSL\((.*)\)HLSL"',open(SRC+'/'+f).read(),re.S).group(1)
head=raw('kHlslHead'); xb=inc('fx_xbrz.inc'); fx=inc('fx_fxaa.inc')
tail='\nfloat4 ps_xbrz(DSO v) : SV_Target { return float4(xbrz(v.uv).rgb, 1.0); }\n'
src=head+xb+tail+fx
dev = spy.Device(type=spy.DeviceType.cpu, compiler_options=spy.SlangCompilerOptions({}))
try:
    dev.load_module_from_source("fx", src)
    print("MODULE OK")
except Exception as e:
    msg=str(e)
    errs=re.findall(r'error\[E\d+\]: ([^\n]+)\n\s+--> [^:]+:(\d+):(\d+)',msg)
    lines=src.split('\n')
    seen=set()
    for t,l,c in errs:
        if (t,l) in seen: continue
        seen.add((t,l)); print(t,'| line',l,':',lines[int(l)-1].strip()[:110])
