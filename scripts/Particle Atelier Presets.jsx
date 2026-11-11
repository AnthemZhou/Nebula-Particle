#target aftereffects
#include "ParticleAtelierPresetLib.jsxinc"

(function (thisObj) {
    var api=PA_Presets;
    var win=(thisObj instanceof Panel) ? thisObj : new Window("palette","Particle Atelier · 粒子预设",undefined,{resizeable:true});
    win.orientation="column"; win.alignChildren=["left","top"]; win.spacing=0; win.margins=14;
    // Includes fixed text/button heights, seven gaps and outer margins.
    win.minimumSize=[300,620]; win.preferredSize=[360,660];
    // A dock can initially inherit the whole workspace. Keep the working area
    // compact instead of allowing buttons to stretch with that host rectangle.
    var content=win.add("group"); content.orientation="column"; content.alignChildren=["fill","top"];
    content.alignment=["fill","top"]; content.spacing=10; content.margins=0;
    content.minimumSize=[260,0]; content.maximumSize=[480,10000];
    function fixedHeight(control,height) {
        control.alignment=["fill","top"];
        control.minimumSize=[0,height]; control.preferredSize=[300,height]; control.maximumSize=[480,height];
    }
    var title=content.add("statictext",undefined,"Particle Atelier · 粒子预设"); fixedHeight(title,22);
    var sourceRow=content.add("group"); sourceRow.orientation="row"; sourceRow.alignChildren=["fill","center"];
    fixedHeight(sourceRow,30);
    var source=sourceRow.add("dropdownlist",undefined,["内置预设","我的预设"]); source.selection=0; source.alignment=["fill","center"];
    source.minimumSize=[160,28]; source.preferredSize=[228,28]; source.maximumSize=[406,28];
    var refresh=sourceRow.add("button",undefined,"刷新");
    refresh.alignment=["right","center"]; refresh.minimumSize=refresh.preferredSize=refresh.maximumSize=[64,28];
    var list=content.add("listbox",undefined,[],{multiselect:false});
    list.minimumSize=[240,120]; list.preferredSize=[300,170]; list.maximumSize=[480,300]; list.alignment=["fill","fill"];
    var description=content.add("statictext",undefined,"",{multiline:true}); fixedHeight(description,65);
    var apply=content.add("button",undefined,"应用到所选图层 / 效果"); fixedHeight(apply,32);
    var save=content.add("button",undefined,"保存当前效果为我的预设…"); fixedHeight(save,32);
    var note=content.add("statictext",undefined,"保留素材、发射范围、随机种子与时间设置。\n含关键帧或表达式的目标参数不会被覆盖。",{multiline:true}); fixedHeight(note,60);
    var status=content.add("statictext",undefined,"选中一个图层，或其中的 Particle Atelier 效果。",{multiline:true}); fixedHeight(status,44);
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
    if(win instanceof Window) { win.size=[360,560];win.center();win.show(); }
}(this));
