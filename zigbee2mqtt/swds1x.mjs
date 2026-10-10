import {battery as batteryConverter} from 'zigbee-herdsman-converters/converters/fromZigbee';
import * as exposes from 'zigbee-herdsman-converters/lib/exposes';

// IAS Alarm1 means OPEN; Zigbee2MQTT contact=true means CLOSED.
export const contact = {
    cluster: 'ssIasZone',
    type: ['commandStatusChangeNotification', 'attributeReport', 'readResponse'],
    convert: (_model, msg) => {
        const status = msg.data.zonestatus ?? msg.data.zoneStatus;
        if (typeof status !== 'number') return;
        return {contact: (status & 1) === 0};
    },
};
export default {
    fingerprint: [{modelID: 'swds1x', manufacturerName: 'Salu Workshop'}],
    model: 'swds1x',
    vendor: 'Salu Workshop',
    description: 'Sleepy Zigbee door/window contact sensor',
    fromZigbee: [contact, batteryConverter],
    toZigbee: [],
    exposes: [exposes.presets.contact(), exposes.presets.battery(), exposes.presets.battery_voltage()],
    configure: async (device, coordinatorEndpoint) => {
        const endpoint = device.getEndpoint(1);
        // Enrollment itself is handled by herdsman's IAS interview.
        await endpoint.bind('ssIasZone', coordinatorEndpoint);
        await endpoint.read('ssIasZone', ['zoneStatus']);
        await endpoint.bind('genPowerCfg', coordinatorEndpoint);
        // Firmware reports once on startup and every 12 hours. Disable SDK
        // reporting timers, including settings persisted by older firmware.
        await endpoint.configureReporting('genPowerCfg', [
            {attribute: 'batteryPercentageRemaining', minimumReportInterval: 0, maximumReportInterval: 0xFFFF, reportableChange: 2},
            {attribute: 'batteryVoltage', minimumReportInterval: 0, maximumReportInterval: 0xFFFF, reportableChange: 1},
        ]);
        await endpoint.read('genPowerCfg', ['batteryVoltage', 'batteryPercentageRemaining']);
    },
};
