using System;
using System.Drawing;
using System.IO;
using System.Text.RegularExpressions;
using System.Windows.Forms;
namespace SinkingMapSetup {
 public static class KeyPreferences {
  public static string PathName {get{return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"SinkingMap","controls.ini");}}
  public static int Encode(Keys key) {return ((int)(key&Keys.KeyCode)&255)|((key&Keys.Alt)!=0?256:0)|((key&Keys.Control)!=0?512:0)|((key&Keys.Shift)!=0?1024:0);}
  public static bool Valid(int code) {
   int key=code&255,mods=code>>8;
   bool allowed=(key>=112&&key<=135)||(key>=65&&key<=90)||(key>=48&&key<=57)||(key>=33&&key<=40)||key==45||key==46||key==32;
   return allowed&&mods>=0&&mods<=7&&!((mods&1)!=0&&key==115);
  }
  public static string Name(int code) {
   int mods=code>>8,key=code&255;
   return ((mods&2)!=0?"Ctrl + ":"")+((mods&1)!=0?"Alt + ":"")+((mods&4)!=0?"Shift + ":"")+((Keys)key).ToString();
  }
  public static int[] Load(string file) {
   int a=119,b=116;
   try {
    string section="";
    foreach(string raw in File.ReadAllLines(file)) {
     string line=raw.Trim();if(line.StartsWith("[")){section=line;continue;}
     if(section!="[Keys]")continue;
     Match match=Regex.Match(line,@"^(Toggle|Refresh)\s*=\s*(-?\d+)\s*$");int n;
     if(match.Success&&Int32.TryParse(match.Groups[2].Value,out n)){if(match.Groups[1].Value=="Toggle")a=n;else b=n;}
    }
   }catch(IOException){}catch(UnauthorizedAccessException){}
   return Valid(a)&&Valid(b)&&a!=b?new[]{a,b}:new[]{119,116};
  }
  public static void Save(string file,int a,int b) {
   if(!Valid(a)||!Valid(b)||a==b)throw new ArgumentException("请为两个操作设置不同的有效按键。");
   Directory.CreateDirectory(Path.GetDirectoryName(file));
   string temp=file+".tmp-"+Guid.NewGuid().ToString("N");
   try {File.WriteAllText(temp,"[Keys]\r\nToggle="+a+"\r\nRefresh="+b+"\r\n",System.Text.Encoding.ASCII);if(File.Exists(file))File.Replace(temp,file,null);else File.Move(temp,file);}
   finally{if(File.Exists(temp))File.Delete(temp);}
  }
 }
 public sealed class KeySettingsForm:Form {
  readonly int[] pending=KeyPreferences.Load(KeyPreferences.PathName);
  readonly TextBox[] fields={new TextBox(),new TextBox()};
  readonly Label notice=new Label();
  public KeySettingsForm(){
   Text="按键设置";Font=new Font("Microsoft YaHei UI",10);AutoScaleDimensions=new SizeF(96,96);AutoScaleMode=AutoScaleMode.Dpi;
   ClientSize=new Size(480,264);MinimumSize=new Size(480,264);FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;MinimizeBox=false;StartPosition=FormStartPosition.CenterParent;BackColor=Color.White;
   var table=new TableLayoutPanel{Dock=DockStyle.Fill,Padding=new Padding(24),ColumnCount=2,RowCount=5};table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute,168));table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent,100));Controls.Add(table);
   var hint=new Label{Text="点击输入框，按下新的按键或组合键。",AutoSize=true,Margin=new Padding(0,0,0,20)};table.Controls.Add(hint,0,0);table.SetColumnSpan(hint,2);
   for(int i=0;i<2;i++){
    int index=i;var label=new Label{Text=i==0?"打开 / 关闭地图":"刷新地图",AutoSize=true,Margin=new Padding(0,6,8,20)};table.Controls.Add(label,0,i+1);
    var box=fields[i];box.Text=KeyPreferences.Name(pending[i]);box.ReadOnly=true;box.BackColor=Color.FromArgb(245,248,253);box.Dock=DockStyle.Fill;box.Margin=new Padding(0,0,0,20);box.TextAlign=HorizontalAlignment.Center;box.ShortcutsEnabled=false;
    box.KeyDown+=(sender,e)=>{e.SuppressKeyPress=true;e.Handled=true;if(e.KeyCode==Keys.Escape){Close();return;}int code=KeyPreferences.Encode(e.KeyData);if(KeyPreferences.Valid(code)){pending[index]=code;box.Text=KeyPreferences.Name(code);notice.Text="设置后点击保存。两个操作不能使用相同组合键。";}};
    table.Controls.Add(box,1,i+1);
   }
   notice.Text="两个操作不能使用相同组合键。";notice.AutoSize=true;notice.ForeColor=Color.FromArgb(86,106,131);notice.Margin=new Padding(0,0,0,16);table.Controls.Add(notice,0,3);table.SetColumnSpan(notice,2);
   var actions=new FlowLayoutPanel{Dock=DockStyle.Fill,AutoSize=true,WrapContents=false,Margin=Padding.Empty};table.Controls.Add(actions,0,4);table.SetColumnSpan(actions,2);
   foreach(string text in new[]{"恢复默认","取消","保存"}){
    var button=new Button{Text=text,Width=128,Height=36,FlatStyle=FlatStyle.Flat,Margin=new Padding(0,0,8,0)};button.FlatAppearance.BorderColor=Color.FromArgb(204,216,232);actions.Controls.Add(button);
    if(text=="恢复默认")button.Click+=delegate{pending[0]=119;pending[1]=116;for(int i=0;i<2;i++)fields[i].Text=KeyPreferences.Name(pending[i]);};
    if(text=="取消"){button.Click+=delegate{Close();};CancelButton=button;}
    if(text=="保存"){
     button.BackColor=Color.FromArgb(37,99,195);button.ForeColor=Color.White;button.FlatAppearance.BorderSize=0;
     button.Click+=delegate{try{KeyPreferences.Save(KeyPreferences.PathName,pending[0],pending[1]);DialogResult=DialogResult.OK;Close();}catch(Exception ex){notice.Text=ex is ArgumentException?ex.Message:"保存失败，原设置未改变。请检查用户目录权限。";}};
    }
   }
  }
 }
}
