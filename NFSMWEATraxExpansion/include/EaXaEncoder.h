#pragma once
// EA-XA v2 SCHl writer. Codec arithmetic verified against vgmstream's BSD
// ea_xa_decoder.c; predictor search and stream writer are authored here.
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace eatrax { namespace eax {
inline void U32(std::ostream& out, uint32_t v) {
    for (int i=0;i<4;++i) out.put(static_cast<char>(v>>(8*i)));
}
inline void Block(std::ostream& out,const char* tag,const std::vector<uint8_t>& bytes) {
    out.write(tag,4); U32(out,static_cast<uint32_t>(bytes.size()+8));
    out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    if(!out) throw std::runtime_error("EA-XA output write failed");
}
inline void Append32(std::vector<uint8_t>& b,uint32_t v) {
    for(int i=0;i<4;++i) b.push_back(static_cast<uint8_t>(v>>(8*i)));
}
inline void Patch(std::vector<uint8_t>& b,uint8_t tag,uint32_t v) {
    int n=1; while(n<4 && (v>>(8*n))) ++n;
    b.push_back(tag); b.push_back(static_cast<uint8_t>(n));
    for(int i=n-1;i>=0;--i) b.push_back(static_cast<uint8_t>(v>>(8*i)));
}
struct History { int h1=0,h2=0; };
inline void Frame(const std::array<int,28>& input,History& history,std::vector<uint8_t>& output) {
    static constexpr int c1[]={0,240,460,392}, c2[]={0,0,-208,-220};
    int64_t best=std::numeric_limits<int64_t>::max();
    std::array<uint8_t,15> encoded{}; History chosen{};
    for(int filter=0;filter<4;++filter) {
        int a=history.h1,b=history.h2,peak=0;
        for(int sample:input) {
            peak=std::max(peak,std::abs(sample-((c1[filter]*a+c2[filter]*b)>>8)));
            b=a;a=sample;
        }
        int shift=12; while(shift>0 && peak>7*(1<<(12-shift))) --shift;
        for(int s=std::max(0,shift-1);s<=std::min(12,shift+1);++s) {
            const int step=1<<(12-s); History h=history;
            std::array<uint8_t,15> trial{}; trial[0]=static_cast<uint8_t>((filter<<4)|s);
            int64_t error=0;
            for(int i=0;i<28;++i) {
                const int prediction=(c1[filter]*h.h1+c2[filter]*h.h2)>>8;
                const int residual=input[i]-prediction;
                const int q=std::clamp(residual>=0?(residual+step/2)/step:-((-residual+step/2)/step),-8,7);
                // This integer expression matches the native/vgmstream decoder.
                const int sample=std::clamp((q*step*256+c1[filter]*h.h1+c2[filter]*h.h2)>>8,-32768,32767);
                const int64_t d=sample-input[i];error+=d*d;
                h.h2=h.h1;h.h1=sample;
                trial[1+i/2]|=static_cast<uint8_t>((q&15)<<((i&1)?0:4));
            }
            if(error<best){best=error;encoded=trial;chosen=h;}
        }
    }
    output.insert(output.end(),encoded.begin(),encoded.end());history=chosen;
}
inline uint32_t EncodeRaw(const std::filesystem::path& source,uint64_t start,uint32_t frames,const std::filesystem::path& target) {
    if(!frames || source==target) throw std::runtime_error("Invalid EA-XA input/output");
    std::ifstream input(source,std::ios::binary);
    if(!input) throw std::runtime_error("Cannot open PCM source");
    input.seekg(0,std::ios::end);const auto length=static_cast<uint64_t>(input.tellg());
    if(length%4 || start>length/4 || start+frames>length/4+28) throw std::runtime_error("Invalid PCM slice");
    input.seekg(static_cast<std::streamoff>(start*4));
    std::ofstream out(target,std::ios::binary|std::ios::trunc);
    if(!out) throw std::runtime_error("Cannot create EA-XA stream");
    std::vector<uint8_t> header={'P','T',0,0,6,1,101,253};
    Patch(header,0x80,2);Patch(header,0x85,frames);Patch(header,0x82,2);Patch(header,0x84,36000);
    header.push_back(255);while(header.size()%4)header.push_back(0);Block(out,"SCHl",header);
    constexpr uint32_t blockFrames=28*85;std::vector<uint8_t> count;
    Append32(count,(frames+blockFrames-1)/blockFrames);Block(out,"SCCl",count);
    History histories[2];
    for(uint32_t offset=0;offset<frames;offset+=blockFrames) {
        const uint32_t n=std::min(blockFrames,frames-offset);
        std::vector<uint8_t> pcm(n*4,0);const uint64_t available=length/4-start;
        const auto readable=static_cast<std::streamsize>(std::min<uint64_t>(n,available>offset?available-offset:0)*4);
        input.read(reinterpret_cast<char*>(pcm.data()),readable);
        if(input.gcount()!=readable) throw std::runtime_error("Truncated PCM source");
        std::vector<uint8_t> channels[2];
        for(int ch=0;ch<2;++ch) {
            for(uint32_t i=0;i<n;i+=28) {
                std::array<int,28> samples{};
                for(uint32_t j=0;j<28 && i+j<n;++j) {
                    const auto p=(i+j)*4+ch*2;
                    samples[j]=static_cast<int16_t>(pcm[p]|(pcm[p+1]<<8));
                }
                Frame(samples,histories[ch],channels[ch]);
            }
            if(channels[ch].size()%2)channels[ch].push_back(0);
        }
        std::vector<uint8_t> block;Append32(block,n);Append32(block,0);Append32(block,static_cast<uint32_t>(channels[0].size()));
        for(auto& ch:channels)block.insert(block.end(),ch.begin(),ch.end());Block(out,"SCDl",block);
    }
    Block(out,"SCEl",{});out.close();if(!out)throw std::runtime_error("EA-XA flush failed");
    return static_cast<uint32_t>((static_cast<uint64_t>(frames)*1000+18000)/36000);
}
} }
