// Read-only compatibility check for an optional local v0.1 AEP fixture.
// Run with AE --unified --noui --execute, with no pre-existing project.
(function () {
    var root = File($.fileName).parent.parent;
    var output = new Folder(root.fsName + "/test-output");
    var lines = [], owned = false;
    function assert(condition, message) { if (!condition) throw Error(message); }
    function find(group, name) {
        var direct = group.property(name);
        if (direct) return direct;
        for (var i = 1; i <= group.numProperties; i++) {
            var p = group.property(i);
            if (p.numProperties) { var child = find(p, name); if (child) return child; }
        }
        return null;
    }
    try {
        assert(!app.project || (!app.project.file && app.project.numItems === 0), "Existing project left untouched.");
        owned = true;
        var fixture = new File(output.fsName + "/legacy-v0.1.aep");
        assert(fixture.exists, "Place an unchanged v0.1 test AEP at test-output/legacy-v0.1.aep first.");
        app.open(fixture);
        var tested = 0;
        for (var i = 1; i <= app.project.numItems; i++) {
            var comp = app.project.item(i);
            if (!(comp instanceof CompItem) || comp.name.indexOf("01 -") !== 0) continue;
            var effect = comp.layer(1).property("ADBE Effect Parade").property(1);
            assert(effect.matchName === "ParticleAtelier.Particles", "Effect identity changed");
            var expected = {
                "Source (None = this layer)":4, "Color map (None = source)":2,
                "Density (births/sec)":750, "Size (px)":2, "Lifetime (sec)":1.8,
                "Initial speed (px/sec)":95, "Direction (0=right)":-70,
                "Spread (degrees)":50, "Damping (per sec)":0,
                "Glow (%)":6, "Twinkle strength (%)":35,
                "Prewarm at frame zero":0, "Show source":1, "Particle shape":2
            };
            for (var name in expected) {
                if (!expected.hasOwnProperty(name)) continue;
                var property = find(effect, name);
                assert(property && Math.abs(property.value - expected[name]) < 0.0001, "Legacy value changed: " + name);
            }
            lines.push("PASS: v0.1 values and layer links retained after Nebula Particle 0.5.0 upgrade.");
            tested++;
        }
        assert(tested === 1, "Expected legacy test composition was not found.");
    } catch (e) { lines.push("FAIL: " + e.toString() + " line " + e.line); }
    finally {
        output.create();
        var log = new File(output.fsName + "/legacy-compatibility.txt");
        log.encoding = "UTF-8";
        if (log.open("w")) { log.write(lines.join("\n")); log.close(); }
        $.writeln(lines.join("\n"));
        if (owned) { app.project.close(CloseOptions.DO_NOT_SAVE_CHANGES); app.quit(); }
    }
}());
