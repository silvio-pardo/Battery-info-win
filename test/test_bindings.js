const BatteryService = require("../lib/bindings.js");
const assert = require("assert");

assert(BatteryService, "The expected module is undefined");

function testBasic()
{
    const instance = new BatteryService();
    //batterylist
    assert(instance.getBatteryList, "The expected method is not defined");
    const returnObject = instance.getBatteryList();
    console.log(returnObject);
    assert.ok(returnObject != null, "Unexpected value returned");
    //systeminfo
    assert(instance.getSystemInfo, "The expected method is not defined");
    const returnObject_1 = instance.getSystemInfo();
    console.log(returnObject_1);
    assert.ok(returnObject_1 != null, "Unexpected value returned");
}

assert.doesNotThrow(testBasic, undefined, "testBasic threw an expection");

console.log("Tests passed- everything looks OK!");