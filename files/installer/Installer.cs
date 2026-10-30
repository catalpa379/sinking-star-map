using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using Microsoft.Win32;

namespace SinkingMapSetup {
 public class Item { public string path {get;set;} public string sha256 {get;set;} }
 public class Manifest { public List<Item> files {get;set;} }
 public class PriorVersions { public List<Manifest> versions {get;set;} }
 public class TerrainFile { public string name {get;set;} public string sha256 {get;set;} }
 public class Terrain { public string revision {get;set;} public List<TerrainFile> files {get;set;} }

 public sealed class Setup {
  public readonly string Base,Files;
  public Action<string> Log = delegate {};
  public Action<bool> CanCancel = delegate {};
  public Action<int,string> Progress = delegate {};
  public volatile bool Cancelled;
  private readonly object sync=new object();
  private Process running;
  const string GameHash="0AE10A513DAE25286FE93D0DB4FA8A2DF9A473BC07E1734E12BC1AD30166DB1C";
  const string LevelsHash="408ABA3676925FAEA0C823F8C231E30844FEFFE9C32AE42644D273EF31863589";
  const string OldNodeHash="6D75619E8720161BB0E8C5617BCF90384B48A920B9182EC6E9D52F11F83A8E84";
  const string Revision="818260c8b5fe704362722a9369350807ba635e68";
  static readonly string[] Images={"map-0.smt","map-1.smt","map-2.smt"};
  public Setup(string directory) { Base=Path.GetFullPath(directory).TrimEnd('\\'); Files=Path.Combine(Base,"files"); }
  public static string Hash(string path) { using(var s=File.OpenRead(path)) using(var h=SHA256.Create()) return BitConverter.ToString(h.ComputeHash(s)).Replace("-",""); }
  public static bool Same(string a,string b) { return String.Equals(a,b,StringComparison.OrdinalIgnoreCase); }
  public static string Within(string root,string relative) {
   var folder=Path.GetFullPath(root).TrimEnd('\\');var file=Path.GetFullPath(Path.Combine(folder,relative));
   if(!file.StartsWith(folder+"\\",StringComparison.OrdinalIgnoreCase)) throw new Exception("安装清单包含越界路径。");return file;
  }
  static T Json<T>(string path) { return new JavaScriptSerializer {MaxJsonLength=Int32.MaxValue}.Deserialize<T>(File.ReadAllText(path,Encoding.UTF8)); }
  public static string Quote(string value) {
   var b=new StringBuilder("\"");int slash=0;
   foreach(char c in value) { if(c=='\\'){slash++;continue;}b.Append('\\',c=='"'?slash*2+1:slash);slash=0;b.Append(c); }
   b.Append('\\',slash*2);return b.Append('"').ToString();
  }
  void CheckCancel() { if(Cancelled)throw new OperationCanceledException(); }
  public void Cancel() { Cancelled=true;lock(sync){if(running!=null&&!running.HasExited)try{running.Kill();}catch{}} }
  public void VerifyPackage() {
   Log("正在核验安装包完整性…");var m=Json<Manifest>(Path.Combine(Files,"package-integrity.json"));
   if(m.files==null||m.files.Count<10)throw new Exception("请先完整解压安装包。");
   foreach(var f in m.files) {CheckCancel();var p=Within(Files,f.path);if(!File.Exists(p)||!Same(Hash(p),f.sha256))throw new Exception("安装包文件缺失或校验失败："+f.path+"。请重新解压。");}
  }
  public static void CheckGame(string game) {
   game=Path.GetFullPath(game);var exe=Path.Combine(game,"sinking_star.exe");
   if(!File.Exists(exe))throw new Exception("请选择正式版 sinking_star.exe 所在目录。");
   var actualExe=Hash(exe);bool original=Same(actualExe,GameHash);bool current=Same(actualExe,"D802098B7CB85EADFC733B88D1554042FAD950C572E916FEBEA3F669A474A549");
   if(!original&&!current)throw new Exception("此游戏版本尚未支持。仅支持 Steam Build 26006400 或 26249670；已停止安装。");
   var levels=Path.Combine(game,"data","levels.package");
   if(!File.Exists(levels)||!Same(Hash(levels),original?LevelsHash:"D747E5D9B8F540C9C6B90E8E987BD74A8C7F8F2F0672FCE189403128F1F3117C"))throw new Exception("游戏地图资源版本不匹配；已停止安装。");
   if(Process.GetProcessesByName("sinking_star").Length!=0)throw new Exception("请先正常退出《沉星之序》，再安装或卸载。");
  }
  string Execute(string exe,string args,bool cancellable) {
   CheckCancel();CanCancel(cancellable);var output=new StringBuilder();
   var info=new ProcessStartInfo(exe,args) {UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true,WorkingDirectory=Files,StandardOutputEncoding=Encoding.UTF8,StandardErrorEncoding=Encoding.UTF8};
   if(String.Equals(Path.GetFileName(exe),"powershell.exe",StringComparison.OrdinalIgnoreCase))info.EnvironmentVariables["PSModulePath"]=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System),"WindowsPowerShell","v1.0","Modules")+";"+Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),"WindowsPowerShell","Modules");
   using(var p=new Process {StartInfo=info}) {
    p.OutputDataReceived+=(s,e)=>{if(e.Data!=null){lock(output)output.AppendLine(e.Data);Log(e.Data);}};
    p.ErrorDataReceived+=(s,e)=>{if(e.Data!=null){lock(output)output.AppendLine(e.Data);Log(e.Data);}};
    if(!p.Start())throw new Exception("无法启动安装辅助程序。");lock(sync)running=cancellable?p:null;
    p.BeginOutputReadLine();p.BeginErrorReadLine();p.WaitForExit();lock(sync)running=null;
    CheckCancel();if(p.ExitCode!=0)throw new Exception("辅助程序未完成。请检查上方日志后重试。\r\n"+output.ToString());
   }return output.ToString();
  }
  public void EnsureRuntimes() {
   CanCancel(true);Progress(20,"2 / 4 · 校验内置运行库");
   var payload=Json<Manifest>(Path.Combine(Files,"SHA256.json"));
   foreach(var f in payload.files){CheckCancel();if(!Same(Hash(Within(Files,f.path)),f.sha256))throw new Exception("模组载荷校验失败："+f.path);}
   Log("内置运行库校验完成，无需下载。");
  }
  public static bool ValidTerrain(string directory) {
   try {var t=Json<Terrain>(Path.Combine(directory,"provenance.json"));if(t.revision!=Revision||t.files==null||t.files.Count!=3)return false;
    if(!new HashSet<string>(t.files.Select(x=>x.name)).SetEquals(Images))return false;
    return t.files.All(f=>Same(Hash(Within(directory,f.name)),f.sha256));
   } catch {return false;}
  }
  void PowerShell(string script,string game,string terrain) {
   var exe=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System),"WindowsPowerShell","v1.0","powershell.exe");
   var args="-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "+Quote(Path.Combine(Files,script))+" -GamePath "+Quote(game);
   if(terrain!=null)args+=" -TerrainPath "+Quote(terrain);
   Execute(exe,args,false);
  }
  bool IsThisVersion(string game,bool allowOldRuntime) {
   var m=Json<Manifest>(Path.Combine(Files,"SHA256.json"));
   foreach(var f in m.files) {var p=Within(game,f.path);if(!File.Exists(p))return false;var actual=Hash(p);
    if(!Same(actual,f.sha256)&&!(allowOldRuntime&&f.path=="SinkingMap/runtime/node.exe"&&Same(actual,OldNodeHash)))return false;}
   return true;
  }
  bool IsSupportedPrevious(string game) {
   var supported=Json<PriorVersions>(Path.Combine(Files,"Supported-Previous.json"));
   return supported.versions!=null&&supported.versions.Any(v=>v.files!=null&&v.files.Count>=10&&v.files.All(f=>{var p=Within(game,f.path);return File.Exists(p)&&Same(Hash(p),f.sha256);}));
  }
  List<Item> LegacyImages(string game) {
   var result=new List<Item>();var folder=Path.Combine(game,"SinkingMap","terrain");
   var expected=new[]{
    new Item{path="ground.jpg",sha256="E29E69194AED0C4C2DEfDF75A69DCB22727D21F9FF5A20068280A6357ACF7B9C"},
    new Item{path="underworld.jpg",sha256="D54E61BF9A6DD1B1EFB8349B2327ADF4647089F7BFF490ECA617DD50EF7B0851"},
    new Item{path="sky.jpg",sha256="57E7FED7CF1308D5DAB7752584E2497671FA6018525405A4584612644F7AE231"}};
   foreach(var f in expected){var p=Within(folder,f.path);if(File.Exists(p)&&Same(Hash(p),f.sha256))result.Add(f);}return result;
  }
  public void Install(string game,string suppliedCache) {
   Cancelled=false;Progress(2,"1 / 4 · 检查安装包与游戏版本");CanCancel(true);game=Path.GetFullPath(game).TrimEnd('\\');VerifyPackage();Log("正在检查游戏版本…");CheckGame(game);
   var proxy=Path.Combine(game,"dxgi.dll");var manifest=Json<Manifest>(Path.Combine(Files,"SHA256.json"));var wanted=manifest.files.First(f=>f.path=="dxgi.dll").sha256;
   var legacyImages=LegacyImages(game);string mode="fresh";
   if(File.Exists(proxy)) {
    if(Same(Hash(proxy),wanted)) {if(IsThisVersion(game,true))mode="same";else if(IsSupportedPrevious(game))mode="update";else throw new Exception("已安装的同名模组文件被修改，停止覆盖。请先核实原模组。");}
    else {var old=Json<PriorVersions>(Path.Combine(Files,"Supported-Previous.json"));if(old.versions==null||!old.versions.Any(v=>v.files.Any(f=>f.path=="dxgi.dll"&&Same(Hash(proxy),f.sha256))))throw new Exception("已有其他版本或其他模组的 dxgi.dll，请先确认并卸载，安装器不会覆盖。");mode="update";}
   } else if(Directory.Exists(Path.Combine(game,"SinkingMap")))throw new Exception("发现已有 SinkingMap 目录，请先核实残留文件或使用卸载功能。");
   EnsureRuntimes();
   Progress(45,"3 / 4 · 校验内置地图素材");
   string cache=String.IsNullOrEmpty(suppliedCache)?Path.Combine(Files,"SinkingMap","terrain"):suppliedCache;
   var existing=Path.Combine(game,"SinkingMap","terrain");
   if(!ValidTerrain(cache))throw new Exception("离线地图缺失或损坏。请完整解压安装包；安装器不会联网补下载。");
   Log("资源校验完成，无需下载。");
   CheckCancel();CanCancel(false);Progress(70,"4 / 4 · 安装并验证文件");CheckGame(game);Log("正在安装已校验的模组文件…");
   if(mode=="fresh")PowerShell("Install.ps1",game,cache);
   else {
    if(mode=="update")PowerShell("Update.ps1",game,null);
    if(mode=="same") {
     var target=Path.Combine(game,"SinkingMap","runtime","node.exe");var source=Path.Combine(Files,"SinkingMap","runtime","node.exe");
     if(!Same(Hash(target),Hash(source))) {var temp=target+".setup-"+Guid.NewGuid().ToString("N");File.Copy(source,temp);File.Replace(temp,target,target+".before-tester-setup-"+DateTime.Now.ToString("yyyyMMddHHmmss"));}
    }
    if(!Same(Path.GetFullPath(cache).TrimEnd('\\'),Path.GetFullPath(existing).TrimEnd('\\'))) {Directory.CreateDirectory(existing);foreach(var name in Images.Concat(new[]{"provenance.json"}))File.Copy(Path.Combine(cache,name),Path.Combine(existing,name),true);}
   }
   if(!IsThisVersion(game,false)||!ValidTerrain(existing))throw new Exception("安装后校验失败，请保留本窗口日志并反馈。");
   foreach(var f in legacyImages){var p=Within(existing,f.path);if(File.Exists(p)&&Same(Hash(p),f.sha256))File.Delete(p);}
   Progress(100,"安装完成");Log("安装完成。请通过 Steam 启动游戏，按键可在“按键设置”中修改。");Log("默认保留迷雾；“显示全部未完成（剧透）”需要主动开启。");
  }
  public void Uninstall(string game) {Cancelled=false;Progress(10,"正在检查已安装文件");VerifyPackage();CheckGame(game);CanCancel(false);Progress(60,"正在移除可核实的模组文件");PowerShell("Uninstall.ps1",game,null);Progress(100,"卸载完成");Log("卸载完成。存档和游戏原文件未改动；已修改文件和旧版备份会保留。");}
  public static string FindGame() {
   var roots=new List<string>();
   foreach(var key in new[]{Registry.CurrentUser.OpenSubKey(@"Software\Valve\Steam"),Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Valve\Steam")})if(key!=null){using(key){var path=(key.GetValue("SteamPath")??key.GetValue("InstallPath"))as string;if(!String.IsNullOrEmpty(path))roots.Add(path);}}
   roots.Add(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),"Steam"));
   foreach(var steam in roots.ToArray())try{var text=File.ReadAllText(Path.Combine(steam,"steamapps","libraryfolders.vdf"));foreach(Match m in Regex.Matches(text,"\"path\"\\s+\"([^\"]+)\""))roots.Add(m.Groups[1].Value.Replace("\\\\","\\"));}catch{}
   foreach(var root in roots.Distinct()){var game=Path.Combine(root,"steamapps","common","Order of the Sinking Star");if(File.Exists(Path.Combine(game,"sinking_star.exe")))return game;}return "";
  }
 }

 // Refactoring UI §2.1/2.8: installation is the primary action; uninstall/details are secondary.
 // §3.2/3.6/4.1: 8/16/24 spacing, 10pt body and 22pt title, auto-sized flow at every DPI.
 // White / blue semantic tokens; §2.8 one primary install action, §3.6 grouped content.
 public class InstallerForm : Form {
  static readonly Color Ink=Color.FromArgb(29,49,76),Muted=Color.FromArgb(86,106,131),Blue=Color.FromArgb(37,99,195),Canvas=Color.FromArgb(245,248,253),Border=Color.FromArgb(204,216,232);

  readonly Setup setup; readonly TextBox path=new TextBox(),log=new TextBox();
  readonly Button browse=new Button(),install=new Button(),uninstall=new Button(),cancel=new Button(),keys=new Button();
  readonly Label state=new Label(),detail=new Label(),elapsed=new Label();
  readonly ProgressBar progress=new ProgressBar(); readonly LinkLabel more=new LinkLabel();
  readonly System.Windows.Forms.Timer timer=new System.Windows.Forms.Timer(); readonly Stopwatch clock=new Stopwatch();
  readonly TableLayoutPanel root=new TableLayoutPanel(); bool working;
  static Label TextLabel(string text,bool muted=false) { return new Label {Text=text,AutoSize=true,Dock=DockStyle.Top,ForeColor=muted?Muted:Ink,Margin=new Padding(0,0,0,8)}; }
  static TableLayoutPanel Stack() {var p=new TableLayoutPanel {ColumnCount=1,AutoSize=true,AutoSizeMode=AutoSizeMode.GrowAndShrink,Dock=DockStyle.Top,Margin=new Padding(0,0,0,16)};p.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));return p;}
  static void Add(TableLayoutPanel table,Control control) {table.RowStyles.Add(new RowStyle(SizeType.AutoSize));table.Controls.Add(control,0,table.RowCount++);}
  static void ButtonStyle(Button b,string text,bool primary=false) {
   b.Text=text;b.AutoSize=true;b.AutoSizeMode=AutoSizeMode.GrowAndShrink;b.Padding=new Padding(16,8,16,8);b.Margin=new Padding(0,0,8,0);
   b.FlatStyle=FlatStyle.Flat;b.FlatAppearance.BorderSize=primary?0:1;b.FlatAppearance.BorderColor=Border;b.FlatAppearance.MouseOverBackColor=primary?Color.FromArgb(28,80,162):Color.FromArgb(234,241,251);b.FlatAppearance.MouseDownBackColor=primary?Color.FromArgb(23,66,137):Color.FromArgb(221,232,247);
   b.BackColor=primary?Blue:Color.White;b.ForeColor=primary?Color.White:Ink;b.UseVisualStyleBackColor=false;b.Cursor=Cursors.Hand;
  }
  public InstallerForm(string game) {
   SuspendLayout();AutoScaleDimensions=new SizeF(96F,96F);AutoScaleMode=AutoScaleMode.Dpi;
   setup=new Setup(AppDomain.CurrentDomain.BaseDirectory);Text="沉星之序地图 · 离线安装";Font=new Font("Microsoft YaHei UI",10F);
   ClientSize=new Size(800,600);MinimumSize=new Size(600,520);StartPosition=FormStartPosition.CenterScreen;BackColor=Canvas;
   var scroll=new Panel {Dock=DockStyle.Fill,AutoScroll=true};Controls.Add(scroll);
   root.ColumnCount=1;root.RowCount=0;root.Dock=DockStyle.Top;root.AutoSize=true;root.AutoSizeMode=AutoSizeMode.GrowAndShrink;root.Padding=new Padding(24);root.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));scroll.Controls.Add(root);
   var header=Stack();
   var eyebrow=TextLabel("离线安装  /  0.1.16");eyebrow.ForeColor=Blue;eyebrow.Font=new Font(Font,FontStyle.Bold);Add(header,eyebrow);
   var title=TextLabel("沉星之序地图");title.Font=new Font(Font.FontFamily,24F,FontStyle.Bold);title.Margin=new Padding(0,0,0,8);Add(header,title);
   Add(root,header);
   var card=Stack();card.Padding=new Padding(24);card.BackColor=Color.White;
   var directory=TextLabel("游戏位置");directory.Font=new Font(Font,FontStyle.Bold);Add(card,directory);
   var line=new TableLayoutPanel {ColumnCount=2,RowCount=1,AutoSize=true,Dock=DockStyle.Top,Margin=new Padding(0,0,0,8)};
   line.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));line.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
   path.Text=game;path.Dock=DockStyle.Fill;path.Margin=new Padding(0,6,16,0);path.AccessibleName="游戏目录";line.Controls.Add(path,0,0);
   ButtonStyle(browse,"浏览…");browse.Margin=Padding.Empty;line.Controls.Add(browse,1,0);Add(card,line);
   Add(card,TextLabel("请选择包含 sinking_star.exe 的文件夹。",true));
   var offline=TextLabel("完整离线 · 安装无需联网");offline.ForeColor=Blue;offline.Margin=new Padding(0,8,0,8);Add(card,offline);
   Add(card,TextLabel("安装前请正常退出游戏。支持 Steam Build 26006400 / 26249670。",true));Add(root,card);
   var status=Stack();status.Padding=new Padding(24,16,24,16);status.BackColor=Color.White;state.Text="准备安装";state.AutoSize=true;state.Dock=DockStyle.Top;state.Font=new Font(Font,FontStyle.Bold);state.Margin=new Padding(0,0,0,8);Add(status,state);
   progress.Dock=DockStyle.Top;progress.Height=6;progress.Margin=new Padding(0,0,0,8);Add(status,progress);
   detail.Text="确认游戏目录后，点击“安装 / 更新”。";detail.AutoSize=true;detail.Dock=DockStyle.Top;detail.Margin=new Padding(0,0,0,8);Add(status,detail);
   elapsed.Text="只读获取游戏进度，不修改存档。";elapsed.AutoSize=true;elapsed.Dock=DockStyle.Top;elapsed.ForeColor=Muted;elapsed.Margin=Padding.Empty;Add(status,elapsed);Add(root,status);
   var actions=new FlowLayoutPanel {AutoSize=true,Dock=DockStyle.Top,WrapContents=true,Margin=new Padding(0,0,0,16)};
   ButtonStyle(install,"安装 / 更新",true);install.MinimumSize=new Size(160,48);ButtonStyle(uninstall,"卸载模组");ButtonStyle(keys,"按键设置");keys.Click+=delegate{using(var dialog=new KeySettingsForm())dialog.ShowDialog(this);};ButtonStyle(cancel,"取消校验");cancel.Visible=false;
   actions.Controls.AddRange(new Control[]{install,uninstall,keys,cancel});Add(root,actions);
   more.Text="查看安装详情";more.AutoSize=true;more.Margin=new Padding(0,0,0,8);more.LinkColor=Blue;more.Click+=delegate{log.Visible=!log.Visible;more.Text=log.Visible?"收起安装详情":"查看安装详情";};Add(root,more);
   log.Multiline=true;log.ReadOnly=true;log.ScrollBars=ScrollBars.Vertical;log.Dock=DockStyle.Top;log.Height=144;log.Visible=false;log.BackColor=Color.White;log.BorderStyle=BorderStyle.FixedSingle;log.Margin=new Padding(0,0,0,16);Add(root,log);
   
   root.SizeChanged+=delegate{WrapText(root);};
   browse.Click+=delegate{using(var dlg=new OpenFileDialog {Title="选择游戏主程序",Filter="游戏主程序|sinking_star.exe",CheckFileExists=true}){if(Directory.Exists(path.Text))dlg.InitialDirectory=path.Text;if(dlg.ShowDialog(this)==DialogResult.OK)path.Text=Path.GetDirectoryName(dlg.FileName);}};
   install.Click+=async delegate{await Work(false);};
   uninstall.Click+=async delegate{if(MessageBox.Show(this,"卸载本版本模组？游戏存档会保留。","卸载模组",MessageBoxButtons.YesNo,MessageBoxIcon.Question)==DialogResult.Yes)await Work(true);};
   cancel.Click+=delegate{cancel.Enabled=false;setup.Cancel();};
   setup.Log=s=>Post(()=>{log.AppendText(s+Environment.NewLine);});
   setup.Progress=(value,text)=>Post(()=>{progress.Value=Math.Max(0,Math.Min(100,value));state.Text=text;detail.Text=value==100?"可以关闭安装器。地图默认保留迷雾。":"正在处理本地文件，请稍候…";});
   setup.CanCancel=b=>Post(()=>{cancel.Visible=b&&working;cancel.Enabled=b;});
   timer.Interval=500;timer.Tick+=delegate{elapsed.Text="已用时 "+clock.Elapsed.ToString(@"mm\:ss")+"  ·  全程离线";};
   FormClosing+=(sender,e)=>{if(working){e.Cancel=true;state.Text="正在处理文件，请等待完成或取消校验。";}};
   FormClosed+=delegate{timer.Dispose();};
   Shown+=delegate{var area=Screen.FromControl(this).WorkingArea;if(Height>area.Height-32)Height=area.Height-32;if(Width>area.Width-32)Width=area.Width-32;WrapText(root);};
   ResumeLayout(true);
  }
  void Post(Action action){if(!IsDisposed&&IsHandleCreated)BeginInvoke(action);}
  static void WrapText(Control parent){foreach(Control c in parent.Controls){var label=c as Label;if(label!=null&&!(label is LinkLabel))label.MaximumSize=new Size(Math.Max(160,parent.ClientSize.Width-parent.Padding.Horizontal-label.Margin.Horizontal),0);else if(c is TableLayoutPanel)WrapText(c);}}
  async Task Work(bool remove) {
   if(String.IsNullOrWhiteSpace(path.Text)){state.Text="请先选择游戏目录。";return;}
   string game=path.Text;working=true;install.Enabled=uninstall.Enabled=browse.Enabled=path.Enabled=false;progress.Value=0;log.Clear();clock.Restart();timer.Start();
   try{await Task.Run(()=>{if(remove)setup.Uninstall(game);else setup.Install(game,null);});state.Text=remove?"卸载完成":"安装完成";progress.Value=100;detail.Text=remove?"已移除可核实的模组文件，存档保持不变。":"从 Steam 启动游戏。按键可通过“按键设置”修改。";}
   catch(OperationCanceledException){state.Text="已取消校验";detail.Text="没有下载任务。可重新点击安装。";}
   catch(Exception ex){state.Text="未能完成";detail.Text=ex.Message.Split(new[]{'\r','\n'})[0];log.AppendText(ex.Message+Environment.NewLine);log.Visible=true;more.Text="收起安装详情";}
   finally{timer.Stop();clock.Stop();elapsed.Text="本次用时 "+clock.Elapsed.ToString(@"mm\:ss")+"  ·  全程离线";working=false;cancel.Visible=false;install.Enabled=uninstall.Enabled=browse.Enabled=path.Enabled=true;}
  }
 }
 static class Program {
  [STAThread] static int Main(string[] args) {
   if(args.Length>0&&args[0].StartsWith("--")&&args[0]!="--preview")try {
    var s=new Setup(AppDomain.CurrentDomain.BaseDirectory);s.Log=Console.WriteLine;
    if(args[0]=="--verify"){s.VerifyPackage();Console.WriteLine("PASS package integrity");return 0;}
    if(args[0]=="--prepare-runtime"){s.VerifyPackage();s.EnsureRuntimes();Console.WriteLine("PASS runtime bootstrap");return 0;}
    if(args[0]=="--install-to"&&args.Length>=2){s.Install(args[1],args.Length>=3?args[2]:null);return 0;}
    if(args[0]=="--uninstall-from"&&args.Length==2){s.Uninstall(args[1]);return 0;}
    throw new Exception("未知测试参数。");
   }catch(Exception ex){Console.Error.WriteLine(ex.ToString());return 1;}
   Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);var form=new InstallerForm(Setup.FindGame());Application.Run(form);return 0;
  }
 }
}
