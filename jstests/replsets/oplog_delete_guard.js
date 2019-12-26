// Test oplog delete guard
// set oplogDeleteGuard=Timestamp(0,0) forbid this function
// set oplogDeleteGuard>Timestamp(0,0) enable this function
// set oplogSize=1MB
(function() {
    "use strict";
    var rt = new ReplSetTest({name: "oplog_delete_guard", nodes: 3, oplogSize: 1});

    var nodes = rt.startSet();
    rt.initiate();
    var master = rt.getPrimary();
    var coll = master.getCollection("test.Test");

    jsTest.log("****************insert test.Test1****************")
    var valStr = "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV"
    for (var i = 0; i < 2048; i++) {
	var res =
            coll.runCommand({insert: coll.getName(), documents: [{key:i, val:valStr}], writeConcern: {w: 2}});
	assert(res.ok);
    }
    assert.eq(2048, coll.count());

    var oplog = master.getCollection("local.oplog.rs");
    var num = oplog.find().count()
    jsTest.log("oplog num = " + num)
    assert.lte(num, 1024, "oplog size is greater 1M");

    var res = oplog.findOne();
    jsTest.log("res = " + tojson(res))
    var local = master.getDB('local');
    var stamp = res['ts']
    local.runCommand({collMod : "oplog.rs", oplogDeleteGuard: stamp});

    jsTest.log("****************insert test.Test2****************")
    
    for (var i = 0; i < 2048; i++) {
	var res =
            coll.runCommand({insert: coll.getName(), documents: [{key:i, val:valStr}], writeConcern: {w: 2}});
	assert(res.ok);
    }
    num = oplog.find().count()
    jsTest.log("oplog num = " + num)
    assert.gte(num, 1024, "oplog size is greater 1M");

    var res1 = oplog.stats()
    var secondary = rt.getSecondary();
    var res2 = secondary.getCollection("local.oplog.rs").stats()
    assert.eq(res1["oplogDeleteGuard"], res2["oplogDeleteGuard"], "oplog configure is not sync between mongodb replset")

    res = oplog.find({ts:stamp}).count()
    assert.gte(res, 1, "can not guard stamp " + stamp)

    res = oplog.findOne({ts: {$gt : stamp}})
    jsTest.log("res = " + tojson(res))
    var stamp2 = res['ts']
    local.runCommand({collMod : "oplog.rs", oplogDeleteGuard: stamp2});
    for (var i = 0; i < 2048; i++) {
    	var res =
            coll.runCommand({insert: coll.getName(), documents: [{key:i, val:valStr}], writeConcern: {w: 2}});
    	assert(res.ok);
    }
    res = oplog.find({ts:stamp}).count()
    assert.lte(res, 1, "can guard stamp " + stamp)
}());
