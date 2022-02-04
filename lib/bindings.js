if(process.platform == "win32") { 
    const addon = require('../build/Release/batteryservice-native');
    function BatteryService(name) {
        this.getBatteryList = function() {
            return _addonInstance.getBatteryList();
        }

        var _addonInstance = new addon.BatteryService(name);
    }

    module.exports = BatteryService;
}else{ 
    module.exports = null;
}