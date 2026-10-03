import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';

// Execute the real converter logic with substitutes for expose/extension
// factories only. This does not emulate a coordinator or herdsman transport.
const source = readFileSync(new URL('../zigbee2mqtt/smart_plug_hlw8032.mjs', import.meta.url), 'utf8')
    .replace(/^import .*;\r?\n/gm, '');
const substitutes = `
const chain = () => new Proxy({}, {get: () => () => chain()});
const e = {numeric: chain};
const ea = {STATE_GET: 5, ALL: 7};
const m = {deviceAddCustomCluster: (name, definition) => ({name, definition}), onOff: () => ({})};
`;
const {default: definition} = await import(`data:text/javascript;base64,${Buffer.from(substitutes + source).toString('base64')}`);
assert.deepEqual(definition.fingerprint.map((f) => f.modelID), ['SWPLUG1x', 'SMART_PLUG_BL0937', 'SMART_PLUG_HLW8032']);
assert.equal(definition.fingerprint[0].manufacturerName, 'Salu Workshop');
assert.equal(definition.vendor, 'Salu Workshop');
assert.equal(definition.model, 'SWPLUG1x');
assert.equal(definition.ota, true);
const cluster = definition.extend[0].definition;
assert.equal(cluster.name, 'smartPlugConfig');
assert.equal(cluster.attributes.telemetryIntervalMs.name, 'telemetryIntervalMs');
assert.equal(cluster.attributes.telemetryIntervalMs.write, true);
assert.equal(cluster.attributes.telemetryIntervalMs.type, 0x23);
const [electrical, metering, interval] = definition.fromZigbee;
assert.deepEqual(electrical.convert(null, {data: {rmsVoltage: 2300, rmsCurrent: 1000, activePower: 230, apparentPower: 230, powerFactor: 100}}),
    {voltage: 230, current: 1, power: 230, apparent_power: 230, power_factor: 1});
assert.deepEqual(electrical.convert(null, {data: {rmsVoltage: 65535, rmsCurrent: null, activePower: -32768, apparentPower: NaN, powerFactor: -128}}),
    {voltage: null, current: null, power: null, apparent_power: null, power_factor: null});
assert.deepEqual(metering.convert(null, {data: {currentSummDelivered: 1000000}}), {energy: 1});
assert.deepEqual(metering.convert(null, {data: {currentSummDelivered: [1, 2]}}), {energy: 4294.967298});
assert.deepEqual(metering.convert(null, {data: {currentSummDelivered: [65535, 4294967295]}}), {energy: null});
assert.deepEqual(interval.convert(null, {data: {0: 5000}}), {telemetry_interval_ms: 5000});
const setter = definition.toZigbee[0];
const calls = [];
const entity = {
    write: async (...args) => calls.push(['write', ...args]),
    read: async (...args) => { calls.push(['read', ...args]); return {telemetryIntervalMs: 5000}; },
};
assert.deepEqual(await setter.convertSet(entity, 'telemetry_interval_ms', 5000), {state: {telemetry_interval_ms: 5000}});
assert.equal(calls[0][0], 'write');
assert.deepEqual(calls[0][2], {telemetryIntervalMs: 5000});
for (const invalid of [99, 3600001, 100.5, '5000', null]) {
    await assert.rejects(setter.convertSet(entity, 'telemetry_interval_ms', invalid));
}
const writesBefore = calls.length;
assert.equal(writesBefore, 2);
await assert.rejects(setter.convertSet({...entity, read: async () => ({telemetryIntervalMs: null})}, 'telemetry_interval_ms', 5000));
console.log('PASS: converter fingerprints, writable uint32 interval, readback, bounds, units, invalid values, uint48 energy (transport not simulated)');
