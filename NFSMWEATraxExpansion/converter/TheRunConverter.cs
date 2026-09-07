// EA-XAS decoding follows vgmstream's ea_xas_decoder.c (BSD license; see Licenses).
// No audio is embedded. The recipe contains source offsets/hashes and authored playback rules only.
using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using System.Drawing;

[assembly: AssemblyVersion("0.4.6.0")]
[assembly: AssemblyFileVersion("0.4.6.0")]
[assembly: AssemblyTitle("The Run Pursuit Converter")]
public class Source { public string id,hash; public long offset; public int length,frames; }
public class Group { public string[] sources; public double gain; public int repeat,frames; }
public class Sample { public int group,start,frames; }
public class Recipe { public int version; public string source; public Source[] sources; public Group[] groups; public Sample[] samples; }
static class ConvertAudio {
 public static bool Japanese=CultureInfo.CurrentUICulture.TwoLetterISOLanguageName=="ja";
 public static string T(string ja,string en){return Japanese?ja:en;}
 public static byte[] Resource(string name){using(var s=Assembly.GetExecutingAssembly().GetManifestResourceStream(name))using(var m=new MemoryStream()){if(s==null)throw new Exception("Missing recipe: "+name);s.CopyTo(m);return m.ToArray();}}
 public static string Hash(byte[] b){using(var h=SHA256.Create())return BitConverter.ToString(h.ComputeHash(b)).Replace("-","");}
 public static string FileHash(string p){using(var h=SHA256.Create())using(var f=File.OpenRead(p))return BitConverter.ToString(h.ComputeHash(f)).Replace("-","");}
 static uint BE(byte[] b,int p){return (uint)(b[p]<<24|b[p+1]<<16|b[p+2]<<8|b[p+3]);}
 static void Check(bool ok,string message){if(!ok)throw new InvalidDataException(message);}
 public static byte[] Decode(byte[] b,int expected) {
  Check(b.Length>=12 && BE(b,0)==0x4800000c && BE(b,4)==0x140cbb80 && (BE(b,8)&0x1fffffff)==expected,"Unsupported The Run SPS header");
  var output=new short[checked(expected*4)];int pos=12,total=0;bool end=false;
  float[] c1={0,0.9375f,1.796875f,1.53125f},c2={0,0,-0.8125f,-0.859375f};
  while(pos+4<=b.Length){uint h=BE(b,pos);int size=(int)(h&0xffffff),tag=(int)(h>>24);Check(size>=4 && size<=b.Length-pos,"Truncated SPS block");
   if(tag==0x45){end=true;break;}Check(tag==0x44 && size>=8,"Invalid SPS data block");int frames=(int)BE(b,pos+4);Check(frames>0 && frames<=expected-total,"Invalid SPS frame count");
   int blocks=(frames+127)/128;Check(8+blocks*76*4<=size,"Truncated EA-XAS frame");
   for(int k=0;k<blocks;k++)for(int ch=0;ch<4;ch++)for(int g=0;g<4;g++) {
    int at=pos+8+k*304+ch*76;uint head=BitConverter.ToUInt32(b,at+g*4);int filter=(int)(head&15);Check(filter<4,"Invalid EA-XAS predictor");
    int h2=(short)(head&0xfff0),h1=(short)((head>>16)&0xfff0),shift=(int)((head>>16)&15);
    int frame=k*128+g*32;
    if(frame<frames)output[(total+frame)*4+ch]=(short)h2;
    if(frame+1<frames)output[(total+frame+1)*4+ch]=(short)h1;
    for(int j=0;j<30;j++){int nib=b[at+16+(j/2)*4+g];nib=(j%2==0?nib>>4:nib&15);int value=(short)(nib<<12)>>shift;
     value=(int)(value+h1*c1[filter]+h2*c2[filter]);value=Math.Max(-32768,Math.Min(32767,value));h2=h1;h1=value;
     if(frame+j+2<frames)output[(total+frame+j+2)*4+ch]=(short)value;
    }
   }total+=frames;pos+=size;
  }Check(end && total==expected,"Incomplete SPS audio");byte[] raw=new byte[output.Length*2];Buffer.BlockCopy(output,0,raw,0,raw.Length);return raw;
 }
 static string Q(string s){if(s.Contains('"'))throw new ArgumentException("Invalid path");return "\""+s+"\"";}
 static void FF(string ff,string args,CancellationToken ct) {
  var si=new ProcessStartInfo(ff,"-v error -nostdin -y -threads 1 "+args){UseShellExecute=false,CreateNoWindow=true,RedirectStandardError=true};
  using(var p=Process.Start(si)) {var errors=p.StandardError.ReadToEndAsync();
   try {while(!p.WaitForExit(100))ct.ThrowIfCancellationRequested();ct.ThrowIfCancellationRequested();if(p.ExitCode!=0)throw new Exception("FFmpeg: "+errors.Result);}
   finally {if(!p.HasExited){p.Kill();p.WaitForExit();}}
  }
 }
 static void Patch(BinaryWriter w,byte tag,uint value){w.Write(tag);int n=value<=255?1:value<=65535?2:value<=16777215?3:4;w.Write((byte)n);for(int j=n-1;j>=0;j--)w.Write((byte)(value>>(j*8)));}
 static void Block(BinaryWriter w,string tag,byte[] b){w.Write(Encoding.ASCII.GetBytes(tag));w.Write(b.Length+8);w.Write(b);}
 static void StreamAudio(BinaryWriter dest,string pcm,int start,int frames) {
  using(var m=new MemoryStream())using(var h=new BinaryWriter(m)) {
   h.Write(new byte[]{80,84,0,0});Patch(h,128,3);Patch(h,129,16);Patch(h,130,2);Patch(h,132,36000);Patch(h,133,(uint)frames);Patch(h,160,8);h.Write((byte)255);
   while(m.Length%4!=0)h.Write((byte)0);Block(dest,"SCHl",m.ToArray());
  }Block(dest,"SCCl",BitConverter.GetBytes((frames+4095)/4096));
  using(var input=File.OpenRead(pcm)){input.Position=(long)start*4;
   for(int offset=0;offset<frames;offset+=4096){int count=Math.Min(4096,frames-offset);byte[] interleaved=new byte[count*4];int got=0,n;
    while(got<interleaved.Length && (n=input.Read(interleaved,got,interleaved.Length-got))>0)got+=n;
    using(var m=new MemoryStream())using(var w=new BinaryWriter(m)){w.Write(count);w.Write(0);w.Write(count*2);for(int ch=0;ch<2;ch++)for(int i=0;i<count;i++){w.Write(interleaved[i*4+ch*2]);w.Write(interleaved[i*4+ch*2+1]);}Block(dest,"SCDl",m.ToArray());}
   }
  }Block(dest,"SCEl",new byte[0]);
 }
 static void GameClosed(string game) {
  foreach(var p in Process.GetProcessesByName("speed"))using(p){try{if(string.Equals(Path.GetDirectoryName(p.MainModule.FileName),game,StringComparison.OrdinalIgnoreCase))throw new InvalidOperationException(T("先にMWを終了してください。","Close MW before converting."));}catch(System.ComponentModel.Win32Exception){throw new InvalidOperationException(T("実行中のゲームを終了してから再試行してください。","Close running game instances and try again."));}}
 }
 static void DeleteWork(string work,string parent){string full=Path.GetFullPath(work);if(!full.StartsWith(Path.GetFullPath(parent)+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase))throw new Exception("Unsafe cleanup path");if(Directory.Exists(full))Directory.Delete(full,true);}
 public static void Run(string sourceGame,string mwGame,Action<string> log,CancellationToken ct) {
  sourceGame=Path.GetFullPath(sourceGame);mwGame=Path.GetFullPath(mwGame).TrimEnd(Path.DirectorySeparatorChar);GameClosed(mwGame);
  string mod=Path.Combine(mwGame,"scripts","NFSMWEATraxExpansion"),ff=Path.Combine(mod,"Runtime","ffmpeg.exe");
  Check(File.Exists(ff) && File.Exists(Path.Combine(mod,"NFSMWEATraxExpansion.ini")),T("先にMW EA TRAX Expansion 0.4.6を導入してください。","Install MW EA TRAX Expansion 0.4.6 first."));
  var serializer=new JavaScriptSerializer(){MaxJsonLength=4000000};Recipe r=serializer.Deserialize<Recipe>(Encoding.UTF8.GetString(Resource("recipe.json")));
  string sb=Path.Combine(sourceGame,r.source);Check(File.Exists(sb),T("The RunのData/Win32/ChunksAudio.sbが見つかりません。","The Run Data/Win32/ChunksAudio.sb was not found."));
  string work=Path.Combine(mod,"converter-work-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(work);
  try {
   var paths=new Dictionary<string,string>();int done=0;
   using(var input=File.OpenRead(sb))foreach(var s in r.sources){ct.ThrowIfCancellationRequested();Check(s.offset>=0 && s.length>0 && s.length<64000000 && s.offset+s.length<=input.Length,"Unsupported source archive size");
    input.Position=s.offset;byte[] b=new byte[s.length];int count=0,n;while(count<b.Length && (n=input.Read(b,count,b.Length-count))>0)count+=n;
    Check(count==b.Length && string.Equals(Hash(b),s.hash,StringComparison.OrdinalIgnoreCase),T("対応していない、または変更されたThe Run音源です: ","Unsupported or modified The Run audio: ")+s.id);
    string raw=Path.Combine(work,"input.raw"),path=Path.Combine(work,"source-"+done+".pcm");File.WriteAllBytes(raw,Decode(b,s.frames));
    FF(ff,"-f s16le -ar 48000 -ac 4 -i "+Q(raw)+" -af \"pan=stereo|c0=0.5*c0+0.5*c2|c1=0.5*c1+0.5*c3\" -ar 36000 -c:a pcm_s16le -f s16le "+Q(path),ct);
    paths[s.id]=path;log(T("音源を読み込み中 ","Reading audio ")+(++done)+"/"+r.sources.Length);
   }
   var groupPaths=new Dictionary<int,string>();
   for(int i=0;i<r.groups.Length;i++){ct.ThrowIfCancellationRequested();Group g=r.groups[i];string phrase=Path.Combine(work,"phrase.pcm"),normalized=Path.Combine(work,"group-"+i+".pcm");
    using(var output=File.Create(phrase)){for(int repeat=0;repeat<g.repeat;repeat++)foreach(string id in g.sources)using(var input=File.OpenRead(paths[id]))input.CopyTo(output);}
    if(g.frames>0)using(var f=new FileStream(phrase,FileMode.Open,FileAccess.ReadWrite)){long wanted=(long)g.frames*4;Check(Math.Abs(wanted-f.Length)<4096,"Unexpected phrase length");if(f.Length<wanted){byte[] last=new byte[4];f.Position=f.Length-4;f.Read(last,0,4);f.Position=f.Length;while(f.Length<wanted)f.Write(last,0,4);}else f.SetLength(wanted);}
    FF(ff,"-f s16le -ar 36000 -ac 2 -i "+Q(phrase)+" -af "+Q("volume="+g.gain.ToString("F9",CultureInfo.InvariantCulture)+"dB,alimiter=limit=0.98:level=0:latency=1")+" -c:a pcm_s16le -f s16le "+Q(normalized),ct);
    Check(new FileInfo(phrase).Length==new FileInfo(normalized).Length,"Normalization changed audio length");groupPaths[i]=normalized;log(T("フレーズを準備中 ","Preparing phrases ")+(i+1)+"/"+r.groups.Length);
   }
   string ready=Path.Combine(work,"Pursuit");Directory.CreateDirectory(ready);byte[] mpf=Resource("graph.mpf");int table=BitConverter.ToInt32(mpf,52);Check((mpf.Length-table)/8==r.samples.Length,"Recipe sample mismatch");
   using(var w=new BinaryWriter(File.Create(Path.Combine(ready,"CustomPursuit.mus"))))for(int i=0;i<r.samples.Length;i++) {
    ct.ThrowIfCancellationRequested();Sample s=r.samples[i];string pcm=groupPaths[s.group];int available=checked((int)(new FileInfo(pcm).Length/4));int frames=s.frames>0?s.frames:available;
    Check(s.start>=0 && frames>0 && s.start+frames<=available+28,"Invalid audio slice");while(w.BaseStream.Position%128!=0)w.Write((byte)0);
    Buffer.BlockCopy(BitConverter.GetBytes(checked((uint)(w.BaseStream.Position/128))),0,mpf,table+i*8,4);
    Buffer.BlockCopy(BitConverter.GetBytes((uint)Math.Round(frames*1000.0/36000)),0,mpf,table+i*8+4,4);
    StreamAudio(w,pcm,s.start,frames);if(i%20==0)log(T("追跡データを生成中 ","Building pursuit data ")+(i+1)+"/"+r.samples.Length);
   }
   File.WriteAllBytes(Path.Combine(ready,"CustomPursuit.mpf"),mpf);
   string ini=Encoding.ASCII.GetString(Resource("profile.ini")).Replace("MpfSHA256=GENERATED","MpfSHA256="+Hash(mpf)).Replace("MusSHA256=GENERATED","MusSHA256="+FileHash(Path.Combine(ready,"CustomPursuit.mus")));
   File.WriteAllText(Path.Combine(ready,"Pursuit.ini"),ini,Encoding.ASCII);
   ct.ThrowIfCancellationRequested();GameClosed(mwGame);
   string target=Path.Combine(mod,"Pursuit"),backup=Path.Combine(mod,"Pursuit-backup-"+DateTime.Now.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N").Substring(0,6));bool moved=false;
   try{if(Directory.Exists(target)){Directory.Move(target,backup);moved=true;}Directory.Move(ready,target);}catch{if(moved && !Directory.Exists(target))Directory.Move(backup,target);throw;}
   log(T("完了。MWを起動し、キャッシュ生成後の通知に従って起動し直してください。","Complete. Launch MW, then restart when its cache generation finishes."));if(moved)log("Backup: "+backup);
  }finally {DeleteWork(work,mod);}
 }
 [STAThread] public static int Main(string[] args) {
  if(args.Length==2 && (args[0]=="--render-ui" || args[0]=="--render-ui-en")) {Japanese=args[0]!="--render-ui-en";Application.EnableVisualStyles();using(var f=new ConverterForm()){f.ShowInTaskbar=false;f.Show();Application.DoEvents();using(var b=new Bitmap(f.Width,f.Height)){f.DrawToBitmap(b,new Rectangle(0,0,b.Width,b.Height));b.Save(args[1]);}}return 0;}
  if(args.Length==4 && args[0]=="--decode-test"){var b=File.ReadAllBytes(args[1]);File.WriteAllBytes(args[3],Decode(b,int.Parse(args[2])));return 0;}
  if(args.Length==3 && args[0]=="--convert") {try{Run(args[1],args[2],Console.WriteLine,CancellationToken.None);return 0;}catch(Exception e){Console.Error.WriteLine(e);return 1;}}
  Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);Application.Run(new ConverterForm());return 0;
 }
}
class ConverterForm:Form {
 TextBox source=new TextBox(),target=new TextBox(),status=new TextBox();Button start=new Button(),cancel=new Button();CancellationTokenSource cts;
 public ConverterForm(){Text="The Run Pursuit Converter 0.4.6";ClientSize=new Size(740,430);StartPosition=FormStartPosition.CenterScreen;Font=new Font("Segoe UI",10);FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;
  Controls.Add(new Label(){Text=ConvertAudio.T("所有するPC版The Runから追跡BGMを生成します。先にMW EA TRAX Expansion 0.4.6を導入してください。","Convert pursuit music from your PC copy of The Run. Install MW EA TRAX Expansion 0.4.6 first."),Bounds=new Rectangle(18,12,704,55)});
  AddPath(source,74,"The Run",false);AddPath(target,132,"Most Wanted",true);
  start.Text=ConvertAudio.T("変換開始","Convert");start.Bounds=new Rectangle(18,196,140,34);cancel.Text=ConvertAudio.T("キャンセル","Cancel");cancel.Bounds=new Rectangle(170,196,140,34);cancel.Enabled=false;Controls.Add(start);Controls.Add(cancel);
  status.Multiline=true;status.ReadOnly=true;status.ScrollBars=ScrollBars.Vertical;status.Bounds=new Rectangle(18,245,704,165);Controls.Add(status);
  start.Click+=async delegate {if(cts!=null)return;cts=new CancellationTokenSource();start.Enabled=false;cancel.Enabled=true;source.Enabled=target.Enabled=false;string a=source.Text,b=target.Text;
   try{await Task.Run(()=>ConvertAudio.Run(a,b,s=>BeginInvoke((Action)(()=>status.AppendText(s+Environment.NewLine))),cts.Token));MessageBox.Show(this,ConvertAudio.T("生成完了。MWを起動し、生成通知に従って再起動してください。","Conversion complete. Start MW and follow its generation/restart notice."),Text,MessageBoxButtons.OK,MessageBoxIcon.Information);}
   catch(OperationCanceledException){status.AppendText(ConvertAudio.T("キャンセルしました。既存データは変更していません。","Cancelled. Existing pursuit data was preserved.")+Environment.NewLine);}
   catch(Exception e){MessageBox.Show(this,e.Message,Text,MessageBoxButtons.OK,MessageBoxIcon.Error);}
   finally{cts.Dispose();cts=null;start.Enabled=true;cancel.Enabled=false;source.Enabled=target.Enabled=true;}
  };cancel.Click+=delegate{if(cts!=null)cts.Cancel();};FormClosing+=delegate(object sender,FormClosingEventArgs e){if(cts!=null){cts.Cancel();e.Cancel=true;status.AppendText(ConvertAudio.T("処理の終了を待っています。終了後に閉じてください。","Waiting for cleanup. Close the window once processing stops.")+Environment.NewLine);}};
 }
 void AddPath(TextBox box,int y,string label,bool mw){Controls.Add(new Label(){Text=label+ConvertAudio.T(" のインストール先"," installation folder"),Bounds=new Rectangle(18,y,640,22)});box.Bounds=new Rectangle(18,y+24,606,26);Controls.Add(box);var button=new Button(){Text="...",Bounds=new Rectangle(638,y+23,84,28)};Controls.Add(button);button.Click+=delegate{if(cts!=null)return;using(var d=new FolderBrowserDialog()){d.Description=label;if(d.ShowDialog(this)==DialogResult.OK)box.Text=d.SelectedPath;}};}
}
