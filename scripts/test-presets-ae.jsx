// Native AE integration: grouped parameters, preset round-trip and safeguards.
// Uses an isolated empty --noui session. Never writes to the real user library.
(function () {
    var root = File($.fileName).parent.parent;
    var output = new Folder(root.fsName + "/test-output"), lines = [], owned = false;
    function assert(condition, message) { if (!condition) throw Error(message); }
    function record(message) { lines.push("PASS: " + message); }
    function near(actual, expected, message) { assert(Math.abs(actual - expected) < 0.0001, message); }
    try {
        assert(!app.project || (!app.project.file && app.project.numItems === 0), "Existing project left untouched.");
        owned = true;
        $.evalFile(new File(root.fsName + "/scripts/NebulaParticlePresetLib.jsxinc"));
        if (!app.project) app.newProject();
        var comp = app.project.items.addComp("Preset integration", 640, 360, 1, 4, 25);
        var source = comp.layers.addSolid([1,1,1], "Source", 640, 360, 1, 4);
        var mask = comp.layers.addSolid([1,1,1], "Mask", 640, 360, 1, 4);
        var map = comp.layers.addSolid([1,0.2,0.4], "Color", 640, 360, 1, 4);
        var carrier = comp.layers.addSolid([0,0,0], "Nebula Particle", 640, 360, 1, 4);
        var fx = carrier.property("ADBE Effect Parade").addProperty("ParticleAtelier.Particles");
        assert(fx.name === "Nebula Particle", "Renamed native effect was not loaded");
        assert(PA_Presets.version === "0.5.0", "Unexpected preset library version");
        var get = function (name) { return PA_Presets.findParameter(fx, name); };
        var preserved = {
            "Source (None = this layer)":source.index,
            "External emission mask":mask.index,
            "Color map (None = source)":map.index,
            "Emit from":3, "Random seed":47, "Prewarm at frame zero":0,
            "Stop emission (-1=never)":2.5, "Show source":0
        };
        for (var name in preserved) if (preserved.hasOwnProperty(name)) get(name).setValue(preserved[name]);
        var builtins = PA_Presets.builtins();
        assert(builtins.length === 6, "Expected six built-in presets");
        for (var b = 0; b < builtins.length; b++) {
            PA_Presets.applyToEffect(fx, builtins[b]);
            near(get("rate").value, builtins[b].values.rate, "Preset density not applied");
            near(get("damping").value, builtins[b].values.damping, "Preset damping not applied");
            for (var link in preserved) if (preserved.hasOwnProperty(link)) near(get(link).value, preserved[link], "Preset changed protected property: " + link);
        }
        record("All six built-in presets apply to real grouped native parameters; source/channel/timing links are retained.");
        get("size").selected = true; get("opacity").selected = true;
        var target = PA_Presets.selectedEffect(comp);
        assert(target.propertyIndex === fx.propertyIndex, "Selecting two subparameters did not resolve one effect");
        carrier.locked = true;
        var lockedRefused = false;
        try { PA_Presets.applyToEffect(fx, builtins[0]); } catch (lockedError) { lockedRefused = true; }
        assert(lockedRefused, "Locked layer was modified");
        carrier.locked = false;
        get("size").selected = false; get("opacity").selected = false;
        var percentRefused = false;
        try { PA_Presets.safeName("%2E%2E%2Fescape"); } catch (nameError) { percentRefused = true; }
        assert(percentRefused, "URI-escaped filename was accepted");
        record("Multiple selected subparameters resolve one effect; locked targets and escaped filenames are rejected.");
        get("size").setValue(3.4); get("damping").setValue(2.75); get("opacity").setValue(61);
        var captured = PA_Presets.captureEffect(fx, "微光拖尾 集成测试", "中文名称与说明，真实磁盘往返。");
        var folder = new Folder(output.fsName + "/preset-roundtrip-" + new Date().getTime());
        var saved = PA_Presets.saveUserPreset(captured, folder);
        var duplicate = PA_Presets.saveUserPreset(captured, folder);
        assert(saved.file.exists && duplicate.file.exists && saved.file.fsName !== duplicate.file.fsName, "Duplicate save overwrote preset");
        var listed = PA_Presets.listUserPresets(folder);
        assert(listed.presets.length === 2 && listed.errors.length === 0, "Saved presets cannot be listed");
        var loaded = PA_Presets.readPreset(saved.file);
        assert(PA_Presets.serialize(loaded) === PA_Presets.serialize(captured), "Preset file round-trip changed values");
        PA_Presets.applyToEffect(fx, builtins[0]);
        PA_Presets.applyToEffect(fx, loaded);
        near(get("size").value, 3.4, "Custom size was not restored");
        near(get("damping").value, 2.75, "Custom damping was not restored");
        near(get("opacity").value, 61, "Custom opacity was not restored");
        record("Chinese user preset saves, reads, lists and restores; duplicate names create separate files.");
        get("size").setValueAtTime(0, 2); get("size").setValueAtTime(1, 4);
        var before = get("rate").value, refused = false;
        try { PA_Presets.applyToEffect(fx, builtins[1]); } catch (expected) { refused = true; }
        assert(refused && get("size").numKeys === 2 && get("rate").value === before, "Animated effect was partially overwritten");
        record("Keyframed target is rejected before any values change.");
        get("size").removeKey(2); get("size").removeKey(1);
        PA_Presets.applyToEffect(fx, loaded);
        var project = new File(output.fsName + "/Preset roundtrip.aep");
        app.project.save(project);
        app.project.close(CloseOptions.DO_NOT_SAVE_CHANGES);
        app.open(project);
        comp = app.project.item(app.project.numItems);
        for (var c = 1; c <= app.project.numItems; c++) if (app.project.item(c) instanceof CompItem) comp = app.project.item(c);
        fx = comp.layer(1).property("ADBE Effect Parade").property(1);
        PA_Presets.applyToEffect(fx, PA_Presets.readPreset(saved.file));
        near(get("damping").value, 2.75, "Preset could not be reapplied after AEP reopen");
        record("Saved AEP reopens and user preset can be applied again.");
    } catch (e) { lines.push("FAIL: " + e.toString() + " line " + e.line); }
    finally {
        output.create();
        var log = new File(output.fsName + "/preset-integration.txt"); log.encoding = "UTF-8";
        if (log.open("w")) { log.write(lines.join("\n")); log.close(); }
        $.writeln(lines.join("\n"));
        if (owned) { app.project.close(CloseOptions.DO_NOT_SAVE_CHANGES); app.quit(); }
    }
}());
