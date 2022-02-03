const BatteryService = require("../lib/bindings.js");
const assert = require("assert");

assert(BatteryService, "The expected module is undefined");

function testBasic()
{
    const instance = new BatteryService();
    assert(instance.getBatteryList, "The expected method is not defined");
    assert.ok(instance.getBatteryList() != null, "Unexpected value returned");
}

assert.doesNotThrow(testBasic, undefined, "testBasic threw an expection");

console.log("Tests passed- everything looks OK!");