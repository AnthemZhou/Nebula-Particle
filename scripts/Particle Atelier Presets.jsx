#target aftereffects
#include "ParticleAtelierPresetLib.jsxinc"

(function (thisObj) {
    var api=PA_Presets;
    var win=(thisObj instanceof Panel) ? thisObj : new Window("palette","Particle Atelier · 粒子预设",undefined,{resizeable:true});
    win.orientation="column"; win.alignChildren=["fill","top"]; win.spacing=10; win.margins=14;
    win.minimumSize=[300,410];
    var title=win.add("statictext",undefined,"Particle Atelier · 粒子预设");
    var sourceRow=win.add("group"); sourceRow.orientation="row"; sourceRow.alignChildren=["fill","center"];
    var source=sourceRow.add("dropdownlist",undefined,["内置预设","我的预设"]); source.selection=0; source.alignment=["fill","center"];
    var refresh=sourceRow.add("button",undefined,"刷新");
    var list=win.add("listbox",undefined,[],{multiselect:false}); list.preferredSize=[300,170]; list.alignment=["fill","fill"];
    var description=win.add("statictext",undefined,"",{multiline:true}); description.preferredSize=[300,65];
    var apply=win.add("button",undefined,"应用到所选图层 / 效果");
    var save=win.add("button",undefined,"保存当前效果为我的预设…");
    var note=win.add("statictext",undefined,"保留素材、发射范围、随机种子与时间设置。\n含关键帧或表达式的目标参数不会被覆盖。",{multiline:true}); note.preferredSize=[300,35];
    var status=win.add("statictext",undefined,"选中一个图层，或其中的 Particle Atelier 效果。",{multiline:true}); status.preferredSize=[300,40];
    var records=[];

    function message(text) { status.text=text; }
    function report(error) { message(error.toString()); alert(error.toString(),"Particle Atelier"); }
    function selected() { return list.selection ? records[list.selection.index] : null; }
    function showDescription() {
        var p=selected(); description.text=p ? p.description : "还没有用户预设。调整粒子后，点击下方保存按钮。";
        apply.enabled=!!p;
    }
    function refreshList(preferredName) {
        records=[]; list.removeAll(); var errors=[];
        try {
            if(source.selection.index===0) records=api.builtins();
            else {
                var result=api.listUserPresets(); errors=result.errors;
                for(var j=0;j<result.presets.length;j++) records.push(result.presets[j].preset);
            }
            var pick=0;
            for(var i=0;i<records.length;i++) { list.add("item",records[i].name); if(records[i].name===preferredName) pick=i; }
            if(records.length) list.selection=pick;
            showDescription();
            if(errors.length) { message("有 "+errors.length+" 个预设文件未通过检查，已跳过。鼠标停留此处查看原因。"); status.helpTip=errors.join("\n"); }
            else { message(records.length+" 个预设 · 选中图层后即可应用。"); status.helpTip=""; }
        } catch(error) { message(error.toString()); showDescription(); }
        win.layout.layout(true);
    }
    function askName() {
        var dialog=new Window("dialog","保存我的粒子预设");
        dialog.orientation="column";dialog.alignChildren=["fill","top"];dialog.margins=16;dialog.spacing=10;
        dialog.add("statictext",undefined,"名称");
        var name=dialog.add("edittext",undefined,"我的粒子效果"); name.characters=30;
        dialog.add("statictext",undefined,"用途说明（可选）");
        var desc=dialog.add("edittext",undefined,"",{multiline:true}); desc.preferredSize=[330,75];
        var hint=dialog.add("statictext",undefined,"保存当前帧的外观和运动数值。\n不保存关键帧、素材链接或发射起止时间。\n同名预设会自动加编号，原文件保留。",{multiline:true});hint.preferredSize=[330,55];
        var buttons=dialog.add("group");buttons.alignment="right";
        buttons.add("button",undefined,"取消",{name:"cancel"});
        var ok=buttons.add("button",undefined,"保存",{name:"ok"});
        var answer=null;
        ok.onClick=function(){
            try { answer={name:api.safeName(name.text),description:desc.text}; dialog.close(1); }
            catch(error) { alert(error.toString(),"预设名称"); }
        };
        name.active=true;
        return dialog.show()===1 ? answer : null;
    }
    source.onChange=function(){refreshList();};
    refresh.onClick=function(){var p=selected();refreshList(p ? p.name : null);};
    list.onChange=showDescription;
    apply.onClick=function(){
        try {
            var p=selected();if(!p) return;
            var result=api.applySelected(p);
            message("已应用“"+result.name+"”"+(result.created ? "，并在所选图层添加 Particle Atelier。" : "。可用 AE 撤销恢复。"));
        } catch(error) { report(error); }
    };
    save.onClick=function(){
        try {
            var effect=api.selectedEffect(app.project.activeItem), metadata=askName();if(!metadata) return;
            var saved=api.saveUserPreset(api.captureEffect(effect,metadata.name,metadata.description));
            source.selection=1;refreshList(saved.preset.name);message("已保存“"+saved.preset.name+"”，原有预设未覆盖。");
        } catch(error) { report(error); }
    };
    win.onResizing=win.onResize=function(){this.layout.resize();};
    refreshList();
    win.layout.layout(true);
    if(win instanceof Window) { win.center();win.show(); }
}(this));
