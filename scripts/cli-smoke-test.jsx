// Used only with AE's own --unified --noui --execute command-line mode.
// Never closes, replaces or edits a pre-existing project.
(function () {
    var root = File($.fileName).parent.parent;
    if (app.project && (app.project.numItems > 0 || app.project.file)) {
        $.writeln("REFUSED: Existing project left untouched.");
        return;
    }
    var owned = false;
    var report = [];
    function record(s) { report.push(s); $.writeln(s); }
    try {
        $.evalFile(new File(root.fsName + "/scripts/create-test-project.jsx"));
        if (!app.project.file || app.project.file.parent.fsName !== root.fsName + "/test-output") {
            throw Error("Test project was not created.");
        }
        owned = true;
        record("AE " + app.version + " native plugin smoke test");
        record("Project " + app.project.file.fsName);
        var comps = [];
        for (var i = 1; i <= app.project.numItems; i++) {
            if (app.project.item(i) instanceof CompItem) comps.push(app.project.item(i));
        }
        var mainFile = app.project.file;
        function queueFrame(comp, time, label) {
            var item = app.project.renderQueue.items.add(comp);
            item.timeSpanStart = time; item.timeSpanDuration = 1 / 25;
            item.outputModule(1).applyTemplate("Multi-Machine Sequence");
            item.outputModule(1).file = new File(root.fsName + "/test-output/" + label + "_[####].psd");
        }
        for (var j = 0; j < comps.length; j++) {
            var at = j === 3 ? 2.4 : (j === 4 ? 4.4 : 1.6);
            queueFrame(comps[j], at, "ae-16-comp-" + j);
        }
        queueFrame(comps[0], 0.6, "ae-16-earlier");
        queueFrame(comps[0], 1.6, "ae-16-repeat");
        app.project.save(mainFile);
        record("Prepared 16 bpc render queue. Actual rendering must use aerender.");
        app.project.bitsPerChannel = 8;
        var originalOutputs = [];
        for (var q = 1; q <= app.project.renderQueue.numItems; q++) {
            var module = app.project.renderQueue.item(q).outputModule(1);
            originalOutputs.push(module.file.fsName);
            module.file = new File(module.file.fsName.replace("ae-16-", "ae-8-"));
        }
        var eightBitFile = new File(root.fsName + "/test-output/Particle Atelier Test 8bit.aep");
        app.project.save(eightBitFile);
        record("Prepared 8 bpc project: " + eightBitFile.fsName);
        app.project.bitsPerChannel = 16;
        for (var q2 = 1; q2 <= app.project.renderQueue.numItems; q2++) {
            app.project.renderQueue.item(q2).outputModule(1).file = new File(originalOutputs[q2-1]);
        }
        app.project.save(mainFile);
        record("PASS: Native plugin loaded and test projects saved. Render verification remains pending.");
    } catch (e) {
        record("FAIL: " + e.toString() + " line " + e.line);
    } finally {
        try {
            var log = new File(root.fsName + "/test-output/ae-render-report.txt");
            log.encoding = "UTF-8"; log.open("w"); log.write(report.join("\n")); log.close();
        } catch (ignored) {}
        if (owned) {
            app.project.close(CloseOptions.DO_NOT_SAVE_CHANGES);
            app.quit();
        }
    }
})();
