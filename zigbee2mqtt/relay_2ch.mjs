import * as m from 'zigbee-herdsman-converters/lib/modernExtend';
import {presets as e, access as ea} from 'zigbee-herdsman-converters/lib/exposes';

const cluster = 'relay2Config';
const clusterDefinition = {
    name: cluster,
    ID: 0xfc00,
    attributes: {
        buttonLinked: {name: 'buttonLinked', ID: 0, type: 0x10, read: true, write: true},
        doubleClickEnabled: {name: 'doubleClickEnabled', ID: 1, type: 0x10, read: true, write: true},
    },
    commands: {},
    commandsResponse: {
        buttonEvent: {name: 'buttonEvent', ID: 0, parameters: [{name: 'event', type: 0x30}]},
    },
};
const settings = {
    button_1_linked: {endpoint: 1, attribute: 'buttonLinked', id: 0},
    button_2_linked: {endpoint: 2, attribute: 'buttonLinked', id: 0},
    button_1_double_click: {endpoint: 1, attribute: 'doubleClickEnabled', id: 1},
    button_2_double_click: {endpoint: 2, attribute: 'doubleClickEnabled', id: 1},
};
const asBoolean = (value) => {
    if (value === true || value === 1) return true;
    if (value === false || value === 0) return false;
    throw new Error('Expected a Boolean Zigbee attribute');
};
const config = {
    cluster,
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg) => {
        const result = {};
        for (const [property, setting] of Object.entries(settings)) {
            if (msg.endpoint.ID !== setting.endpoint) continue;
            const value = msg.data[setting.attribute] ?? msg.data[setting.id];
            if (value === undefined) continue;
            try { result[property] = asBoolean(value); } catch { /* Ignore invalid readbacks. */ }
        }
        return result;
    },
};
const actions = {
    cluster,
    type: ['commandButtonEvent'],
    convert: (model, msg) => {
        if (msg.endpoint.ID !== 1 && msg.endpoint.ID !== 2) return;
        const event = msg.data.event;
        if (!Number.isInteger(event) || event < 0 || event > 2) return;
        return {action: `button_${msg.endpoint.ID}_${['single', 'double', 'hold'][event]}`};
    },
};
const setConfig = {
    key: Object.keys(settings),
    convertSet: async (entity, key, value, meta) => {
        if (typeof value !== 'boolean') throw new Error(`${key} must be true or false`);
        const setting = settings[key];
        if (!setting || !meta.device) throw new Error('Button configuration requires a device');
        // A converter reload may occur after the device's start/configure hooks.
        meta.device.addCustomCluster(cluster, clusterDefinition);
        const endpoint = meta.device.getEndpoint(setting.endpoint);
        await endpoint.write(cluster, {[setting.attribute]: value});
        const response = await endpoint.read(cluster, [setting.attribute]);
        return {state: {[key]: asBoolean(response[setting.attribute] ?? response[setting.id])}};
    },
    convertGet: async (entity, key, meta) => {
        const setting = settings[key];
        if (!setting || !meta.device) throw new Error('Button configuration requires a device');
        meta.device.addCustomCluster(cluster, clusterDefinition);
        await meta.device.getEndpoint(setting.endpoint).read(cluster, [setting.attribute]);
    },
};

export default {
    fingerprint: [{modelID: 'swrelay2x', manufacturerName: 'Salu Workshop'}],
    model: 'swrelay2x',
    vendor: 'Salu Workshop',
    description: 'TLSR8258 two-channel relay with independent button settings and actions',
    ota: true,
    extend: [
        m.deviceAddCustomCluster(cluster, clusterDefinition),
        m.deviceEndpoints({endpoints: {l1: 1, l2: 2}, multiEndpointSkip: ['action', ...Object.keys(settings)]}),
        m.onOff({endpointNames: ['l1', 'l2'], powerOnBehavior: true}),
    ],
    fromZigbee: [config, actions],
    toZigbee: [setConfig],
    exposes: [
        ...Object.keys(settings).map((key) => e.binary(key, ea.ALL, true, false).withCategory('config')
            .withDescription(key.endsWith('_linked') ? 'Single click toggles the matching relay' : 'Enable double click; single click waits 400 ms')),
        e.action(['button_1_single', 'button_1_double', 'button_1_hold', 'button_2_single', 'button_2_double', 'button_2_hold']),
    ],
    configure: async (device) => {
        device.addCustomCluster(cluster, clusterDefinition);
        for (const id of [1, 2]) await device.getEndpoint(id).read(cluster, ['buttonLinked', 'doubleClickEnabled']);
        // Button events go directly to coordinator endpoint 1; no event binding
        // or Configure Reporting for configuration attributes is required.
    },
};
