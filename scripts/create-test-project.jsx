// Run in a NEW/EMPTY AE session: File > Scripts > Run Script File.
// Refuses to replace an existing project. No user preference changes.
(function () {
    var root = File($.fileName).parent.parent;
    var output = new Folder(root.fsName + "/test-output");
    var statusPath = new File(output.fsName + "/ae-test-status.txt");
    function status(text) {
        try { statusPath.encoding = "UTF-8"; statusPath.open("w"); statusPath.write(text); statusPath.close(); } catch (ignored) {}
        $.writeln(text);
    }
    function sequence(folder) {
        var file = new File(root.fsName + "/test-output/fixtures/" + folder + "/frame_0000.png");
        if (!file.exists) throw Error("Run scripts/make-fixtures.py first.");
        var options = new ImportOptions(file); options.sequence = true;
        var footage = app.project.importFile(options);
        footage.mainSource.conformFrameRate = 25;
        return footage;
    }
    function set(effect, name, value) {
        var p = effect.property(name);
        if (!p) throw Error("Missing plugin parameter: " + name);
        p.setValue(value);
    }
    function makeComp(name, source, mask, map, damping) {
        var duration = name.indexOf("After footage") >= 0 ? 6 : 4;
        var c = app.project.items.addComp(name, 640, 360, 1, duration, 25);
        var src = c.layers.add(source); src.name = "Baked moving RGBA sequence";
        var matte = c.layers.add(mask); matte.name = "External luminance pass";
        var color = c.layers.add(map); color.name = "Independent color map";
        var layer = c.layers.addSolid([0, 0, 0], "Nebula Particle", 640, 360, 1, duration);
        var fx = layer.property("ADBE Effect Parade").addProperty("ParticleAtelier.Particles");
        if (!fx) throw Error("Nebula Particle did not load. Install the plugin and start a fresh AE session.");
        if (fx.name !== "Nebula Particle") throw Error("An older plugin build is loaded; restart AE after installing Nebula Particle.");
        // Assign after all layers have been added, since inserting layers changes indices.
        set(fx, "Source (None = this layer)", src.index);
        set(fx, "Color map (None = source)", color.index);
        set(fx, "Density (births/sec)", 750);
        set(fx, "Size (px)", 2.0);
        set(fx, "Lifetime (sec)", 1.8);
        set(fx, "Initial speed (px/sec)", 95);
        set(fx, "Direction (0=right)", -70);
        set(fx, "Spread (degrees)", 50);
        set(fx, "Damping (per sec)", damping);
        set(fx, "Glow (%)", 6);
        set(fx, "Twinkle strength (%)", 35);
        set(fx, "Prewarm at frame zero", 0);
        set(fx, "Show source", 1);
        set(fx, "Particle shape", 2);
        if (name.indexOf("External") >= 0) {
            set(fx, "Emit from", 3);
            set(fx, "External emission mask", matte.index);
        }
        src.enabled = false; matte.enabled = false; color.enabled = false;
        c.time = 1.6; c.workAreaStart = 0; c.workAreaDuration = 4;
        return c;
    }
    try {
        if (app.project && (app.project.numItems > 0 || app.project.file)) {
            status("REFUSED: An existing project is open. It was not modified.");
            return;
        }
        if (!app.project) app.newProject();
        app.beginUndoGroup("Create Nebula Particle test project");
        app.project.bitsPerChannel = 16;
        var source = sequence("rgba"), mask = sequence("luma");
        var map = app.project.importFile(new ImportOptions(new File(root.fsName + "/test-output/fixtures/color-map.png")));
        var trail = makeComp("01 - Alpha trail - damping 0", source, mask, map, 0);
        makeComp("02 - Alpha trail - damping 2", source, mask, map, 2);
        makeComp("03 - External luminance trail", source, mask, map, 1);
        var stop = makeComp("04 - Stop emission test", source, mask, map, 0);
        var stopFx = stop.layer(1).property("ADBE Effect Parade").property(1);
        set(stopFx, "Stop emission (-1=never)", 1);
        set(stopFx, "Lifetime (sec)", 1);
        set(stopFx, "Show source", 0);
        stop.time = 2.4;
        var tail = makeComp("05 - After footage end", source, mask, map, 1);
        set(tail.layer(1).property("ADBE Effect Parade").property(1), "Show source", 0);
        tail.time = 4.4;
        var outputFile = new File(output.fsName + "/Nebula Particle Test.aep");
        if (outputFile.exists) {
            outputFile = new File(output.fsName + "/Nebula Particle Test " + (new Date()).getTime() + ".aep");
        }
        app.project.save(outputFile);
        trail.openInViewer();
        app.endUndoGroup();
        status("PASS: Native effect added with all parameters.\nProject: " + outputFile.fsName + "\nAE " + app.version + "\n16 bpc, 640x360, 25 fps, baked moving PNG sequence.\nNo optical flow/GPU/3D claim. Render verification is a separate step.");
    } catch (error) {
        try { app.endUndoGroup(); } catch (ignored) {}
        status("FAIL: " + error.toString() + " line " + error.line);
        alert("Nebula Particle test: " + error.toString());
    }
})();
