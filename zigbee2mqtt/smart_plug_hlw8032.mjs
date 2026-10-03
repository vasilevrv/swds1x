import * as m from 'zigbee-herdsman-converters/lib/modernExtend';
import {presets as e, access as ea} from 'zigbee-herdsman-converters/lib/exposes';

// Matches smart_plug/zcl_clusters.c and smart_plug/smart_plug.c.
const configCluster = 'smartPlugConfig';
const intervalAttribute = 'telemetryIntervalMs';
const minimumInterval = 100;
const maximumInterval = 3600000;
const configDefinition = {
    name: configCluster,
    ID: 0xfc00,
    attributes: {
        telemetryIntervalMs: {
            name: intervalAttribute, ID: 0x0000, type: 0x23, // uint32
            read: true, write: true, min: minimumInterval, max: maximumInterval,
        },
    },
    commands: {},
    commandsResponse: {},
};

const electrical = {
    cluster: 'haElectricalMeasurement',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg) => {
        const result = {};
        const attributes = [
            ['rmsVoltage', 'voltage', 10, 0xffff],
            ['rmsCurrent', 'current', 1000, 0xffff],
            ['activePower', 'power', 1, -32768],
            ['apparentPower', 'apparent_power', 1, 0xffff],
            ['powerFactor', 'power_factor', 100, -128],
        ];
        for (const [attribute, property, divisor, invalid] of attributes) {
            const value = msg.data[attribute];
            if (value === null || (typeof value === 'number' && !Number.isFinite(value))) {
                result[property] = null;
            } else if (typeof value === 'number') {
                result[property] = value === invalid ? null : value / divisor;
            }
        }
        return result;
    },
};

const metering = {
    cluster: 'seMetering',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg) => {
        const raw = msg.data.currentSummDelivered;
        if (raw === undefined) return;
        // Herdsman versions use a number or [high16, low32] for uint48.
        const value = Array.isArray(raw) ? raw[0] * 0x100000000 + raw[1] : raw;
        if (!Number.isSafeInteger(value) || value < 0 || value > 0xffffffffffff) return;
        return {energy: value === 0xffffffffffff ? null : value / 1000000};
        // instantaneousDemand is deliberately ignored: power comes from
        // haElectricalMeasurement, avoiding two sources for the same property.
    },
};

const interval = {
    cluster: configCluster,
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg) => {
        const value = msg.data[intervalAttribute] ?? msg.data[0];
        if (Number.isInteger(value) && value >= minimumInterval && value <= maximumInterval) {
            return {telemetry_interval_ms: value};
        }
    },
};

const setInterval = {
    key: ['telemetry_interval_ms'],
    convertSet: async (entity, key, value) => {
        if (!Number.isInteger(value) || value < minimumInterval || value > maximumInterval) {
            throw new Error('telemetry_interval_ms must be an integer from 100 to 3600000');
        }
        await entity.write(configCluster, {[intervalAttribute]: value});
        const response = await entity.read(configCluster, [intervalAttribute]);
        const actual = response[intervalAttribute] ?? response[0];
        if (!Number.isInteger(actual) || actual < minimumInterval || actual > maximumInterval) {
            throw new Error(`Invalid telemetry interval readback: ${JSON.stringify(response)}`);
        }
        // Return the confirmed value even when readResponse event publication
        // differs between Zigbee2MQTT versions.
        return {state: {telemetry_interval_ms: actual}};
    },
    convertGet: async (entity) => {
        await entity.read(configCluster, [intervalAttribute]);
    },
};

const getMeasurements = {
    key: ['voltage', 'current', 'power', 'apparent_power', 'power_factor', 'energy'],
    convertGet: async (entity, key) => {
        if (key === 'energy') {
            await entity.read('seMetering', ['currentSummDelivered']);
        } else {
            const attributes = {
                voltage: 'rmsVoltage', current: 'rmsCurrent', power: 'activePower',
                apparent_power: 'apparentPower', power_factor: 'powerFactor',
            };
            await entity.read('haElectricalMeasurement', [attributes[key]]);
        }
    },
};

export default {
    fingerprint: [
        {modelID: 'SWPLUG1x', manufacturerName: 'Salu Workshop'},
        // Recognize devices still running the previous firmware identity.
        {modelID: 'SMART_PLUG_BL0937', manufacturerName: 'TELINK'},
        {modelID: 'SMART_PLUG_HLW8032', manufacturerName: 'TELINK'},
    ],
    model: 'SWPLUG1x',
    vendor: 'Salu Workshop',
    description: 'TLSR8258 smart plug with BL0937 or HLW8032 energy meter',
    ota: true,
    extend: [
        m.deviceAddCustomCluster(configCluster, configDefinition),
        m.onOff({powerOnBehavior: true}),
    ],
    fromZigbee: [electrical, metering, interval],
    toZigbee: [setInterval, getMeasurements],
    exposes: [
        e.numeric('power', ea.STATE_GET).withUnit('W').withDescription('Average active power over the telemetry window'),
        e.numeric('voltage', ea.STATE_GET).withUnit('V').withDescription('Average RMS voltage over the telemetry window'),
        e.numeric('current', ea.STATE_GET).withUnit('A').withDescription('Average RMS current over the telemetry window'),
        e.numeric('energy', ea.STATE_GET).withUnit('kWh').withDescription('Lifetime consumed energy; factory reset clears it'),
        e.numeric('apparent_power', ea.STATE_GET).withUnit('VA').withDescription('Average apparent power over the telemetry window'),
        e.numeric('power_factor', ea.STATE_GET).withValueMin(0).withValueMax(1).withDescription('Ratio of average active power to average apparent power'),
        e.numeric('telemetry_interval_ms', ea.ALL).withUnit('ms')
            .withValueMin(minimumInterval).withValueMax(maximumInterval).withValueStep(1)
            .withCategory('config').withDescription('Aggregation/report interval in milliseconds; firmware default is 10000'),
    ],
    configure: async (device) => {
        device.addCustomCluster(configCluster, configDefinition);
        const endpoint = device.getEndpoint(1);
        // Firmware sends metering reports directly to coordinator endpoint 1.
        // These attributes are READ-only: do not Configure Reporting for them.
        await endpoint.read(configCluster, [intervalAttribute]);
        await endpoint.read('haElectricalMeasurement', ['rmsVoltage', 'rmsCurrent', 'activePower']);
        await endpoint.read('haElectricalMeasurement', ['apparentPower', 'powerFactor']);
        await endpoint.read('seMetering', ['currentSummDelivered']);
    },
};
