"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const api = require("../scripts/NebulaParticlePresetLib.jsxinc");
const clone = value => JSON.parse(JSON.stringify(value));
let passed = 0;
function test(name, action) { action(); passed++; process.stdout.write(`PASS ${name}\n`); }

function mockEffect() {
    const base = api.builtins()[0].values;
    const leaves = {};
    const extras = [
        ["Source (None = this layer)", 8], ["Color map (None = source)", 9],
        ["External emission mask", 10], ["Emit from", 3], ["Random seed", 432],
        ["Prewarm at frame zero", 0], ["Stop emission (-1=never)", 7], ["Preset", 6], ["Show source", 0]
    ];
    const makeLeaf = (name, value) => ({name, matchName:name, value:clone(value), numProperties:0, numKeys:0,
        expression:"", writes:0, setValue(v) { this.writes++; this.value=clone(v); }});
    const visual = api.fields.map(spec => leaves[spec.key] = makeLeaf(spec.name, base[spec.key]));
    const untouched = extras.map(([name,value]) => makeLeaf(name,value));
    function group(name, children) {
        const g={name, matchName:name, numProperties:children.length, property(i) { return children[i-1]; }};
        children.forEach((p,i) => { p.parentProperty=g; p.propertyIndex=i+1; }); return g;
    }
    const effect=group("Renamed by user", [group("Sources",untouched), group("Appearance",visual.slice(0,7)),
        group("Advanced",[group("Motion",visual.slice(7,11)), group("Animation",visual.slice(11))])]);
    effect.matchName=api.matchName;
    return {effect,leaves,untouched,visual,snapshot:() => clone(visual.map(p=>p.value))};
}

test("six concrete builtins round-trip without evaluating data", () => {
    const presets=api.builtins(); assert.equal(presets.length,6);
    presets.forEach(p => assert.deepEqual(api.parse(api.serialize(p)),p));
    assert.equal(presets[4].values.speed,85);
    assert.equal(presets[4].values.damping,1.7);
    assert.equal(presets[5].values.colorMode,2);
    assert.equal(presets[0].values.glow,8);
    assert.equal(presets[1].values.glow,12);
    assert.deepEqual(Object.keys(presets[0].values).sort(),api.fields.map(f=>f.key).sort());
    presets[0].values.tint[0]=0;
    assert.notEqual(api.builtins()[0].values.tint[0],0,"builtins must return fresh records");
});

test("release branding preserves the legacy native and preset format identifiers", () => {
    assert.equal(api.version,"0.5.0");
    assert.equal(api.matchName,"ParticleAtelier.Particles");
    const oldText="PARTICLE_ATELIER_PRESET 1\nname=Legacy%20Dust\ndescription=Old%20project\n"+
        "rate=650\nsize=1.2\nlife=2.4\nshape=1\nblend=2\ncolorMode=1\ntint=0.5,0.7,1,1\n"+
        "speed=0\ndirection=0\nspread=360\ndamping=0\nfadeIn=4\nfadeOut=70\ntwinkle=15\ntwinkleHz=1.4\nglow=12\nopacity=85\n";
    const old=api.parse(oldText);
    assert.equal(old.name,"Legacy Dust");assert.equal(old.values.rate,650);
    assert.equal(api.serialize(old),oldText);
});

test("legacy merging only deduplicates identical content and labels differing namesakes", () => {
    const current=api.builtins()[0]; current.name="我的星尘";
    const different=clone(current); different.values.glow=2;
    const descriptionOnly=clone(current); descriptionOnly.description="旧版说明仍然保留";
    const legacyOnly=api.builtins()[1];legacyOnly.name="旧目录独有";
    const entries=(preset,filename)=>({preset,file:{name:filename}});
    const merged=api.mergePresetEntries([entries(current,"new.papreset")], [
        entries(clone(current),"duplicate.papreset"),entries(different,"different.papreset"),
        entries(descriptionOnly,"description.papreset"),entries(legacyOnly,"only.papreset")]);
    assert.equal(merged.length,4);
    assert.equal(merged.filter(p=>p.preset.name==="我的星尘").length,3);
    assert.equal(merged.find(p=>p.origin==="current").displayName,"我的星尘");
    assert.ok(merged.filter(p=>p.origin==="legacy").every(p=>p.displayName.includes("旧版")));
    const labels=merged.map(p=>p.displayName);assert.equal(new Set(labels).size,4);
    merged[0].preset.values.glow=99;assert.equal(current.values.glow,8);
});

test("default preset listing reads both libraries while saves target the new brand", () => {
    const newRoot="/user-data/Nebula Particle/User Presets", oldRoot="/user-data/Particle Atelier/User Presets";
    const folders={}, reads=[];
    function FakeFolder(name) { this.fsName=name;this.exists=!!folders[name]; }
    FakeFolder.userData={fsName:"/user-data"};
    FakeFolder.prototype.getFiles=function(){reads.push(this.fsName);return folders[this.fsName] || [];};
    function FakeFile(name,text) {this.name=name;this.length=text.length;this.text=text;}
    FakeFile.prototype.open=function(mode){assert.equal(mode,"r");return true;};
    FakeFile.prototype.read=function(){return this.text;};FakeFile.prototype.close=function(){};
    const p=api.builtins()[1];
    folders[newRoot]=[];folders[oldRoot]=[new FakeFile("legacy.papreset",api.serialize(p))];
    const context={Folder:FakeFolder,File:FakeFile};
    vm.createContext(context);
    vm.runInContext(fs.readFileSync(path.join(__dirname,"../scripts/NebulaParticlePresetLib.jsxinc"),"utf8"),context);
    assert.equal(context.PA_Presets.userFolder().fsName,newRoot);
    assert.equal(context.PA_Presets.legacyUserFolder().fsName,oldRoot);
    assert.equal(context.PA_Presets.listUserPresets().presets.length,1);
    assert.deepEqual(reads,[newRoot,oldRoot]);
    reads.length=0;
    assert.equal(context.PA_Presets.listUserPresets(new FakeFolder(newRoot)).presets.length,0);
    assert.deepEqual(reads,[newRoot],"explicit test/import folders must not merge unrelated user data");
});

test("strict parser rejects injected code, duplicate keys and unknown schema", () => {
    const text=api.serialize(api.builtins()[0]);
    const bad=[text.replace("rate=180","rate=180;globalThis.PA_TEST_INJECTION=true"),
        text+"rate=1\n",text+"__proto__=1\n",text+"constructor=1\n",
        text+"Source=5\n",text.replace("PRESET 1","PRESET 2"),text.replace("size=1.8","size=Infinity"),
        text.replace("size=1.8","size=NaN"),text.replace("name=","name=%XX"),"x".repeat(65537)];
    bad.forEach(s=>assert.throws(()=>api.parse(s)));
    assert.equal(globalThis.PA_TEST_INJECTION,undefined);
    assert.equal({}.polluted,undefined);
});

test("name, range and color validation reject unsafe or incompatible records", () => {
    ["", "../escape", ".hidden", "bad/name", "bad\\name", "bad:name", "bad\nname", "end.", "Light%20Dust", "%2E%2E%2F", "a%2fescape", "长".repeat(65)]
        .forEach(name=>assert.throws(()=>api.safeName(name)));
    assert.equal(api.safeName("  我的星尘  "),"我的星尘");
    for(const [key,value] of [["rate",10001],["life",0],["shape",1.5],["tint",[0,1,1,0.4]],["glow",-1]]) {
        const p=api.builtins()[0]; p.values[key]=value; assert.throws(()=>api.validate(p));
    }
    const missing=api.builtins()[0]; delete missing.values.opacity; assert.throws(()=>api.validate(missing));
    const forbidden=api.builtins()[0]; forbidden.values.seed=12; assert.throws(()=>api.validate(forbidden));
});

test("recursive application changes 17 visual fields and preserves links and time", () => {
    const m=mockEffect(), before=clone(m.untouched.map(p=>p.value));
    const result=api.applyToEffect(m.effect,api.builtins()[4]);
    assert.equal(result.applied,17);
    api.fields.forEach(f=>assert.deepEqual(m.leaves[f.key].value,api.builtins()[4].values[f.key]));
    assert.deepEqual(m.untouched.map(p=>p.value),before);
    assert.ok(m.untouched.every(p=>p.writes===0));
    assert.equal(api.findParameter(m.effect,"Damping (per sec)"),m.leaves.damping);
    m.leaves.size.name="Localized name";
    assert.equal(api.findParameter(m.effect,"size"),m.leaves.size,"matchName fallback");
});

test("capture and restore are detached from live AE color arrays", () => {
    const m=mockEffect(), saved=api.captureEffect(m.effect,"中文测试","运动序列的星点\n低光晕");
    m.leaves.tint.value[0]=0;
    assert.notEqual(saved.values.tint[0],0);
    api.applyToEffect(m.effect,api.builtins()[3]);
    api.applyToEffect(m.effect,api.parse(api.serialize(saved)));
    assert.deepEqual(api.captureEffect(m.effect,saved.name,saved.description),saved);
});

test("any keyed or expressed target rejects before the first parameter write", () => {
    for(const mode of ["key","expression"]) {
        const m=mockEffect(), before=m.snapshot();
        if(mode==="key") m.leaves.opacity.numKeys=2;
        else { m.leaves.opacity.expression="wiggle(2,5)"; m.leaves.opacity.expressionEnabled=false; }
        assert.throws(()=>api.applyToEffect(m.effect,api.builtins()[4]),/关键帧或表达式/);
        assert.deepEqual(m.snapshot(),before); assert.ok(m.visual.every(p=>p.writes===0));
    }
    const m=mockEffect(); m.leaves.opacity.name="Removed";m.leaves.opacity.matchName="Removed";
    assert.throws(()=>api.applyToEffect(m.effect,api.builtins()[4]));
    assert.ok(m.visual.every(p=>p.writes===0));
});

test("host assignment errors roll back earlier writes", () => {
    const m=mockEffect(), before=m.snapshot();
    m.leaves.fadeOut.setValue=function(){throw new Error("host rejected value");};
    assert.throws(()=>api.applyToEffect(m.effect,api.builtins()[4]),/已恢复原数值/);
    assert.deepEqual(m.snapshot(),before);
});

test("selection resolves only one intended Nebula Particle effect", () => {
    const m=mockEffect();
    const comp={selectedLayers:[],selectedProperties:[m.leaves.size]};
    assert.equal(api.selectedEffect(comp),m.effect);
    comp.selectedProperties.push(mockEffect().leaves.glow);
    assert.throws(()=>api.selectedEffect(comp),/多个/);
    assert.throws(()=>api.selectedEffect({selectedLayers:[{},{}],selectedProperties:[]}),/一个图层/);
});

test("selection deduplicates host wrappers and rejects locked selected effects", () => {
    const a=mockEffect(), b=mockEffect();
    a.effect.propertyIndex=b.effect.propertyIndex=2;
    a.effect.propertyDepth=b.effect.propertyDepth=2;
    const layer={id:123,index:1,locked:false};
    a.effect.propertyGroup=()=>layer; b.effect.propertyGroup=()=>({...layer});
    const comp={selectedLayers:[],selectedProperties:[a.leaves.size,b.leaves.glow]};
    assert.equal(api.selectedEffect(comp),a.effect);
    layer.locked=true;
    assert.throws(()=>api.selectedEffect(comp),/锁定/);
});

test("panel source parses after standard ExtendScript directives", () => {
    const filename=path.join(__dirname,"../scripts/Nebula Particle Presets.jsx");
    const source=fs.readFileSync(filename,"utf8").replace(/^#.*$/gm,"");
    new vm.Script(source,{filename});
});
process.stdout.write(`${passed} preset library checks passed. Real AE behavior requires host integration tests.\n`);
