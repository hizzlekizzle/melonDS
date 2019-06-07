/*
    Copyright 2016-2019 Arisotura

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#ifndef GPU3D_OPENGL_SHADERS_H
#define GPU3D_OPENGL_SHADERS_H

#define kShaderHeader "#version 140"


const char* kClearVS = kShaderHeader R"(

in vec2 vPosition;

uniform uint uDepth;

void main()
{
    float fdepth = (float(uDepth) / 8388608.0) - 1.0;
    gl_Position = vec4(vPosition, fdepth, 1.0);
}
)";

const char* kClearFS = kShaderHeader R"(

uniform uvec4 uColor;
uniform uint uOpaquePolyID;
uniform uint uFogFlag;

out vec4 oColor;
out vec4 oAttr;

void main()
{
    oColor = vec4(uColor).bgra / 31.0;
    oAttr.r = float(uOpaquePolyID) / 63.0;
    oAttr.g = 0;
    oAttr.b = float(uFogFlag);
    oAttr.a = 1;
}
)";



const char* kFinalPassVS = kShaderHeader R"(

in vec2 vPosition;

void main()
{
    // heh
    gl_Position = vec4(vPosition, 0.0, 1.0);
}
)";

const char* kFinalPassFS = kShaderHeader R"(

uniform sampler2D DepthBuffer;
uniform sampler2D AttrBuffer;

layout(std140) uniform uConfig
{
    vec2 uScreenSize;
    int uDispCnt;
    vec4 uToonColors[32];
    vec4 uEdgeColors[8];
    vec4 uFogColor;
    float uFogDensity[34];
    int uFogOffset;
    int uFogShift;
};

out vec4 oColor;

vec4 CalculateFog(float depth)
{
    int idepth = int(depth * 16777216.0);
    int densityid, densityfrac;

    if (idepth < uFogOffset)
    {
        densityid = 0;
        densityfrac = 0;
    }
    else
    {
        uint udepth = uint(idepth);
        udepth -= uint(uFogOffset);
        udepth = (udepth >> 2) << uint(uFogShift);

        densityid = int(udepth >> 17);
        if (densityid >= 32)
        {
            densityid = 32;
            densityfrac = 0;
        }
        else
            densityfrac = int(udepth & uint(0x1FFFF));
    }

    float density = mix(uFogDensity[densityid], uFogDensity[densityid+1], float(densityfrac)/131072.0);

    return vec4(density, density, density, density);
}

void main()
{
    ivec2 coord = ivec2(gl_FragCoord.xy);

    vec4 ret = vec4(0,0,0,0);
    vec4 depth = texelFetch(DepthBuffer, coord, 0);
    vec4 attr = texelFetch(AttrBuffer, coord, 0);

    if (attr.b != 0) ret = CalculateFog(depth.r);

    oColor = ret;
}
)";



const char* kRenderVSCommon = R"(

layout(std140) uniform uConfig
{
    vec2 uScreenSize;
    int uDispCnt;
    vec4 uToonColors[32];
    vec4 uEdgeColors[8];
    vec4 uFogColor;
    float uFogDensity[34];
    int uFogOffset;
    int uFogShift;
};

in uvec4 vPosition;
in uvec4 vColor;
in ivec2 vTexcoord;
in ivec3 vPolygonAttr;

smooth out vec4 fColor;
smooth out vec2 fTexcoord;
flat out ivec3 fPolygonAttr;
)";

const char* kRenderFSCommon = R"(

uniform usampler2D TexMem;
uniform sampler2D TexPalMem;

layout(std140) uniform uConfig
{
    vec2 uScreenSize;
    int uDispCnt;
    vec4 uToonColors[32];
    vec4 uEdgeColors[8];
    vec4 uFogColor;
    float uFogDensity[34];
    int uFogOffset;
    int uFogShift;
};

smooth in vec4 fColor;
smooth in vec2 fTexcoord;
flat in ivec3 fPolygonAttr;

out vec4 oColor;
out vec4 oAttr;

int TexcoordWrap(int c, int maxc, int mode)
{
    if ((mode & (1<<0)) != 0)
    {
        if ((mode & (1<<2)) != 0 && (c & maxc) != 0)
            return (maxc-1) - (c & (maxc-1));
        else
            return (c & (maxc-1));
    }
    else
        return clamp(c, 0, maxc-1);
}

vec4 TextureFetch_A3I5(ivec2 addr, ivec4 st, int wrapmode)
{
    st.x = TexcoordWrap(st.x, st.z, wrapmode>>0);
    st.y = TexcoordWrap(st.y, st.w, wrapmode>>1);

    addr.x += ((st.y * st.z) + st.x);
    ivec4 pixel = ivec4(texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0));

    pixel.a = (pixel.r & 0xE0);
    pixel.a = (pixel.a >> 3) + (pixel.a >> 6);
    pixel.r &= 0x1F;

    addr.y = (addr.y << 3) + pixel.r;
    vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);

    return vec4(color.rgb, float(pixel.a)/31.0);
}

vec4 TextureFetch_I2(ivec2 addr, ivec4 st, int wrapmode, float alpha0)
{
    st.x = TexcoordWrap(st.x, st.z, wrapmode>>0);
    st.y = TexcoordWrap(st.y, st.w, wrapmode>>1);

    addr.x += ((st.y * st.z) + st.x) >> 2;
    ivec4 pixel = ivec4(texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0));
    pixel.r >>= (2 * (st.x & 3));
    pixel.r &= 0x03;

    addr.y = (addr.y << 2) + pixel.r;
    vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);

    return vec4(color.rgb, (pixel.r>0)?1:alpha0);
}

vec4 TextureFetch_I4(ivec2 addr, ivec4 st, int wrapmode, float alpha0)
{
    st.x = TexcoordWrap(st.x, st.z, wrapmode>>0);
    st.y = TexcoordWrap(st.y, st.w, wrapmode>>1);

    addr.x += ((st.y * st.z) + st.x) >> 1;
    ivec4 pixel = ivec4(texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0));
    if ((st.x & 1) != 0) pixel.r >>= 4;
    else                 pixel.r &= 0x0F;

    addr.y = (addr.y << 3) + pixel.r;
    vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);

    return vec4(color.rgb, (pixel.r>0)?1:alpha0);
}

vec4 TextureFetch_I8(ivec2 addr, ivec4 st, int wrapmode, float alpha0)
{
    st.x = TexcoordWrap(st.x, st.z, wrapmode>>0);
    st.y = TexcoordWrap(st.y, st.w, wrapmode>>1);

    addr.x += ((st.y * st.z) + st.x);
    ivec4 pixel = ivec4(texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0));

    addr.y = (addr.y << 3) + pixel.r;
    vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);

    return vec4(color.rgb, (pixel.r>0)?1:alpha0);
}

vec4 TextureFetch_Compressed(ivec2 addr, ivec4 st, int wrapmode)
{
    st.x = TexcoordWrap(st.x, st.z, wrapmode>>0);
    st.y = TexcoordWrap(st.y, st.w, wrapmode>>1);

    addr.x += ((st.y & 0x3FC) * (st.z>>2)) + (st.x & 0x3FC) + (st.y & 0x3);
    ivec4 p = ivec4(texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0));
    int val = (p.r >> (2 * (st.x & 0x3))) & 0x3;

    int slot1addr = 0x20000 + ((addr.x & 0x1FFFC) >> 1);
    if (addr.x >= 0x40000) slot1addr += 0x10000;

    int palinfo;
    p = ivec4(texelFetch(TexMem, ivec2(slot1addr&0x3FF, slot1addr>>10), 0));
    palinfo = p.r;
    slot1addr++;
    p = ivec4(texelFetch(TexMem, ivec2(slot1addr&0x3FF, slot1addr>>10), 0));
    palinfo |= (p.r << 8);

    addr.y = (addr.y << 3) + ((palinfo & 0x3FFF) << 1);
    palinfo >>= 14;

    if (val == 0)
    {
        vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
        return vec4(color.rgb, 1.0);
    }
    else if (val == 1)
    {
        addr.y++;
        vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
        return vec4(color.rgb, 1.0);
    }
    else if (val == 2)
    {
        if (palinfo == 1)
        {
            vec4 color0 = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            addr.y++;
            vec4 color1 = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            return vec4((color0.rgb + color1.rgb) / 2.0, 1.0);
        }
        else if (palinfo == 3)
        {
            vec4 color0 = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            addr.y++;
            vec4 color1 = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            return vec4((color0.rgb*5.0 + color1.rgb*3.0) / 8.0, 1.0);
        }
        else
        {
            addr.y += 2;
            vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            return vec4(color.rgb, 1.0);
        }
    }
    else
    {
        if (palinfo == 2)
        {
            addr.y += 3;
            vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            return vec4(color.rgb, 1.0);
        }
        else if (palinfo == 3)
        {
            vec4 color0 = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            addr.y++;
            vec4 color1 = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);
            return vec4((color0.rgb*3.0 + color1.rgb*5.0) / 8.0, 1.0);
        }
        else
        {
            return vec4(0.0);
        }
    }
}

vec4 TextureFetch_A5I3(ivec2 addr, ivec4 st, int wrapmode)
{
    st.x = TexcoordWrap(st.x, st.z, wrapmode>>0);
    st.y = TexcoordWrap(st.y, st.w, wrapmode>>1);

    addr.x += ((st.y * st.z) + st.x);
    ivec4 pixel = ivec4(texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0));

    pixel.a = (pixel.r & 0xF8) >> 3;
    pixel.r &= 0x07;

    addr.y = (addr.y << 3) + pixel.r;
    vec4 color = texelFetch(TexPalMem, ivec2(addr.y&0x3FF, addr.y>>10), 0);

    return vec4(color.rgb, float(pixel.a)/31.0);
}

vec4 TextureFetch_Direct(ivec2 addr, ivec4 st, int wrapmode)
{
    st.x = TexcoordWrap(st.x, st.z, wrapmode>>0);
    st.y = TexcoordWrap(st.y, st.w, wrapmode>>1);

    addr.x += ((st.y * st.z) + st.x) << 1;
    ivec4 pixelL = ivec4(texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0));
    addr.x++;
    ivec4 pixelH = ivec4(texelFetch(TexMem, ivec2(addr.x&0x3FF, addr.x>>10), 0));

    vec4 color;
    color.r = float(pixelL.r & 0x1F) / 31.0;
    color.g = float((pixelL.r >> 5) | ((pixelH.r & 0x03) << 3)) / 31.0;
    color.b = float((pixelH.r & 0x7C) >> 2) / 31.0;
    color.a = float(pixelH.r >> 7);

    return color;
}

// begin SABR
   /*
      Constants
   */
   /*
      Inequation coefficients for interpolation
   Equations are in the form: Ay + Bx = C
   45, 30, and 60 denote the angle from x each line the cooeficient variable set builds
   */
   const vec4 Ai  = vec4( 1.0, -1.0, -1.0,  1.0);
   const vec4 B45 = vec4( 1.0,  1.0, -1.0, -1.0);
   const vec4 C45 = vec4( 1.5,  0.5, -0.5,  0.5);
   const vec4 B30 = vec4( 0.5,  2.0, -0.5, -2.0);
   const vec4 C30 = vec4( 1.0,  1.0, -0.5,  0.0);
   const vec4 B60 = vec4( 2.0,  0.5, -2.0, -0.5);
   const vec4 C60 = vec4( 2.0,  0.0, -1.0,  0.5);

   const vec4 M45 = vec4(0.4, 0.4, 0.4, 0.4);
   const vec4 M30 = vec4(0.2, 0.4, 0.2, 0.4);
   const vec4 M60 = M30.yxwz;
   const vec4 Mshift = vec4(0.2);

   // Coefficient for weighted edge detection
   const float coef = 2.0;
   // Threshold for if luminance values are "equal"
   const vec4 threshold = vec4(0.32);

   // Conversion from RGB to Luminance (from GIMP)
   const vec3 lum = vec3(0.21, 0.72, 0.07);

   // Performs same logic operation as && for vectors
   bvec4 _and_(bvec4 A, bvec4 B) {
      return bvec4(A.x && B.x, A.y && B.y, A.z && B.z, A.w && B.w);
   }

   // Performs same logic operation as || for vectors
   bvec4 _or_(bvec4 A, bvec4 B) {
      return bvec4(A.x || B.x, A.y || B.y, A.z || B.z, A.w || B.w);
   }

   // Converts 4 3-color vectors into 1 4-value luminance vector
   vec4 lum_to(vec3 v0, vec3 v1, vec3 v2, vec3 v3) {
      return vec4(dot(lum, v0), dot(lum, v1), dot(lum, v2), dot(lum, v3));
   }

   // Gets the difference between 2 4-value luminance vectors
   vec4 lum_df(vec4 A, vec4 B) {
      return abs(A - B);
   }

   // Determines if 2 4-value luminance vectors are "equal" based on threshold
   bvec4 lum_eq(vec4 A, vec4 B) {
      return lessThan(lum_df(A, B), threshold);
   }

   vec4 lum_wd(vec4 a, vec4 b, vec4 c, vec4 d, vec4 e, vec4 f, vec4 g, vec4 h) {
      return lum_df(a, b) + lum_df(a, c) + lum_df(d, e) + lum_df(d, f) + 4.0 * lum_df(g, h);
   }

   // Gets the difference between 2 3-value rgb colors
   float c_df(vec3 c1, vec3 c2) {
      vec3 df = abs(c1 - c2);
      return df.r + df.g + df.b;
   }

   vec4 TextureLookup_Nearest(vec2 st)
   {
       int attr = int(fPolygonAttr.y);
       int paladdr = int(fPolygonAttr.z);
       
       ivec4 xyp_1_2_3    = ivec4(st.xxxy) + ivec4(-1,  0, 1, -2);
       ivec4 xyp_6_7_8    = ivec4(st.xxxy) + ivec4(-1,  0, 1, -1);
       ivec4 xyp_11_12_13 = ivec4(st.xxxy) + ivec4(-1,  0, 1,  0);
       ivec4 xyp_16_17_18 = ivec4(st.xxxy) + ivec4(-1,  0, 1,  1);
       ivec4 xyp_21_22_23 = ivec4(st.xxxy) + ivec4(-1,  0, 1,  2);
       ivec4 xyp_5_10_15  = ivec4(st.xyyy) + ivec4(-2, -1, 0,  1);
       ivec4 xyp_9_14_9   = ivec4(st.xyyy) + ivec4( 2, -1, 0,  1);
       
       ivec4 P1_coord  = ivec4(xyp_1_2_3.xw, tw, th   );
       ivec4 P2_coord  = ivec4(xyp_1_2_3.yw, tw, th   );
       ivec4 P3_coord  = ivec4(xyp_1_2_3.zw, tw, th   );

       ivec4 P6_coord  = ivec4(xyp_6_7_8.xw, tw, th   );
       ivec4 P7_coord  = ivec4(xyp_6_7_8.yw, tw, th   );
       ivec4 P8_coord  = ivec4(xyp_6_7_8.zw, tw, th   );

       ivec4 P11_coord = ivec4(xyp_11_12_13.xw, tw, th);
       ivec4 P12_coord = ivec4(xyp_11_12_13.yw, tw, th);
       ivec4 P13_coord = ivec4(xyp_11_12_13.zw, tw, th);

       ivec4 P16_coord = ivec4(xyp_16_17_18.xw, tw, th);
       ivec4 P17_coord = ivec4(xyp_16_17_18.yw, tw, th);
       ivec4 P18_coord = ivec4(xyp_16_17_18.zw, tw, th);

       ivec4 P21_coord = ivec4(xyp_21_22_23.xw, tw, th);
       ivec4 P22_coord = ivec4(xyp_21_22_23.yw, tw, th);
       ivec4 P23_coord = ivec4(xyp_21_22_23.zw, tw, th);

       ivec4 P5_coord  = ivec4(xyp_5_10_15.xy, tw, th );
       ivec4 P10_coord = ivec4(xyp_5_10_15.xz, tw, th );
       ivec4 P15_coord = ivec4(xyp_5_10_15.xw, tw, th );

       ivec4 P9_coord  = ivec4(xyp_9_14_9.xy, tw, th  );
       ivec4 P14_coord = ivec4(xyp_9_14_9.xz, tw, th  );
       ivec4 P19_coord = ivec4(xyp_9_14_9.xw, tw, th  );

       float alpha0;
       if ((attr & (1<<29)) != 0) alpha0 = 0.0;
       else                       alpha0 = 1.0;

       int tw = 8 << ((attr >> 20) & 0x7);
       int th = 8 << ((attr >> 23) & 0x7);
       ivec4 st_full = ivec4(ivec2(st), tw, th);

       ivec2 vramaddr = ivec2((attr & 0xFFFF) << 3, paladdr);
       int wrapmode = (attr >> 16);

       int type = (attr >> 26) & 0x7;
       
       vec4 P1, P2, P3, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15, P16, P17, P18, P19, P21, P22, P23;
       
       // Get mask values by performing texture lookup with each sampler
       if      (type == 5){
          P1  = TextureFetch_Compressed(vramaddr, P1_coord, wrapmode   );
          P2  = TextureFetch_Compressed(vramaddr, P2_coord, wrapmode   );
          P3  = TextureFetch_Compressed(vramaddr, P3_coord, wrapmode   );
      
          P6  = TextureFetch_Compressed(vramaddr, P6_coord, wrapmode   );
          P7  = TextureFetch_Compressed(vramaddr, P7_coord, wrapmode   );
          P8  = TextureFetch_Compressed(vramaddr, P8_coord, wrapmode   );
      
          P11 = TextureFetch_Compressed(vramaddr, P11_coord, wrapmode  );
          P12 = TextureFetch_Compressed(vramaddr, P12_coord, wrapmode  );
          P13 = TextureFetch_Compressed(vramaddr, P13_coord, wrapmode  );
      
          P16 = TextureFetch_Compressed(vramaddr, P16_coord, wrapmode  );
          P17 = TextureFetch_Compressed(vramaddr, P17_coord, wrapmode  );
          P18 = TextureFetch_Compressed(vramaddr, P18_coord, wrapmode  );
      
          P21 = TextureFetch_Compressed(vramaddr, P21_coord, wrapmode  );
          P22 = TextureFetch_Compressed(vramaddr, P22_coord, wrapmode  );
          P23 = TextureFetch_Compressed(vramaddr, P23_coord, wrapmode  );
      
          P5  = TextureFetch_Compressed(vramaddr, P5_coord, wrapmode   );
          P10 = TextureFetch_Compressed(vramaddr, P10_coord, wrapmode  );
          P15 = TextureFetch_Compressed(vramaddr, P15_coord, wrapmode  );
      
          P9  = TextureFetch_Compressed(vramaddr, P9_coord, wrapmode   );
          P14 = TextureFetch_Compressed(vramaddr, P14_coord, wrapmode  );
          P19 = TextureFetch_Compressed(vramaddr, P19_coord, wrapmode  );
       }
       else if (type == 2){
          P1  = TextureFetch_I2(vramaddr, P1_coord, wrapmode, alpha0   );
          P2  = TextureFetch_I2(vramaddr, P2_coord, wrapmode, alpha0   );
          P3  = TextureFetch_I2(vramaddr, P3_coord, wrapmode, alpha0   );
      
          P6  = TextureFetch_I2(vramaddr, P6_coord, wrapmode, alpha0   );
          P7  = TextureFetch_I2(vramaddr, P7_coord, wrapmode, alpha0   );
          P8  = TextureFetch_I2(vramaddr, P8_coord, wrapmode, alpha0   );
      
          P11 = TextureFetch_I2(vramaddr, P11_coord, wrapmode, alpha0  );
          P12 = TextureFetch_I2(vramaddr, P12_coord, wrapmode, alpha0  );
          P13 = TextureFetch_I2(vramaddr, P13_coord, wrapmode, alpha0  );
      
          P16 = TextureFetch_I2(vramaddr, P16_coord, wrapmode, alpha0  );
          P17 = TextureFetch_I2(vramaddr, P17_coord, wrapmode, alpha0  );
          P18 = TextureFetch_I2(vramaddr, P18_coord, wrapmode, alpha0  );
      
          P21 = TextureFetch_I2(vramaddr, P21_coord, wrapmode, alpha0  );
          P22 = TextureFetch_I2(vramaddr, P22_coord, wrapmode, alpha0  );
          P23 = TextureFetch_I2(vramaddr, P23_coord, wrapmode, alpha0  );
      
          P5  = TextureFetch_I2(vramaddr, P5_coord, wrapmode, alpha0   );
          P10 = TextureFetch_I2(vramaddr, P10_coord, wrapmode, alpha0  );
          P15 = TextureFetch_I2(vramaddr, P15_coord, wrapmode, alpha0  );
      
          P9  = TextureFetch_I2(vramaddr, P9_coord, wrapmode, alpha0   );
          P14 = TextureFetch_I2(vramaddr, P14_coord, wrapmode, alpha0  );
          P19 = TextureFetch_I2(vramaddr, P19_coord, wrapmode, alpha0  );
       }
       else if (type == 3){
          P1  = TextureFetch_I4(vramaddr, P1_coord, wrapmode, alpha0   );
          P2  = TextureFetch_I4(vramaddr, P2_coord, wrapmode, alpha0   );
          P3  = TextureFetch_I4(vramaddr, P3_coord, wrapmode, alpha0   );
      
          P6  = TextureFetch_I4(vramaddr, P6_coord, wrapmode, alpha0   );
          P7  = TextureFetch_I4(vramaddr, P7_coord, wrapmode, alpha0   );
          P8  = TextureFetch_I4(vramaddr, P8_coord, wrapmode, alpha0   );
      
          P11 = TextureFetch_I4(vramaddr, P11_coord, wrapmode, alpha0  );
          P12 = TextureFetch_I4(vramaddr, P12_coord, wrapmode, alpha0  );
          P13 = TextureFetch_I4(vramaddr, P13_coord, wrapmode, alpha0  );
      
          P16 = TextureFetch_I4(vramaddr, P16_coord, wrapmode, alpha0  );
          P17 = TextureFetch_I4(vramaddr, P17_coord, wrapmode, alpha0  );
          P18 = TextureFetch_I4(vramaddr, P18_coord, wrapmode, alpha0  );
      
          P21 = TextureFetch_I4(vramaddr, P21_coord, wrapmode, alpha0  );
          P22 = TextureFetch_I4(vramaddr, P22_coord, wrapmode, alpha0  );
          P23 = TextureFetch_I4(vramaddr, P23_coord, wrapmode, alpha0  );
      
          P5  = TextureFetch_I4(vramaddr, P5_coord, wrapmode, alpha0   );
          P10 = TextureFetch_I4(vramaddr, P10_coord, wrapmode, alpha0  );
          P15 = TextureFetch_I4(vramaddr, P15_coord, wrapmode, alpha0  );
      
          P9  = TextureFetch_I4(vramaddr, P9_coord, wrapmode, alpha0   );
          P14 = TextureFetch_I4(vramaddr, P14_coord, wrapmode, alpha0  );
          P19 = TextureFetch_I4(vramaddr, P19_coord, wrapmode, alpha0  );
       }
       else if (type == 4){
          P1  = TextureFetch_I8(vramaddr, P1_coord, wrapmode, alpha0   );
          P2  = TextureFetch_I8(vramaddr, P2_coord, wrapmode, alpha0   );
          P3  = TextureFetch_I8(vramaddr, P3_coord, wrapmode, alpha0   );
      
          P6  = TextureFetch_I8(vramaddr, P6_coord, wrapmode, alpha0   );
          P7  = TextureFetch_I8(vramaddr, P7_coord, wrapmode, alpha0   );
          P8  = TextureFetch_I8(vramaddr, P8_coord, wrapmode, alpha0   );
      
          P11 = TextureFetch_I8(vramaddr, P11_coord, wrapmode, alpha0  );
          P12 = TextureFetch_I8(vramaddr, P12_coord, wrapmode, alpha0  );
          P13 = TextureFetch_I8(vramaddr, P13_coord, wrapmode, alpha0  );
      
          P16 = TextureFetch_I8(vramaddr, P16_coord, wrapmode, alpha0  );
          P17 = TextureFetch_I8(vramaddr, P17_coord, wrapmode, alpha0  );
          P18 = TextureFetch_I8(vramaddr, P18_coord, wrapmode, alpha0  );
      
          P21 = TextureFetch_I8(vramaddr, P21_coord, wrapmode, alpha0  );
          P22 = TextureFetch_I8(vramaddr, P22_coord, wrapmode, alpha0  );
          P23 = TextureFetch_I8(vramaddr, P23_coord, wrapmode, alpha0  );
      
          P5  = TextureFetch_I8(vramaddr, P5_coord, wrapmode, alpha0   );
          P10 = TextureFetch_I8(vramaddr, P10_coord, wrapmode, alpha0  );
          P15 = TextureFetch_I8(vramaddr, P15_coord, wrapmode, alpha0  );
      
          P9  = TextureFetch_I8(vramaddr, P9_coord, wrapmode, alpha0   );
          P14 = TextureFetch_I8(vramaddr, P14_coord, wrapmode, alpha0  );
          P19 = TextureFetch_I8(vramaddr, P19_coord, wrapmode, alpha0  );
       }
       else if (type == 1){
          P1  = TextureFetch_A3I5(vramaddr, P1_coord, wrapmode   );
          P2  = TextureFetch_A3I5(vramaddr, P2_coord, wrapmode   );
          P3  = TextureFetch_A3I5(vramaddr, P3_coord, wrapmode   );
      
          P6  = TextureFetch_A3I5(vramaddr, P6_coord, wrapmode   );
          P7  = TextureFetch_A3I5(vramaddr, P7_coord, wrapmode   );
          P8  = TextureFetch_A3I5(vramaddr, P8_coord, wrapmode   );
      
          P11 = TextureFetch_A3I5(vramaddr, P11_coord, wrapmode  );
          P12 = TextureFetch_A3I5(vramaddr, P12_coord, wrapmode  );
          P13 = TextureFetch_A3I5(vramaddr, P13_coord, wrapmode  );
      
          P16 = TextureFetch_A3I5(vramaddr, P16_coord, wrapmode  );
          P17 = TextureFetch_A3I5(vramaddr, P17_coord, wrapmode  );
          P18 = TextureFetch_A3I5(vramaddr, P18_coord, wrapmode  );
      
          P21 = TextureFetch_A3I5(vramaddr, P21_coord, wrapmode  );
          P22 = TextureFetch_A3I5(vramaddr, P22_coord, wrapmode  );
          P23 = TextureFetch_A3I5(vramaddr, P23_coord, wrapmode  );
      
          P5  = TextureFetch_A3I5(vramaddr, P5_coord, wrapmode   );
          P10 = TextureFetch_A3I5(vramaddr, P10_coord, wrapmode  );
          P15 = TextureFetch_A3I5(vramaddr, P15_coord, wrapmode  );
      
          P9  = TextureFetch_A3I5(vramaddr, P9_coord, wrapmode   );
          P14 = TextureFetch_A3I5(vramaddr, P14_coord, wrapmode  );
          P19 = TextureFetch_A3I5(vramaddr, P19_coord, wrapmode  );
       }
       else if (type == 6){
          P1  = TextureFetch_A5I3(vramaddr, P1_coord, wrapmode   );
          P2  = TextureFetch_A5I3(vramaddr, P2_coord, wrapmode   );
          P3  = TextureFetch_A5I3(vramaddr, P3_coord, wrapmode   );
      
          P6  = TextureFetch_A5I3(vramaddr, P6_coord, wrapmode   );
          P7  = TextureFetch_A5I3(vramaddr, P7_coord, wrapmode   );
          P8  = TextureFetch_A5I3(vramaddr, P8_coord, wrapmode   );
      
          P11 = TextureFetch_A5I3(vramaddr, P11_coord, wrapmode  );
          P12 = TextureFetch_A5I3(vramaddr, P12_coord, wrapmode  );
          P13 = TextureFetch_A5I3(vramaddr, P13_coord, wrapmode  );
      
          P16 = TextureFetch_A5I3(vramaddr, P16_coord, wrapmode  );
          P17 = TextureFetch_A5I3(vramaddr, P17_coord, wrapmode  );
          P18 = TextureFetch_A5I3(vramaddr, P18_coord, wrapmode  );
      
          P21 = TextureFetch_A5I3(vramaddr, P21_coord, wrapmode  );
          P22 = TextureFetch_A5I3(vramaddr, P22_coord, wrapmode  );
          P23 = TextureFetch_A5I3(vramaddr, P23_coord, wrapmode  );
      
          P5  = TextureFetch_A5I3(vramaddr, P5_coord, wrapmode   );
          P10 = TextureFetch_A5I3(vramaddr, P10_coord, wrapmode  );
          P15 = TextureFetch_A5I3(vramaddr, P15_coord, wrapmode  );
      
          P9  = TextureFetch_A5I3(vramaddr, P9_coord, wrapmode   );
          P14 = TextureFetch_A5I3(vramaddr, P14_coord, wrapmode  );
          P19 = TextureFetch_A5I3(vramaddr, P19_coord, wrapmode  );
       }
       else               {
          P1  = TextureFetch_Direct(vramaddr, P1_coord, wrapmode   );
          P2  = TextureFetch_Direct(vramaddr, P2_coord, wrapmode   );
          P3  = TextureFetch_Direct(vramaddr, P3_coord, wrapmode   );
      
          P6  = TextureFetch_Direct(vramaddr, P6_coord, wrapmode   );
          P7  = TextureFetch_Direct(vramaddr, P7_coord, wrapmode   );
          P8  = TextureFetch_Direct(vramaddr, P8_coord, wrapmode   );
      
          P11 = TextureFetch_Direct(vramaddr, P11_coord, wrapmode  );
          P12 = TextureFetch_Direct(vramaddr, P12_coord, wrapmode  );
          P13 = TextureFetch_Direct(vramaddr, P13_coord, wrapmode  );
      
          P16 = TextureFetch_Direct(vramaddr, P16_coord, wrapmode  );
          P17 = TextureFetch_Direct(vramaddr, P17_coord, wrapmode  );
          P18 = TextureFetch_Direct(vramaddr, P18_coord, wrapmode  );
      
          P21 = TextureFetch_Direct(vramaddr, P21_coord, wrapmode  );
          P22 = TextureFetch_Direct(vramaddr, P22_coord, wrapmode  );
          P23 = TextureFetch_Direct(vramaddr, P23_coord, wrapmode  );
      
          P5  = TextureFetch_Direct(vramaddr, P5_coord, wrapmode   );
          P10 = TextureFetch_Direct(vramaddr, P10_coord, wrapmode  );
          P15 = TextureFetch_Direct(vramaddr, P15_coord, wrapmode  );
      
          P9  = TextureFetch_Direct(vramaddr, P9_coord, wrapmode   );
          P14 = TextureFetch_Direct(vramaddr, P14_coord, wrapmode  );
          P19 = TextureFetch_Direct(vramaddr, P19_coord, wrapmode  );
       }
       
       // Store luminance values of each point
       vec4 p7  = lum_to(P7.rgb,  P11.rgb, P17.rgb, P13.rgb);
       vec4 p8  = lum_to(P8.rgb,  P6.rgb,  P16.rgb, P18.rgb);
       vec4 p11 = p7.yzwx;                      // P11, P17, P13, P7
       vec4 p12 = lum_to(P12.rgb, P12.rgb, P12.rgb, P12.rgb);
       vec4 p13 = p7.wxyz;                      // P13, P7,  P11, P17
       vec4 p14 = lum_to(P14.rgb, P2.rgb,  P10.rgb, P22.rgb);
       vec4 p16 = p8.zwxy;                      // P16, P18, P8,  P6
       vec4 p17 = p7.zwxy;                      // P11, P17, P13, P7
       vec4 p18 = p8.wxyz;                      // P18, P8,  P6,  P16
       vec4 p19 = lum_to(P19.rgb, P3.rgb,  P5.rgb,  P21.rgb);
       vec4 p22 = p14.wxyz;                     // P22, P14, P2,  P10
       vec4 p23 = lum_to(P23.rgb, P9.rgb,  P1.rgb,  P15.rgb);

       vec2 fp = fract(st);

       vec4 ma45 = smoothstep(C45 - M45, C45 + M45, Ai * fp.y + B45 * fp.x);
       vec4 ma30 = smoothstep(C30 - M30, C30 + M30, Ai * fp.y + B30 * fp.x);
       vec4 ma60 = smoothstep(C60 - M60, C60 + M60, Ai * fp.y + B60 * fp.x);
       vec4 marn = smoothstep(C45 - M45 + Mshift, C45 + M45 + Mshift, Ai * fp.y + B45 * fp.x);

       vec4 e45   = lum_wd(p12, p8, p16, p18, p22, p14, p17, p13);
       vec4 econt = lum_wd(p17, p11, p23, p13, p7, p19, p12, p18);
       vec4 e30   = lum_df(p13, p16);
       vec4 e60   = lum_df(p8, p17);

       vec4 final45 = vec4(1.0);
       vec4 final30 = vec4(0.0);
       vec4 final60 = vec4(0.0);
       vec4 final36 = vec4(0.0);
       vec4 finalrn = vec4(0.0);

       vec4 px = step(lum_df(p12, p17), lum_df(p12, p13));

       vec4 mac = final36 * max(ma30, ma60) + final30 * ma30 + final60 * ma60 + final45 * ma45 + finalrn * marn;

       vec4 res1 = P12;
       res1 = mix(res1, mix(P13, P17, px.x), mac.x);
       res1 = mix(res1, mix(P7 , P13, px.y), mac.y);
       res1 = mix(res1, mix(P11, P7 , px.z), mac.z);
       res1 = mix(res1, mix(P17, P11, px.w), mac.w);

       vec4 res2 = P12;
       res2 = mix(res2, mix(P17, P11, px.w), mac.w);
       res2 = mix(res2, mix(P11, P7 , px.z), mac.z);
       res2 = mix(res2, mix(P7 , P13, px.y), mac.y);
       res2 = mix(res2, mix(P13, P17, px.x), mac.x);
       
       return mix(res1, res2, step(c_df(P12, res1), c_df(P12, res2)));
   }
// SABR

vec4 TextureLookup_Linear(vec2 texcoord)
{
    ivec2 intpart = ivec2(texcoord);
    vec2 fracpart = fract(texcoord);

    int attr = int(fPolygonAttr.y);
    int paladdr = int(fPolygonAttr.z);

    float alpha0;
    if ((attr & (1<<29)) != 0) alpha0 = 0.0;
    else                       alpha0 = 1.0;

    int tw = 8 << ((attr >> 20) & 0x7);
    int th = 8 << ((attr >> 23) & 0x7);
    ivec4 st_full = ivec4(intpart, tw, th);

    ivec2 vramaddr = ivec2((attr & 0xFFFF) << 3, paladdr);
    int wrapmode = (attr >> 16);

    vec4 A, B, C, D;
    int type = (attr >> 26) & 0x7;
    if (type == 5)
    {
        A = TextureFetch_Compressed(vramaddr, st_full                 , wrapmode);
        B = TextureFetch_Compressed(vramaddr, st_full + ivec4(1,0,0,0), wrapmode);
        C = TextureFetch_Compressed(vramaddr, st_full + ivec4(0,1,0,0), wrapmode);
        D = TextureFetch_Compressed(vramaddr, st_full + ivec4(1,1,0,0), wrapmode);
    }
    else if (type == 2)
    {
        A = TextureFetch_I2(vramaddr, st_full                 , wrapmode, alpha0);
        B = TextureFetch_I2(vramaddr, st_full + ivec4(1,0,0,0), wrapmode, alpha0);
        C = TextureFetch_I2(vramaddr, st_full + ivec4(0,1,0,0), wrapmode, alpha0);
        D = TextureFetch_I2(vramaddr, st_full + ivec4(1,1,0,0), wrapmode, alpha0);
    }
    else if (type == 3)
    {
        A = TextureFetch_I4(vramaddr, st_full                 , wrapmode, alpha0);
        B = TextureFetch_I4(vramaddr, st_full + ivec4(1,0,0,0), wrapmode, alpha0);
        C = TextureFetch_I4(vramaddr, st_full + ivec4(0,1,0,0), wrapmode, alpha0);
        D = TextureFetch_I4(vramaddr, st_full + ivec4(1,1,0,0), wrapmode, alpha0);
    }
    else if (type == 4)
    {
        A = TextureFetch_I8(vramaddr, st_full                 , wrapmode, alpha0);
        B = TextureFetch_I8(vramaddr, st_full + ivec4(1,0,0,0), wrapmode, alpha0);
        C = TextureFetch_I8(vramaddr, st_full + ivec4(0,1,0,0), wrapmode, alpha0);
        D = TextureFetch_I8(vramaddr, st_full + ivec4(1,1,0,0), wrapmode, alpha0);
    }
    else if (type == 1)
    {
        A = TextureFetch_A3I5(vramaddr, st_full                 , wrapmode);
        B = TextureFetch_A3I5(vramaddr, st_full + ivec4(1,0,0,0), wrapmode);
        C = TextureFetch_A3I5(vramaddr, st_full + ivec4(0,1,0,0), wrapmode);
        D = TextureFetch_A3I5(vramaddr, st_full + ivec4(1,1,0,0), wrapmode);
    }
    else if (type == 6)
    {
        A = TextureFetch_A5I3(vramaddr, st_full                 , wrapmode);
        B = TextureFetch_A5I3(vramaddr, st_full + ivec4(1,0,0,0), wrapmode);
        C = TextureFetch_A5I3(vramaddr, st_full + ivec4(0,1,0,0), wrapmode);
        D = TextureFetch_A5I3(vramaddr, st_full + ivec4(1,1,0,0), wrapmode);
    }
    else
    {
        A = TextureFetch_Direct(vramaddr, st_full                 , wrapmode);
        B = TextureFetch_Direct(vramaddr, st_full + ivec4(1,0,0,0), wrapmode);
        C = TextureFetch_Direct(vramaddr, st_full + ivec4(0,1,0,0), wrapmode);
        D = TextureFetch_Direct(vramaddr, st_full + ivec4(1,1,0,0), wrapmode);
    }

    float fx = fracpart.x;
    vec4 AB;
    if (A.a < (0.5/31.0) && B.a < (0.5/31.0))
        AB = vec4(0);
    else
    {
        //if (A.a < (0.5/31.0) || B.a < (0.5/31.0))
        //    fx = step(0.5, fx);

        AB = mix(A, B, fx);
    }

    fx = fracpart.x;
    vec4 CD;
    if (C.a < (0.5/31.0) && D.a < (0.5/31.0))
        CD = vec4(0);
    else
    {
        //if (C.a < (0.5/31.0) || D.a < (0.5/31.0))
        //    fx = step(0.5, fx);

        CD = mix(C, D, fx);
    }

    fx = fracpart.y;
    vec4 ret;
    if (AB.a < (0.5/31.0) && CD.a < (0.5/31.0))
        ret = vec4(0);
    else
    {
        //if (AB.a < (0.5/31.0) || CD.a < (0.5/31.0))
        //    fx = step(0.5, fx);

        ret = mix(AB, CD, fx);
    }

    return ret;
}

vec4 FinalColor()
{
    vec4 col;
    vec4 vcol = fColor;
    int blendmode = (fPolygonAttr.x >> 4) & 0x3;

    if (blendmode == 2)
    {
        if ((uDispCnt & (1<<1)) == 0)
        {
            // toon
            vec3 tooncolor = uToonColors[int(vcol.r * 31)].rgb;
            vcol.rgb = tooncolor;
        }
        else
        {
            // highlight
            vcol.rgb = vcol.rrr;
        }
    }

    if ((((fPolygonAttr.y >> 26) & 0x7) == 0) || ((uDispCnt & (1<<0)) == 0))
    {
        // no texture
        col = vcol;
    }
    else
    {
        vec4 tcol = TextureLookup_Nearest(fTexcoord);
        //vec4 tcol = TextureLookup_Linear(fTexcoord);

        if ((blendmode & 1) != 0)
        {
            // decal
            col.rgb = (tcol.rgb * tcol.a) + (vcol.rgb * (1.0-tcol.a));
            col.a = vcol.a;
        }
        else
        {
            // modulate
            col = vcol * tcol;
        }
    }

    if (blendmode == 2)
    {
        if ((uDispCnt & (1<<1)) != 0)
        {
            vec3 tooncolor = uToonColors[int(vcol.r * 31)].rgb;
            col.rgb = min(col.rgb + tooncolor, 1.0);
        }
    }

    return col.bgra;
}
)";


const char* kRenderVS_Z = R"(

void main()
{
    int attr = vPolygonAttr.x;
    int zshift = (attr >> 16) & 0x1F;

    vec4 fpos;
    fpos.xy = ((vec2(vPosition.xy) * 2.0) / uScreenSize) - 1.0;
    fpos.z = (float(vPosition.z << zshift) / 8388608.0) - 1.0;
    fpos.w = float(vPosition.w) / 65536.0f;
    fpos.xyz *= fpos.w;

    fColor = vec4(vColor) / vec4(255.0,255.0,255.0,31.0);
    fTexcoord = vec2(vTexcoord) / 16.0;
    fPolygonAttr = vPolygonAttr;

    gl_Position = fpos;
}
)";

const char* kRenderVS_W = R"(

smooth out float fZ;

void main()
{
    int attr = vPolygonAttr.x;
    int zshift = (attr >> 16) & 0x1F;

    vec4 fpos;
    fpos.xy = ((vec2(vPosition.xy) * 2.0) / uScreenSize) - 1.0;
    fZ = float(vPosition.z << zshift) / 16777216.0;
    fpos.w = float(vPosition.w) / 65536.0f;
    fpos.xy *= fpos.w;

    fColor = vec4(vColor) / vec4(255.0,255.0,255.0,31.0);
    fTexcoord = vec2(vTexcoord) / 16.0;
    fPolygonAttr = vPolygonAttr;

    gl_Position = fpos;
}
)";


const char* kRenderFS_ZO = R"(

void main()
{
    vec4 col = FinalColor();
    if (col.a < 30.5/31) discard;

    oColor = col;
    oAttr.r = float((fPolygonAttr.x >> 24) & 0x3F) / 63.0;
    oAttr.b = float((fPolygonAttr.x >> 15) & 0x1);
    oAttr.a = 1;
}
)";

const char* kRenderFS_WO = R"(

smooth in float fZ;

void main()
{
    vec4 col = FinalColor();
    if (col.a < 30.5/31) discard;

    oColor = col;
    oAttr.r = float((fPolygonAttr.x >> 24) & 0x3F) / 63.0;
    oAttr.b = float((fPolygonAttr.x >> 15) & 0x1);
    oAttr.a = 1;
    gl_FragDepth = fZ;
}
)";

const char* kRenderFS_ZT = R"(

void main()
{
    vec4 col = FinalColor();
    if (col.a < 0.5/31) discard;
    if (col.a >= 30.5/31) discard;

    oColor = col;
    oAttr.b = 0;
    oAttr.a = 1;
}
)";

const char* kRenderFS_WT = R"(

smooth in float fZ;

void main()
{
    vec4 col = FinalColor();
    if (col.a < 0.5/31) discard;
    if (col.a >= 30.5/31) discard;

    oColor = col;
    oAttr.b = 0;
    oAttr.a = 1;
    gl_FragDepth = fZ;
}
)";

const char* kRenderFS_ZSM = R"(

void main()
{
    oColor = vec4(0,0,0,1);
}
)";

const char* kRenderFS_WSM = R"(

smooth in float fZ;

void main()
{
    oColor = vec4(0,0,0,1);
    gl_FragDepth = fZ;
}
)";

#endif // GPU3D_OPENGL_SHADERS_H
