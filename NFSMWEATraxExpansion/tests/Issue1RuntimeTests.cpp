// Exercise real callbacks with isolated state; no game launch or save access.
#include "../src/RuntimeHooks.cpp"
#include <iostream>
#include <stdexcept>
void Check(bool yes,const char* what){if(!yes)throw std::runtime_error(what);}
int wmain(int argc,wchar_t** argv){
 using namespace eatrax;
 if(argc!=2)return 2;
 const std::filesystem::path root(argv[1]);std::filesystem::create_directories(root);
 CatalogResult cat;cat.config.statePath=root/L"State.ini";cat.config.iniPath=root/L"Main.ini";
 cat.tracks.resize(2);
 for(unsigned i=0;i<2;++i){cat.tracks[i].artist="Artist"+std::to_string(i);cat.tracks[i].album="Album"+std::to_string(i);cat.tracks[i].stateSection="Track_"+std::to_string(i);}
 g_catalog=&cat;g_stockProfile="ProfileOne";
 SaveTrack native[26]{},shadow[28]{};g_nativeEntries=native;g_shadowEntries=shadow;
 g_status.nativeTrackCount=26;g_combinedModes.assign(28,TrackMode::All);g_customModes.assign(2,TrackMode::All);
 for(unsigned i=0;i<26;++i){g_stockEvents[i]=0x100+i;native[i].mode=3;}
 for(unsigned i=0;i<26;++i){OnTrackModeChanged(i,0);Check(native[i].mode==0,"native mode write");Check(ReadStockMode(cat.config.statePath,"ProfileOne",g_stockEvents[i])==0,"restart persisted off");Check(!ReadStockMode(cat.config.statePath,"ProfileTwo",g_stockEvents[i]),"cross-profile leakage");}
 Check(!ReadStockMode(cat.config.statePath,"ProfileOne",0x900),"missing key overrides native save");
 Check(!WriteStockMode(cat.config.statePath,"",0x100,0),"empty profile accepted");
 Check(!WriteStockMode(cat.config.statePath,"ProfileOne",0x100,4),"invalid mode accepted");
 const auto key=StockModeSection("ProfileOne",0x999);WritePrivateProfileStringW(key.c_str(),L"Mode",L"99",cat.config.statePath.c_str());
 Check(!ReadStockMode(cat.config.statePath,"ProfileOne",0x999),"invalid stored mode accepted");
 const auto before=Sha256File(cat.config.statePath);
 cat.config.streamerMode=true;cat.config.statePath=root/L"StreamerState.ini";
 for(unsigned i=0;i<26;++i)for(unsigned m=0;m<4;++m){shadow[i].mode=static_cast<uint8_t>(m);OnTrackModeChanged(i,m);Check(native[i].mode==0&&shadow[i].mode==0,"streamer changed normal mode");}
 Check(Sha256File(root/L"State.ini")==before,"streamer wrote normal state");
 OnTrackModeChanged(26,2);Check(ReadIniString(cat.config.statePath,L"Track_0",L"Mode",L"")==L"IG","custom streamer save failed");
 cat.config.streamerMode=false;cat.config.statePath=root/L"State.ini";OnTrackModeChanged(0,3);
 Check(ReadStockMode(cat.config.statePath,"ProfileOne",0x100)==3,"normal mode cannot recover");
 JukeboxTrack tracks[2]{};g_addedTracks={&tracks[0],&tracks[1]};
 for(bool swap:{true,false,true,false}){
 WritePrivateProfileStringW(L"Main",L"SwapArtistAlbum",swap?L"true":L"false",cat.config.iniPath.c_str());g_lastDisplayPoll=0;PollTrackPresentation();
 for(unsigned i=0;i<2;++i){Check(std::string(tracks[i].artist)==(swap?cat.tracks[i].album:cat.tracks[i].artist),"global artist pointer swap");Check(std::string(tracks[i].album)==(swap?cat.tracks[i].artist:cat.tracks[i].album),"global album pointer swap");Check(cat.tracks[i].artist=="Artist"+std::to_string(i)&&cat.tracks[i].album=="Album"+std::to_string(i),"source metadata changed");}}
 InitializeLogging(root/L"Disabled.log",false);Log(LogLevel::Info,"must not write");Check(!std::filesystem::exists(root/L"Disabled.log"),"logging off wrote a file");
 PlaybackDiagnosticState diag;Check(diag.Poll(1000)&&!diag.Poll(1100)&&diag.Poll(2000),"disabled/uninitialized poll not throttled");
 PlaybackSnapshot s;s.music=0;s.channel=1;s.flags=4;
 Check(!diag.SuspectedGap(s,1000)&&!diag.SuspectedGap(s,10999)&&diag.SuspectedGap(s,11000)&&!diag.SuspectedGap(s,12000),"gap warning threshold/latch");
 s.paused=true;Check(!diag.SuspectedGap(s,13000),"pause warns");s.paused=false;s.channel=3;Check(!diag.SuspectedGap(s,25000),"paused channel warns");s.channel=1;s.music=1;Check(!diag.SuspectedGap(s,40000),"pursuit warns");
 std::cout<<"PASS 26 stock persistence, profile isolation, invalid/absent fallback, 104 streamer edits, global display toggle, diagnostic throttle/gap/pause\n";
 return 0;
}
